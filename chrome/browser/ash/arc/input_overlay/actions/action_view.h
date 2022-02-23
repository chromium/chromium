// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_VIEW_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_circle.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action_label.h"
#include "chrome/browser/ash/arc/input_overlay/display_mode.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/view.h"

namespace arc {
namespace input_overlay {

class Action;

// ActionView is the view for each action.
class ActionView : public views::View {
 public:
  explicit ActionView(Action* owner);
  ActionView(const ActionView&) = delete;
  ActionView& operator=(const ActionView&) = delete;
  ~ActionView() override;

  Action* action() { return action_; }

  void set_editable(bool editable) { editable_ = editable; }

  // Set position from its center position.
  void SetPositionFromCenterPosition(gfx::PointF& center_position);
  void SetDisplayMode(const DisplayMode mode);

 protected:
  Action* action_ = nullptr;
  bool editable_ = false;
  ActionCircle* circle_ = nullptr;
  std::vector<ActionLabel*> labels_;
  DisplayMode current_display_mode_ = DisplayMode::kNone;
  gfx::Point center_;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ACTIONS_ACTION_VIEW_H_
