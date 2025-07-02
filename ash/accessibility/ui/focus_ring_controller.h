// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_FOCUS_RING_CONTROLLER_H_
#define ASH_ACCESSIBILITY_UI_FOCUS_RING_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/ui/focus_ring_layer.h"
#include "ash/accessibility/ui/native_focus_watcher.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

// FocusRingController manages the focus ring around the focused view in
// OOBE when ForceKeyboardDrivenUINavigation is enabled. FocusRingController
// watches native focus changes and updates the focus ring layer when the
// focused widget or view within a widget changes.
//
// For managing general focus rings around the focus view, instead use the
// focus highlight provided by ash::A11yFeatureType::kFocusHighlight.
class ASH_EXPORT FocusRingController : public AccessibilityLayerDelegate,
                                       public NativeFocusObserver {
 public:
  FocusRingController();

  FocusRingController(const FocusRingController&) = delete;
  FocusRingController& operator=(const FocusRingController&) = delete;

  ~FocusRingController() override;

  // Turns on/off the focus ring.
  void SetVisible(bool visible);

  // NativeFocusObserver:
  void OnNativeFocusChanged(const gfx::Rect& bounds_in_screen) override;
  void OnNativeFocusCleared() override;

 private:
  // AccessibilityLayerDelegate.
  void OnDeviceScaleFactorChanged() override;

  void UpdateFocusRing();
  void ClearFocusRing();

  bool visible_ = false;

  std::unique_ptr<NativeFocusWatcher> native_focus_watcher_;
  gfx::Rect bounds_in_screen_;
  std::unique_ptr<FocusRingLayer> focus_ring_layer_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_FOCUS_RING_CONTROLLER_H_
