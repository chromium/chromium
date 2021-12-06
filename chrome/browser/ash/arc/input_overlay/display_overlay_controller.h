// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace arc {
// DisplayOverlayController manages the input mapping view, view and edit mode,
// menu, and educational dialog.
class DisplayOverlayController {
 public:
  explicit DisplayOverlayController(TouchInjector* touch_injector);
  DisplayOverlayController(const DisplayOverlayController&) = delete;
  DisplayOverlayController& operator=(const DisplayOverlayController&) = delete;
  ~DisplayOverlayController();

  void OnWindowBoundsChanged();

  // For test:
  gfx::Rect GetInputMappingViewBoundsForTesting();

 private:
  friend class DisplayOverlayControllerTest;

  // InputMappingView is the whole view of the input mappings.
  class InputMappingView;

  void AddOverlay();
  void RemoveOverlayIfAny();
  void AddInputMappingView();
  void RemoveInputMappingView();
  views::Widget* GetOverlayWidget();

  TouchInjector* touch_injector_;
  InputMappingView* input_mapping_view_ = nullptr;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
