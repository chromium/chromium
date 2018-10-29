// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/shortcut_viewer_application.h"

#include "ash/components/shortcut_viewer/last_window_closed_observer.h"
#include "ash/components/shortcut_viewer/views/keyboard_shortcut_view.h"
#include "ash/public/cpp/ash_client.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/views/mus/aura_init.h"

namespace keyboard_shortcut_viewer {

ShortcutViewerApplication::ShortcutViewerApplication()
    : shortcut_viewer_binding_(this) {
  registry_.AddInterface<shortcut_viewer::mojom::ShortcutViewer>(
      base::BindRepeating(&ShortcutViewerApplication::AddBinding,
                          base::Unretained(this)));
}

ShortcutViewerApplication::~ShortcutViewerApplication() = default;

// static
void ShortcutViewerApplication::RegisterForTraceEvents() {
  TRACE_EVENT0("shortcut_viewer", "ignored");
}

void ShortcutViewerApplication::OnStart() {
  views::AuraInit::InitParams params;
  params.connector = context()->connector();
  params.identity = context()->identity();
  params.register_path_provider = false;
  params.use_accessibility_host = true;
  aura_init_ = views::AuraInit::Create(params);
  if (!aura_init_) {
    context()->QuitNow();
    return;
  }

  // Register as a client of the window manager.
  ash::ash_client::Init();

  // Quit the application when the window is closed.
  last_window_closed_observer_ = std::make_unique<LastWindowClosedObserver>(
      context()->CreateQuitClosure());
}

void ShortcutViewerApplication::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void ShortcutViewerApplication::OnDeviceListsComplete() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  KeyboardShortcutView::Toggle(user_gesture_time_, nullptr);
}

void ShortcutViewerApplication::Toggle(base::TimeTicks user_gesture_time) {
  user_gesture_time_ = user_gesture_time;

  // This app needs InputDeviceManager information that loads asynchronously via
  // InputDeviceClient. If the device list is incomplete, wait for it to load.
  DCHECK(ui::InputDeviceManager::HasInstance());
  if (ui::InputDeviceManager::GetInstance()->AreDeviceListsComplete())
    KeyboardShortcutView::Toggle(user_gesture_time_, nullptr);
  else
    ui::InputDeviceManager::GetInstance()->AddObserver(this);
}

void ShortcutViewerApplication::AddBinding(
    shortcut_viewer::mojom::ShortcutViewerRequest request) {
  shortcut_viewer_binding_.Close();
  shortcut_viewer_binding_.Bind(std::move(request));
}

}  // namespace keyboard_shortcut_viewer
