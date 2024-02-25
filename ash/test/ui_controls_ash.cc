// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/functional/callback.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/class_property.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/display/screen.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ui_controls::UIControlsAura*)

namespace {

using ui_controls::MouseButton;
using ui_controls::UIControlsAura;

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(UIControlsAura, kUIControlsKey, NULL)

// Returns the UIControls object for RootWindow.
// kUIControlsKey is owned property and UIControls object
// will be deleted when the root window is deleted.
UIControlsAura* GetUIControlsForRootWindow(aura::Window* root_window) {
  UIControlsAura* native_ui_control = root_window->GetProperty(kUIControlsKey);
  if (!native_ui_control) {
    native_ui_control =
        aura::test::CreateUIControlsAura(root_window->GetHost());
    // Pass the ownership to the |root_window|.
    root_window->SetProperty(kUIControlsKey, native_ui_control);
  }
  return native_ui_control;
}

// Returns the UIControls object for the RootWindow at |point_in_screen|.
UIControlsAura* GetUIControlsAt(const gfx::Point& point_in_screen) {
  // TODO(mazda): Support the case passive grab is taken.
  return GetUIControlsForRootWindow(
      ash::window_util::GetRootWindowAt(point_in_screen));
}

class UIControlsAsh : public UIControlsAura {
 public:
  UIControlsAsh() = default;

  UIControlsAsh(const UIControlsAsh&) = delete;
  UIControlsAsh& operator=(const UIControlsAsh&) = delete;

  ~UIControlsAsh() override = default;

  // UIControslAura overrides:
  bool SendKeyEvents(gfx::NativeWindow window,
                     ui::KeyboardCode key,
                     int key_event_types,
                     int accelerator_state) override {
    return SendKeyEventsNotifyWhenDone(window, key, key_event_types,
                                       base::OnceClosure(), accelerator_state,
                                       ui_controls::KeyEventType::kKeyRelease);
  }

  bool SendKeyEventsNotifyWhenDone(
      gfx::NativeWindow window,
      ui::KeyboardCode key,
      int key_event_types,
      base::OnceClosure closure,
      int accelerator_state,
      ui_controls::KeyEventType wait_for) override {
    aura::Window* root = window ? window->GetRootWindow()
                                : ash::Shell::GetRootWindowForNewWindows();
    UIControlsAura* ui_controls = GetUIControlsForRootWindow(root);

    return ui_controls && ui_controls->SendKeyEventsNotifyWhenDone(
                              window, key, key_event_types, std::move(closure),
                              accelerator_state, wait_for);
  }

  bool SendMouseMove(int x, int y) override {
    gfx::Point p(x, y);
    UIControlsAura* ui_controls = GetUIControlsAt(p);
    return ui_controls && ui_controls->SendMouseMove(p.x(), p.y());
  }

  bool SendMouseMoveNotifyWhenDone(int x,
                                   int y,
                                   base::OnceClosure closure) override {
    gfx::Point p(x, y);
    UIControlsAura* ui_controls = GetUIControlsAt(p);
    return ui_controls && ui_controls->SendMouseMoveNotifyWhenDone(
                              p.x(), p.y(), std::move(closure));
  }

  bool SendMouseEvents(MouseButton type,
                       int button_state,
                       int accelerator_state) override {
    gfx::Point p(display::Screen::GetScreen()->GetCursorScreenPoint());
    UIControlsAura* ui_controls = GetUIControlsAt(p);
    return ui_controls &&
           ui_controls->SendMouseEvents(type, button_state, accelerator_state);
  }

  bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                     int button_state,
                                     base::OnceClosure closure,
                                     int accelerator_state) override {
    gfx::Point p(aura::Env::GetInstance()->last_mouse_location());
    UIControlsAura* ui_controls = GetUIControlsAt(p);
    return ui_controls &&
           ui_controls->SendMouseEventsNotifyWhenDone(
               type, button_state, std::move(closure), accelerator_state);
  }

  bool SendMouseClick(MouseButton type) override {
    gfx::Point p(display::Screen::GetScreen()->GetCursorScreenPoint());
    UIControlsAura* ui_controls = GetUIControlsAt(p);
    return ui_controls && ui_controls->SendMouseClick(type);
  }

  bool SendTouchEvents(int action, int id, int x, int y) override {
    UIControlsAura* ui_controls = GetUIControlsAt(gfx::Point(x, y));
    return ui_controls && ui_controls->SendTouchEvents(action, id, x, y);
  }

  bool SendTouchEventsNotifyWhenDone(int action,
                                     int id,
                                     int x,
                                     int y,
                                     base::OnceClosure task) override {
    UIControlsAura* ui_controls = GetUIControlsAt(gfx::Point(x, y));
    return ui_controls && ui_controls->SendTouchEventsNotifyWhenDone(
                              action, id, x, y, std::move(task));
  }
};

}  // namespace

namespace ash::test {

void EnableUIControlsAsh() {
  ui_controls::InstallUIControlsAura(new UIControlsAsh());
}

}  // namespace ash::test
