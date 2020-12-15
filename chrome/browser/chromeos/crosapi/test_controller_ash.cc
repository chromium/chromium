// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/test_controller_ash.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/crosapi/window_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_source.h"
#include "ui/gfx/geometry/point.h"

namespace crosapi {

namespace {

// Returns whether the dispatcher or target was destroyed.
bool DispatchMouseEvent(aura::Window* window, ui::EventType type) {
  const gfx::Point center = window->bounds().CenterPoint();
  ui::MouseEvent press(type, center, center, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  ui::EventDispatchDetails dispatch_details =
      window->GetHost()->GetEventSource()->SendEventToSink(&press);
  return dispatch_details.dispatcher_destroyed ||
         dispatch_details.target_destroyed;
}

}  // namespace

TestControllerAsh::TestControllerAsh() = default;
TestControllerAsh::~TestControllerAsh() = default;

void TestControllerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::TestController> receiver) {
// This interface is not available on production devices. It's only needed for
// tests that run on Linux-chrome so no reason to expose it.
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  LOG(ERROR) << "Ash does not support TestController on devices";
#else
  receivers_.Add(this, std::move(receiver));
#endif
}

void TestControllerAsh::DoesWindowExist(const std::string& window_id,
                                        DoesWindowExistCallback callback) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  std::move(callback).Run(window != nullptr);
}

void TestControllerAsh::ClickWindow(const std::string& window_id) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window)
    return;
  bool destroyed = DispatchMouseEvent(window, ui::ET_MOUSE_PRESSED);
  if (!destroyed) {
    DispatchMouseEvent(window, ui::ET_MOUSE_RELEASED);
  }
}

}  // namespace crosapi
