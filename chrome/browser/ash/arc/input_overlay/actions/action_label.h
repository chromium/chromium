// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_LABEL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_LABEL_H_

#include "ui/views/controls/label.h"

namespace arc {
namespace input_overlay {
// ActionLabel is the basic UI label for the action. It can set default view
// mode and edit mode.
class ActionLabel : public views::Label {
 public:
  ActionLabel();
  explicit ActionLabel(const std::u16string& text);

  ActionLabel(const ActionLabel&) = delete;
  ActionLabel& operator=(const ActionLabel&) = delete;
  ~ActionLabel() override;

  // Set it with default view mode.
  void SetDefaultViewMode();
  // Set position from its center position.
  void SetPositionFromCenterPosition(gfx::PointF& center_position);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
};
}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_LABEL_H_
