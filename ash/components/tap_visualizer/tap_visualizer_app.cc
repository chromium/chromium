// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/tap_visualizer/tap_visualizer_app.h"

#include <utility>

#include "ash/components/tap_visualizer/tap_renderer.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace tap_visualizer {

TapVisualizerApp::TapVisualizerApp() = default;

TapVisualizerApp::~TapVisualizerApp() {
  display::Screen::GetScreen()->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveEventObserver(this);
}

void TapVisualizerApp::Start() {
  // Watches moves so the user can drag around a touch point.
  aura::Env* env = aura::Env::GetInstance();
  std::set<ui::EventType> types = {ui::ET_TOUCH_PRESSED, ui::ET_TOUCH_RELEASED,
                                   ui::ET_TOUCH_MOVED, ui::ET_TOUCH_CANCELLED};
  env->AddEventObserver(this, env, types);
  display::Screen::GetScreen()->AddObserver(this);
  for (const display::Display& display :
       display::Screen::GetScreen()->GetAllDisplays()) {
    CreateWidgetForDisplay(display.id());
  }
}

void TapVisualizerApp::OnStart() {
  views::AuraInit::InitParams params;
  params.connector = context()->connector();
  params.identity = context()->identity();
  params.register_path_provider = false;
  aura_init_ = views::AuraInit::Create(params);
  if (!aura_init_) {
    context()->QuitNow();
    return;
  }
  Start();
}

void TapVisualizerApp::OnEvent(const ui::Event& event) {
  if (!event.IsTouchEvent())
    return;

  // The event never targets this app, so the location is in screen coordinates.
  const gfx::Point screen_location = event.AsTouchEvent()->root_location();
  int64_t display_id = display::Screen::GetScreen()
                           ->GetDisplayNearestPoint(screen_location)
                           .id();
  auto it = display_id_to_renderer_.find(display_id);
  if (it != display_id_to_renderer_.end()) {
    TapRenderer* renderer = it->second.get();
    renderer->HandleTouchEvent(*event.AsTouchEvent());
  }
}

void TapVisualizerApp::OnDisplayAdded(const display::Display& new_display) {
  CreateWidgetForDisplay(new_display.id());
}

void TapVisualizerApp::OnDisplayRemoved(const display::Display& old_display) {
  // Deletes the renderer.
  display_id_to_renderer_.erase(old_display.id());
}

void TapVisualizerApp::CreateWidgetForDisplay(int64_t display_id) {
  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.accept_events = false;
  params.delegate = new views::WidgetDelegateView;
  params.mus_properties[ws::mojom::WindowManager::kContainerId_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(
          static_cast<int32_t>(ash::kShellWindowId_OverlayContainer));
  params.mus_properties[ws::mojom::WindowManager::kDisplayId_InitProperty] =
      mojo::ConvertTo<std::vector<uint8_t>>(display_id);
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.name = "TapVisualizer";
  widget->Init(params);
  widget->Show();

  display_id_to_renderer_[display_id] =
      std::make_unique<TapRenderer>(std::move(widget));
}

}  // namespace tap_visualizer
