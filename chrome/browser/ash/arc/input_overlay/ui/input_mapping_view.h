// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MAPPING_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MAPPING_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "ui/views/view.h"

namespace arc {
namespace input_overlay {

class DisplayOverlayController;
// InputMappingView shows all the input mappings.
class InputMappingView : public views::View {
 public:
  explicit InputMappingView(
      DisplayOverlayController* display_overlay_controller);
  InputMappingView(const InputMappingView&) = delete;
  InputMappingView& operator=(const InputMappingView&) = delete;
  ~InputMappingView() override;

  void SetDisplayMode(const DisplayMode mode);

 private:
  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
  DisplayMode current_display_mode_ = DisplayMode::kNone;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_INPUT_MAPPING_VIEW_H_
