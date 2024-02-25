// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_SCOPED_A11Y_OVERRIDE_WINDOW_SETTER_H_
#define ASH_ACCESSIBILITY_SCOPED_A11Y_OVERRIDE_WINDOW_SETTER_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Scoped class that helps setting the window for accessibility focus for the
// duration of its own lifetime. Clears the accessibility focus window when
// destructed.
class ScopedA11yOverrideWindowSetter : public aura::WindowObserver {
 public:
  ScopedA11yOverrideWindowSetter() = default;
  ScopedA11yOverrideWindowSetter(const ScopedA11yOverrideWindowSetter&) =
      delete;
  ScopedA11yOverrideWindowSetter& operator=(
      const ScopedA11yOverrideWindowSetter&) = delete;
  ~ScopedA11yOverrideWindowSetter() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Updates the a11y focus window if `current_a11y_override_window_` is not
  // equal to `a11y_override_window`. This will make sure the accessibility
  // features can always get the correct a11y override window to focus before
  // getting the window with actual focus.
  void MaybeUpdateA11yOverrideWindow(aura::Window* a11y_override_window);

 private:
  // Caches the value of the a11y override window. It will be updated when a
  // different window should get focus from the accessibility features.
  raw_ptr<aura::Window, DanglingUntriaged> current_a11y_override_window_ =
      nullptr;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_SCOPED_A11Y_OVERRIDE_WINDOW_SETTER_H_
