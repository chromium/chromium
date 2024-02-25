// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_SHADOW_CONTROLLER_DELEGATE_H_
#define ASH_WM_WM_SHADOW_CONTROLLER_DELEGATE_H_

#include "ui/wm/core/shadow_controller_delegate.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// WmShadowControllerDelegate is a delegate for showing the shadow for window
// management purposes.
class WmShadowControllerDelegate : public wm::ShadowControllerDelegate {
 public:
  WmShadowControllerDelegate();
  WmShadowControllerDelegate(const WmShadowControllerDelegate&) = delete;
  WmShadowControllerDelegate& operator=(const WmShadowControllerDelegate&) =
      delete;
  ~WmShadowControllerDelegate() override;

  // wm::ShadowControllerDelegate:
  bool ShouldShowShadowForWindow(const aura::Window* window) override;
  bool ShouldUpdateShadowOnWindowPropertyChange(const aura::Window* window,
                                                const void* key,
                                                intptr_t old) override;
  void ApplyColorThemeToWindowShadow(aura::Window* window) override;
};

}  // namespace ash

#endif  // ASH_WM_WM_SHADOW_CONTROLLER_DELEGATE_H_
