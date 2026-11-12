// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_DECORATION_CONTROLLER_DELEGATE_H_
#define ASH_WM_WM_DECORATION_CONTROLLER_DELEGATE_H_

#include "ui/wm/core/decoration_controller_delegate.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// WmDecorationControllerDelegate is a delegate for showing the shadow and
// highlight border for window management purposes.
class WmDecorationControllerDelegate : public wm::DecorationControllerDelegate {
 public:
  WmDecorationControllerDelegate();
  WmDecorationControllerDelegate(const WmDecorationControllerDelegate&) =
      delete;
  WmDecorationControllerDelegate& operator=(
      const WmDecorationControllerDelegate&) = delete;
  ~WmDecorationControllerDelegate() override;

  // wm::DecorationControllerDelegate:
  bool ShouldObserveWindow(const aura::Window* window) override;
  bool ShouldShowShadowForWindow(const aura::Window* window) override;
  bool ShouldShowHighlightBorderForWindow(const aura::Window* window) override;
  bool ShouldUpdateDecorationOnWindowPropertyChange(const aura::Window* window,
                                                    const void* key,
                                                    intptr_t old) override;
  void ApplyColorThemeToWindowDecoration(aura::Window* window) override;
  bool ShouldRoundDecorationForWindow(const aura::Window* window) override;
};

}  // namespace ash

#endif  // ASH_WM_WM_DECORATION_CONTROLLER_DELEGATE_H_
