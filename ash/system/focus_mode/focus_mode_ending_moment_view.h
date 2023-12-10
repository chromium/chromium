// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_ENDING_MOMENT_VIEW_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_ENDING_MOMENT_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

class PillButton;

// Contains congratulatory text for the user completing a focus session, as well
// as buttons to complete the session or add 10 minutes. This is only shown when
// the session duration is reached.
class FocusModeEndingMomentView : public views::FlexLayoutView {
  METADATA_HEADER(FocusModeEndingMomentView, views::FlexLayoutView)

 public:
  FocusModeEndingMomentView();
  FocusModeEndingMomentView(const FocusModeEndingMomentView&) = delete;
  FocusModeEndingMomentView& operator=(const FocusModeEndingMomentView&) =
      delete;
  ~FocusModeEndingMomentView() override = default;

  // Used to set if `extend_session_duration_button_` should be enabled or not.
  void SetExtendButtonEnabled(bool enabled);

 private:
  friend class FocusModeEndingMomentViewTest;

  // The `+10 min` button.
  raw_ptr<PillButton> extend_session_duration_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_ENDING_MOMENT_VIEW_H_
