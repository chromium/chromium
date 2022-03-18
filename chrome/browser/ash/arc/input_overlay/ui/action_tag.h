// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_TAG_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_TAG_H_

#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_label.h"
#include "ui/views/view.h"

namespace arc {
namespace input_overlay {

// ActionTag is used to showing the mapping hint for each action. It can show
// text hint or image hint.
class ActionTag : public views::View {
 public:
  ActionTag();
  ~ActionTag() override;

  static std::unique_ptr<ActionTag> CreateTextActionTag(std::string text);
  static std::unique_ptr<ActionTag> CreateImageActionTag(
      std::string mouse_action);

  void SetDisplayMode(DisplayMode mode);
  void OnActionLabelFocused();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  class ActionImage;

  void InitTextTag();
  void InitImageTag();

  ActionImage* image_ = nullptr;
  ActionLabel* label_ = nullptr;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_TAG_H_
