// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_controller.h"
#include "base/memory/raw_ptr.h"

#include <utility>

#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_util.h"
#include "ash/display/root_window_transformers.h"
#include "ash/display/screen_position_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/root_window_transformer.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_transform.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/native_widget_types.h"

namespace ash {
namespace {

// ScreenPositionClient for mirroring windows.
class MirroringScreenPositionClient
    : public aura::client::ScreenPositionClient {
 public:
  explicit MirroringScreenPositionClient(MirrorWindowController* controller)
      : controller_(controller) {}

  MirroringScreenPositionClient(const MirroringScreenPositionClient&) = delete;
  MirroringScreenPositionClient& operator=(
      const MirroringScreenPositionClient&) = delete;

  // aura::client::ScreenPositionClient:
  void ConvertPointToScreen(const aura::Window* window,
                            gfx::PointF* point) override {
    const aura::Window* root = window->GetRootWindow();
    aura::Window::ConvertPointToTarget(window, root, point);
    const display::Display& display =
        controller_->GetDisplayForRootWindow(root);
    const gfx::Point display_origin = display.bounds().origin();
    point->Offset(display_origin.x(), display_origin.y());
  }

  void ConvertPointFromScreen(const aura::Window* window,
                              gfx::PointF* point) override {
    const aura::Window* root = window->GetRootWindow();
    const display::Display& display =
        controller_->GetDisplayForRootWindow(root);
    const gfx::Point display_origin = display.bounds().origin();
    point->Offset(-display_origin.x(), -display_origin.y());
    aura::Window::ConvertPointToTarget(root, window, point);
  }

  void ConvertHostPointToScreen(aura::Window* root_window,
                                gfx::Point* point) override {
    aura::Window* not_used;
    ScreenPositionController::ConvertHostPointToRelativeToRootWindow(
        root_window, controller_->GetAllRootWindows(), point, &not_used);
    aura::client::ScreenPositionClient::ConvertPointToScreen(root_window,
                                                             point);
  }

  void SetBounds(aura::Window* window,
                 const gfx::Rect& bounds,
                 const display::Display& display) override {
    NOTREACHED();
  }

 protected:
  // aura::client::ScreenPositionClient:
  gfx::Point GetRootWindowOriginInScreen(
      const aura::Window* root_window) override {
    DCHECK(root_window->IsRootWindow());
    const display::Display& display =
        controller_->GetDisplayForRootWindow(root_window);
    return display.bounds().origin();
  }

 private:
  raw_ptr<MirrorWindowController> controller_;  // not owned.
};

// A trivial CaptureClient that does nothing. That is, calls to set/release
// capture are dropped.
class NoneCaptureClient : public aura::client::CaptureClient {
 public:
  NoneCaptureClient() = default;

  NoneCaptureClient(const NoneCaptureClient&) = delete;
  NoneCaptureClient& operator=(const NoneCaptureClient&) = delete;

  ~NoneCaptureClient() override = default;

 private:
  // aura::client::CaptureClient:
  void SetCapture(aura::Window* window) override {}
  void ReleaseCapture(aura::Window* window) override {}
  aura::Window* GetCaptureWindow() override { return nullptr; }
  aura::Window* GetGlobalCaptureWindow() override { return nullptr; }
  void AddObserver(aura::client::CaptureClientObserver* observer) override {}
  void RemoveObserver(aura::client::CaptureClientObserver* observer) override {}
};

display::DisplayManager::MultiDisplayMode GetCurrentMultiDisplayMode() {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  return display_manager->IsInUnifiedMode()
             ? display::DisplayManager::UNIFIED
             : (display_manager->IsInSoftwareMirrorMode()
                    ? display::DisplayManager::MIRRORING
                    : display::DisplayManager::EXTENDED);
}

int64_t GetCurrentReflectingSourceId() {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  if (display_manager->IsInUnifiedMode())
    return display::Screen::GetScreen()->GetPrimaryDisplay().id();
  if (display_manager->IsInSoftwareMirrorMode())
    return display_manager->mirroring_source_id();
  return display::kInvalidDisplayId;
}

}  // namespace

struct MirrorWindowController::MirroringHostInfo {
  MirroringHostInfo();
  ~MirroringHostInfo();
  std::unique_ptr<AshWindowTreeHost> ash_host;
  gfx::Size mirror_window_host_size;
  raw_ptr<aura::Window> mirror_window = nullptr;
};

MirrorWindowController::MirroringHostInfo::MirroringHostInfo() = default;
MirrorWindowController::MirroringHostInfo::~MirroringHostInfo() = default;

MirrorWindowController::MirrorWindowController()
    : current_event_targeter_src_host_(nullptr),
      multi_display_mode_(display::DisplayManager::EXTENDED),
      screen_position_client_(new MirroringScreenPositionClient(this)) {}

MirrorWindowController::~MirrorWindowController() {
  // Make sure the root window gets deleted before cursor_window_delegate.
  Close(false);
}

void MirrorWindowController::UpdateWindow(
    const std::vector<display::ManagedDisplayInfo>& display_info_list) {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  DCHECK(display_manager->IsInSoftwareMirrorMode() ||
         display_manager->IsInUnifiedMode());
  static int mirror_host_count = 0;

  multi_display_mode_ = GetCurrentMultiDisplayMode();
  reflecting_source_id_ = GetCurrentReflectingSourceId();
  viz::SurfaceId reflecting_surface_id =
      Shell::GetRootWindowForDisplayId(reflecting_source_id_)->GetSurfaceId();

  for (const display::ManagedDisplayInfo& display_info : display_info_list) {
    std::unique_ptr<RootWindowTransformer> transformer;
    if (display_manager->IsInSoftwareMirrorMode()) {
      transformer = CreateRootWindowTransformerForMirroredDisplay(
          display_manager->GetDisplayInfo(reflecting_source_id_), display_info);
    } else {
      DCHECK(display_manager->IsInUnifiedMode());
      display::Display display =
          display_manager->GetMirroringDisplayById(display_info.id());
      transformer = CreateRootWindowTransformerForUnifiedDesktop(
          display::Screen::GetScreen()->GetPrimaryDisplay().bounds(), display);
    }

    if (!base::Contains(mirroring_host_info_map_, display_info.id())) {
      AshWindowTreeHostInitParams init_params;
      init_params.initial_bounds = display_info.bounds_in_native();
      init_params.display_id = display_info.id();
      init_params.delegate = this;
      init_params.mirroring_unified = display_manager->IsInUnifiedMode();
      init_params.device_scale_factor = display_info.device_scale_factor();
      MirroringHostInfo* host_info = new MirroringHostInfo;
      host_info->ash_host = AshWindowTreeHost::Create(init_params);
      mirroring_host_info_map_[display_info.id()] = host_info;

      aura::WindowTreeHost* host = host_info->ash_host->AsWindowTreeHost();
      DCHECK(!host->has_input_method());
      host->SetSharedInputMethod(
          Shell::Get()->window_tree_host_manager()->input_method());
      host->window()->SetName(
          base::StringPrintf("MirrorRootWindow-%d", mirror_host_count++));
      host->compositor()->SetBackgroundColor(SK_ColorBLACK);
      // No need to remove the observer because the WindowTreeHostManager
      // outlives the host.
      host->AddObserver(Shell::Get()->window_tree_host_manager());
      host->AddObserver(this);
      // TODO(oshima): TouchHUD is using idkey.
      InitRootWindowSettings(host->window())->display_id = display_info.id();
      host->InitHost();
      host->window()->Show();

      if (display_manager->IsInUnifiedMode()) {
        host_info->ash_host->ConfineCursorToRootWindow();
        AshWindowTreeHost* unified_ash_host =
            Shell::Get()
                ->window_tree_host_manager()
                ->GetAshWindowTreeHostForDisplayId(reflecting_source_id_);
        unified_ash_host->RegisterMirroringHost(host_info->ash_host.get());
        aura::client::SetScreenPositionClient(host->window(),
                                              screen_position_client_.get());
      }

      aura::client::SetCaptureClient(host->window(), new NoneCaptureClient());
      host->Show();

      aura::Window* mirror_window = host_info->mirror_window =
          new aura::Window(nullptr);
      mirror_window->Init(ui::LAYER_SOLID_COLOR);
      host->window()->AddChild(mirror_window);
      host_info->ash_host->SetRootWindowTransformer(std::move(transformer));

      const display::Display::Rotation effective_rotation =
          display_info.GetLogicalActiveRotation();
      host->SetDisplayTransformHint(
          display::DisplayRotationToOverlayTransform(effective_rotation));

      // The accelerated widget is created synchronously.
      DCHECK_NE(gfx::kNullAcceleratedWidget, host->GetAcceleratedWidget());
    } else {
      AshWindowTreeHost* ash_host =
          mirroring_host_info_map_[display_info.id()]->ash_host.get();
      aura::WindowTreeHost* host = ash_host->AsWindowTreeHost();
      GetRootWindowSettings(host->window())->display_id = display_info.id();
      ash_host->SetRootWindowTransformer(std::move(transformer));
      host->SetBoundsInPixels(display_info.bounds_in_native());

      // TODO(oshima): Consolidate the code above.
      const display::Display::Rotation effective_rotation =
          display_info.GetLogicalActiveRotation();
      host->SetDisplayTransformHint(
          display::DisplayRotationToOverlayTransform(effective_rotation));
    }

    // |mirror_size| is the size of the compositor of the mirror source in
    // physical pixels. The RootWindowTransformer corrects the scale of the
    // mirrored display and the location of input events.
    ui::Compositor* source_compositor =
        Shell::GetRootWindowForDisplayId(reflecting_source_id_)
            ->GetHost()
            ->compositor();
    gfx::Size mirror_size = source_compositor->size();

    auto* mirroring_host_info =
        mirroring_host_info_map_[display_info.id()].get();

    const bool should_undo_rotation = ShouldUndoRotationForMirror();

    if (!should_undo_rotation && !display_manager->IsInUnifiedMode()) {
      // Use the rotation from source display without panel orientation
      // applied instead of the display transform hint in |source_compositor|
      // so that panel orientation is not applied to the mirror host.
      mirroring_host_info->ash_host->AsWindowTreeHost()
          ->SetDisplayTransformHint(display::DisplayRotationToOverlayTransform(
              display_manager->GetDisplayInfo(reflecting_source_id_)
                  .GetActiveRotation()));
    }

    aura::Window* mirror_window = mirroring_host_info->mirror_window;
    mirror_window->SetBounds(gfx::Rect(mirror_size));
    mirror_window->Show();
    mirror_window->layer()->SetShowReflectedSurface(reflecting_surface_id,
                                                    mirror_size);
  }

  // Deleting WTHs for disconnected displays.
  if (mirroring_host_info_map_.size() > display_info_list.size()) {
    for (MirroringHostInfoMap::iterator iter = mirroring_host_info_map_.begin();
         iter != mirroring_host_info_map_.end();) {
      if (!base::Contains(display_info_list, iter->first,
                          &display::ManagedDisplayInfo::id)) {
        CloseAndDeleteHost(iter->second, true);
        iter = mirroring_host_info_map_.erase(iter);
      } else {
        ++iter;
      }
    }
  }

}

void MirrorWindowController::UpdateWindow() {
  if (mirroring_host_info_map_.empty())
    return;
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::Screen* screen = display::Screen::GetScreen();

  std::vector<display::ManagedDisplayInfo> display_info_list;
  // Prune the window on the removed displays.
  for (auto& pair : mirroring_host_info_map_) {
    MirroringHostInfo* info = pair.second;
    if (screen
            ->GetDisplayNearestWindow(
                info->ash_host->AsWindowTreeHost()->window())
            .is_valid()) {
      display_info_list.push_back(display_manager->GetDisplayInfo(pair.first));
    }
  }
  UpdateWindow(display_info_list);
}

void MirrorWindowController::CloseIfNotNecessary() {
  display::DisplayManager::MultiDisplayMode new_mode =
      GetCurrentMultiDisplayMode();
  int64_t new_reflecting_source_id = GetCurrentReflectingSourceId();
  if (multi_display_mode_ != new_mode ||
      reflecting_source_id_ != new_reflecting_source_id) {
    Close(true);
  } else {
    UpdateWindow();
  }
}

void MirrorWindowController::Close(bool delay_host_deletion) {
  for (auto& info : mirroring_host_info_map_)
    CloseAndDeleteHost(info.second, delay_host_deletion);
  mirroring_host_info_map_.clear();
}

void MirrorWindowController::OnHostResized(aura::WindowTreeHost* host) {
  for (auto& pair : mirroring_host_info_map_) {
    MirroringHostInfo* info = pair.second;
    if (info->ash_host->AsWindowTreeHost() == host) {
      if (info->mirror_window_host_size == host->GetBoundsInPixels().size())
        return;
      info->mirror_window_host_size = host->GetBoundsInPixels().size();
      // No need to update the transformer as new transformer is already set
      // in UpdateWindow.
      Shell::Get()
          ->window_tree_host_manager()
          ->cursor_window_controller()
          ->UpdateLocation();
      return;
    }
  }
}

display::Display MirrorWindowController::GetDisplayForRootWindow(
    const aura::Window* root) const {
  for (const auto& pair : mirroring_host_info_map_) {
    if (pair.second->ash_host->AsWindowTreeHost()->window() == root) {
      // Sanity check to catch an error early.
      const int64_t id = pair.first;
      const display::Display* display = GetDisplayById(id);
      DCHECK(display);
      if (display)
        return *display;
    }
  }
  return display::Display();
}

AshWindowTreeHost* MirrorWindowController::GetAshWindowTreeHostForDisplayId(
    int64_t id) {
  if (mirroring_host_info_map_.count(id) == 0)
    return nullptr;
  return mirroring_host_info_map_[id]->ash_host.get();
}

aura::Window::Windows MirrorWindowController::GetAllRootWindows() const {
  aura::Window::Windows root_windows;
  for (const auto& pair : mirroring_host_info_map_)
    root_windows.push_back(pair.second->ash_host->AsWindowTreeHost()->window());
  return root_windows;
}

const display::Display* MirrorWindowController::GetDisplayById(
    int64_t display_id) const {
  const display::Displays& list =
      Shell::Get()->display_manager()->software_mirroring_display_list();
  for (const auto& display : list) {
    if (display.id() == display_id)
      return &display;
  }

  return nullptr;
}

void MirrorWindowController::SetCurrentEventTargeterSourceHost(
    aura::WindowTreeHost* targeter_src_host) {
  current_event_targeter_src_host_ = targeter_src_host;
}

void MirrorWindowController::CloseAndDeleteHost(MirroringHostInfo* host_info,
                                                bool delay_host_deletion) {
  aura::WindowTreeHost* host = host_info->ash_host->AsWindowTreeHost();

  aura::client::SetScreenPositionClient(host->window(), nullptr);

  NoneCaptureClient* capture_client = static_cast<NoneCaptureClient*>(
      aura::client::GetCaptureClient(host->window()));
  aura::client::SetCaptureClient(host->window(), nullptr);
  delete capture_client;

  host->RemoveObserver(Shell::Get()->window_tree_host_manager());
  host->RemoveObserver(this);
  host_info->ash_host->PrepareForShutdown();

  // EventProcessor may be accessed after this call if the mirroring window
  // was deleted as a result of input event (e.g. shortcut), so don't delete
  // now.
  if (delay_host_deletion)
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                  host_info);
  else
    delete host_info;
}

}  // namespace ash
