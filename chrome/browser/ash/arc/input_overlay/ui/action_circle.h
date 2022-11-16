// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_CIRCLE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_CIRCLE_H_

#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class ActionCircle : public views::View {
 public:
  class CircleBackground;

  explicit ActionCircle(int radius);
  ActionCircle(const ActionCircle&) = delete;
  ActionCircle& operator=(const ActionCircle&) = delete;
  ~ActionCircle() override;

  void SetDisplayMode(DisplayMode mode);

 private:
  DisplayMode current_mode_ = DisplayMode::kNone;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_CIRCLE_H_
