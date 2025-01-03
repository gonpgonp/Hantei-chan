#include "main_frame.h"

#include "main.h"
#include "filedialog.h"
#include "ini.h"
#include "imgui_utils.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <windows.h>

#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>

MainFrame::MainFrame(ContextGl *context_):
context(context_),
currState{},
mainPane(&render, &framedata, currState),
rightPane(&render, &framedata, currState),
boxPane(&render, &framedata, currState),
curPalette(0),
x(0),y(150)
{
	currState.spriteId = -1;
	render.SetCg(&cg);
	LoadSettings();
}

MainFrame::~MainFrame()
{
	ImGui::SaveIniSettingsToDisk(ImGui::GetCurrentContext()->IO.IniFilename);
}

void MainFrame::LoadSettings()
{
	LoadTheme(gSettings.theme);
	SetZoom(gSettings.zoomLevel);
	smoothRender = gSettings.bilinear;
	memcpy(clearColor, gSettings.color, sizeof(float)*3);
}

void MainFrame::Draw()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	DrawUi();
	DrawBack();
	ImGui::Render();
	
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	SwapBuffers(context->dc);

	gSettings.theme = style_idx;
	gSettings.zoomLevel = zoom_idx;
	gSettings.bilinear = smoothRender;
	memcpy(gSettings.color, clearColor, sizeof(float)*3);
}

void MainFrame::DrawBack()
{
	render.filter = smoothRender;
	glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.f);
	glClear(GL_COLOR_BUFFER_BIT |  GL_DEPTH_BUFFER_BIT);
	render.x = (x+clientRect.x/2)/render.scale;
	render.y = (y+clientRect.y/2)/render.scale;
	render.Draw();
}

void MainFrame::DrawUi()
{
	ImGuiID errorPopupId = ImGui::GetID("Loading Error");
	

	//Fullscreen docker to provide the layout for the panes
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Dock Window", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoBackground 
	);
		ImGui::PopStyleVar(3);
		Menu(errorPopupId);
		
		ImGuiID dockspaceID = ImGui::GetID("Dock Space");
		if (!ImGui::DockBuilderGetNode(dockspaceID)) {
			ImGui::DockBuilderRemoveNode(dockspaceID);
			ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspaceID, clientRect); 

			ImGuiID toSplit = dockspaceID;
			ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Left, 0.30f, nullptr, &toSplit);
			ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Right, 0.45f, nullptr, &toSplit);
			ImGuiID dock_down_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Down, 0.20f, nullptr, &toSplit);

			ImGui::DockBuilderDockWindow("Left Pane", dock_left_id);
			ImGui::DockBuilderDockWindow("Right Pane", dock_right_id);
 			ImGui::DockBuilderDockWindow("Box Pane", dock_down_id);

			ImGui::DockBuilderFinish(dockspaceID);
		}
		ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f),
			ImGuiDockNodeFlags_PassthruCentralNode |
			ImGuiDockNodeFlags_NoDockingInCentralNode |
			ImGuiDockNodeFlags_AutoHideTabBar 
			//ImGuiDockNodeFlags_NoSplit
		); 
	ImGui::End();

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Loading Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("There was a problem loading the file.\n"
			"The file couldn't be accessed or it's not a valid file.\n\n");
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}

	mainPane.Draw(); 
	rightPane.Draw();
	boxPane.Draw();
	aboutWindow.Draw();
	vectors.Draw();

	RenderUpdate();
}

void MainFrame::RenderUpdate()
{
	Sequence *seq;
	if((seq = framedata.get_sequence(currState.pattern)) &&
		seq->frames.size() > 0)
	{
		auto &frame =  seq->frames[currState.frame];
		currState.spriteId = frame.AF.spriteId;
		render.GenerateHitboxVertices(frame.hitboxes);
		render.offsetX = (frame.AF.offset_x)*1;
		render.offsetY = (frame.AF.offset_y)*1;
		render.SetImageColor(frame.AF.rgba);
		render.rotX = frame.AF.rotation[0];
		render.rotY = frame.AF.rotation[1];
		render.rotZ = frame.AF.rotation[2];
		render.scaleX = frame.AF.scale[0];
		render.scaleY = frame.AF.scale[1];
		
		switch (frame.AF.blend_mode)
		{
		case 2:
			render.blendingMode = Render::additive;
			break;
		case 3:
			render.blendingMode = Render::subtractive;
			break;
		default:
			render.blendingMode = Render::normal;
			break;
		}
	}
	else
	{
		currState.spriteId = -1;
		
		render.DontDraw();
	}
	render.SwitchImage(currState.spriteId);
}

void MainFrame::AdvancePattern(int dir)
{
	currState.pattern+= dir;
	if(currState.pattern < 0)
		currState.pattern = 0;
	else if(currState.pattern >= framedata.get_sequence_count())
		currState.pattern = framedata.get_sequence_count()-1;
	currState.frame = 0;
}

void MainFrame::AdvanceFrame(int dir)
{
	auto seq = framedata.get_sequence(currState.pattern);
	currState.frame += dir;
	if(currState.frame < 0)
		currState.frame = 0;
	else if(seq && currState.frame >= seq->frames.size())
		currState.frame = seq->frames.size()-1;
}

void MainFrame::UpdateBackProj(float x, float y)
{
	render.UpdateProj(x, y);
	glViewport(0, 0, x, y);
}

void MainFrame::HandleMouseDrag(int x_, int y_, bool dragRight, bool dragLeft)
{
	if(dragRight)
	{
		boxPane.BoxDrag(x_, y_);
	}
	else if(dragLeft)
	{
		x += x_;
		y += y_;
	}
}

void MainFrame::RightClick(int x_, int y_)
{
	boxPane.BoxStart((x_-x-clientRect.x/2)/render.scale, (y_-y-clientRect.y/2)/render.scale);
}

bool MainFrame::HandleKeys(uint64_t vkey)
{
	switch (vkey)
	{
	case VK_UP:
		AdvancePattern(-1);
		return true;
	case VK_DOWN:
		AdvancePattern(1);
		return true;
	case VK_LEFT:
		AdvanceFrame(-1);
		return true;
	case VK_RIGHT:
		AdvanceFrame(+1);
		return true;
	case 'Z':
		boxPane.AdvanceBox(-1);
		return true;
	case 'X':
		boxPane.AdvanceBox(+1);
		return true;
	}
	return false;
}

void MainFrame::ChangeClearColor(float r, float g, float b)
{
	clearColor[0] = r;
	clearColor[1] = g;
	clearColor[2] = b;
}

void MainFrame::SetZoom(float level)
{
	render.scale = level;
	zoom_idx = level;
}

void MainFrame::LoadTheme(int i )
{
	style_idx = i;
	switch (i)
	{
		case 0: WarmStyle(); break;
		case 1: ImGui::StyleColorsDark(); ChangeClearColor(0.202f, 0.243f, 0.293f); break;
		case 2: ImGui::StyleColorsLight(); ChangeClearColor(0.534f, 0.568f, 0.587f); break;
		case 3: ImGui::StyleColorsClassic(); ChangeClearColor(0.142f, 0.075f, 0.147f); break;
	}
}

void MainFrame::Menu(unsigned int errorPopupId)
{
	if (ImGui::BeginMenuBar())
	{
		//ImGui::Separator();
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New file"))
			{
				framedata.initEmpty();
				currentFilePath.clear();
				mainPane.RegenerateNames();
			}
			if (ImGui::MenuItem("Close file"))
			{
				framedata.Free();
				currentFilePath.clear();
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Load from .txt..."))
			{
				std::string &&file = FileDialog(fileType::TXT);
				if(!file.empty())
				{
					if(!LoadFromIni(&framedata, &cg, file))
					{
						ImGui::OpenPopup(errorPopupId);
					}
					else
					{
						currentFilePath.clear();
						mainPane.RegenerateNames();
						render.SwitchImage(-1);
					}
				}
			}

			if (ImGui::MenuItem("Load HA6..."))
			{
				std::string &&file = FileDialog(fileType::HA6);
				if(!file.empty())
				{
					if(!framedata.load(file.c_str()))
					{
						ImGui::OpenPopup(errorPopupId);
					}
					else
					{
						currentFilePath = file;
						mainPane.RegenerateNames();
					}
				}
			}

			if (ImGui::MenuItem("Load HA6 and Patch..."))
			{
				std::string &&file = FileDialog(fileType::HA6);
				if(!file.empty())
				{
					if(!framedata.load(file.c_str(), true))
					{
						ImGui::OpenPopup(errorPopupId);
					}
					else
						mainPane.RegenerateNames();
					currentFilePath.clear();
				}
			}

			ImGui::Separator();
			//TODO: Implement hotkeys, someday.
			if (ImGui::MenuItem("Save"/* , "Ctrl+S" */)) 
			{
				if(currentFilePath.empty())
					currentFilePath = FileDialog(fileType::HA6, true);
				if(!currentFilePath.empty())
				{
					framedata.save(currentFilePath.c_str());
				}
			}

			if (ImGui::MenuItem("Save as...")) 
			{
				std::string &&file = FileDialog(fileType::HA6, true);
				if(!file.empty())
				{
					framedata.save(file.c_str());
					currentFilePath = file;
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Load CG...")) 
			{
				std::string &&file = FileDialog(fileType::CG);
				if(!file.empty())
				{
					if(!cg.load(file.c_str()))
					{
						ImGui::OpenPopup(errorPopupId);	
					}
					render.SwitchImage(-1);
				}
			}

			if (ImGui::MenuItem("Load palette...")) 
			{
				std::string &&file = FileDialog(fileType::PAL);
				if(!file.empty())
				{
					if(!cg.loadPalette(file.c_str()))
					{
						ImGui::OpenPopup(errorPopupId);	
					}
					render.SwitchImage(-1);
				}
			}

			if (ImGui::MenuItem("Load vector.txt..."))
			{
				std::string&& file = FileDialog(fileType::VECTOR);
				if (!file.empty())
				{
					if (!vectors.load(file.c_str()))
					{
						ImGui::OpenPopup(errorPopupId);
					}
					render.SwitchImage(-1);
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) PostQuitMessage(0);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Preferences"))
		{
			if (ImGui::BeginMenu("Switch preset style"))
			{		
				if (ImGui::Combo("Style", &style_idx, "Warm\0Dark\0Light\0ImGui\0"))
				{
					LoadTheme(style_idx);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Background color"))
			{
				ImGui::ColorEdit3("##clearColor", (float*)&clearColor, ImGuiColorEditFlags_NoInputs);
				ImGui::EndMenu();
			}
			if (cg.getPalNumber() > 0 && ImGui::BeginMenu("Palette number"))
			{
				ImGui::SetNextItemWidth(80);
				ImGui::InputInt("Palette", &curPalette);
				if(curPalette >= cg.getPalNumber())
					curPalette = cg.getPalNumber()-1;
				else if(curPalette < 0)
					curPalette = 0;
				if(cg.changePaletteNumber(curPalette))
					render.SwitchImage(-1);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Zoom level"))
			{
				ImGui::SetNextItemWidth(80);
				if (ImGui::SliderFloat("Zoom", &zoom_idx, 0.25f, 20.0f, "%.2f"))
				{
					SetZoom(zoom_idx);
				}
				ImGui::SameLine();
				ImGui::TextDisabled("(?)");
				if (ImGui::IsItemHovered())
					Tooltip("Ctrl + Click to set exact value");
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Filter"))
			{
				if (ImGui::Checkbox("Bilinear", &smoothRender))
				{
					render.filter = smoothRender;
					render.SwitchImage(-1);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Windows"))
		{
			if (ImGui::MenuItem("Vectors Guide")) vectors.drawWindow = !vectors.drawWindow;
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About")) aboutWindow.isVisible = !aboutWindow.isVisible;
			ImGui::EndMenu();
		}
		
		ImGui::EndMenuBar();
	}
}

void MainFrame::WarmStyle()
{
	ImVec4* colors = ImGui::GetStyle().Colors;

	colors[ImGuiCol_Text]                   = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_WindowBg]               = ImVec4(0.95f, 0.91f, 0.85f, 1.00f);
	colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg]                = ImVec4(0.98f, 0.96f, 0.93f, 1.00f);
	colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
	colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg]                = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.95f, 1.00f, 0.62f, 1.00f);
	colors[ImGuiCol_FrameBgActive]          = ImVec4(0.98f, 1.00f, 0.81f, 1.00f);
	colors[ImGuiCol_TitleBg]                = ImVec4(0.82f, 0.73f, 0.64f, 0.81f);
	colors[ImGuiCol_TitleBgActive]          = ImVec4(0.97f, 0.65f, 0.00f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg]              = ImVec4(0.89f, 0.83f, 0.76f, 1.00f);
	colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.89f, 0.83f, 0.76f, 1.00f);
	colors[ImGuiCol_ScrollbarGrab]          = ImVec4(1.00f, 0.96f, 0.87f, 0.99f);
	colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(1.00f, 0.98f, 0.94f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.72f, 0.66f, 0.48f, 0.99f);
	colors[ImGuiCol_CheckMark]              = ImVec4(1.00f, 0.52f, 0.00f, 1.00f);
	colors[ImGuiCol_SliderGrab]             = ImVec4(1.00f, 0.52f, 0.00f, 1.00f);
	colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.55f, 0.53f, 0.32f, 1.00f);
	colors[ImGuiCol_Button]                 = ImVec4(0.74f, 1.00f, 0.53f, 0.25f);
	colors[ImGuiCol_ButtonHovered]          = ImVec4(1.00f, 0.77f, 0.41f, 0.96f);
	colors[ImGuiCol_ButtonActive]           = ImVec4(1.00f, 0.47f, 0.00f, 1.00f);
	colors[ImGuiCol_Header]                 = ImVec4(0.74f, 0.57f, 0.33f, 0.31f);
	colors[ImGuiCol_HeaderHovered]          = ImVec4(0.94f, 0.75f, 0.36f, 0.42f);
	colors[ImGuiCol_HeaderActive]           = ImVec4(1.00f, 0.75f, 0.01f, 0.61f);
	colors[ImGuiCol_Separator]              = ImVec4(0.38f, 0.34f, 0.25f, 0.66f);
	colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.76f, 0.70f, 0.59f, 0.98f);
	colors[ImGuiCol_SeparatorActive]        = ImVec4(0.32f, 0.32f, 0.32f, 0.45f);
	colors[ImGuiCol_ResizeGrip]             = ImVec4(0.35f, 0.35f, 0.35f, 0.17f);
	colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.41f, 0.80f, 1.00f, 0.84f);
	colors[ImGuiCol_ResizeGripActive]       = ImVec4(1.00f, 0.61f, 0.23f, 1.00f);
	colors[ImGuiCol_Tab]                    = ImVec4(0.79f, 0.74f, 0.64f, 0.00f);
	colors[ImGuiCol_TabHovered]             = ImVec4(1.00f, 0.64f, 0.06f, 0.85f);
	colors[ImGuiCol_TabActive]              = ImVec4(0.69f, 0.40f, 0.12f, 0.31f);
	colors[ImGuiCol_TabUnfocused]           = ImVec4(0.93f, 0.92f, 0.92f, 0.98f);
	colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.91f, 0.87f, 0.74f, 1.00f);
	colors[ImGuiCol_DockingPreview]         = ImVec4(0.26f, 0.98f, 0.35f, 0.22f);
	colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_PlotLines]              = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.58f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.40f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.45f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.69f, 0.53f, 0.32f, 0.30f);
	colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.69f, 0.58f, 0.44f, 1.00f);
	colors[ImGuiCol_TableBorderLight]       = ImVec4(0.70f, 0.62f, 0.42f, 0.40f);
	colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.30f, 0.30f, 0.30f, 0.09f);
	colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.98f, 0.89f, 0.35f);
	colors[ImGuiCol_DragDropTarget]         = ImVec4(0.26f, 0.98f, 0.94f, 0.95f);
	colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.97f, 0.98f, 0.80f);
	colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(0.70f, 0.70f, 0.70f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
	ChangeClearColor(0.324f, 0.409f, 0.185f);
}
