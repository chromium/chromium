// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_
#define ASH_WM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class RoundedContainer;

// This view displays the focus panel settings that a user can set.
class ASH_EXPORT FocusModeDetailedView : public TrayDetailedView {
 public:
  METADATA_HEADER(FocusModeDetailedView);

  explicit FocusModeDetailedView(DetailedViewDelegate* delegate);
  FocusModeDetailedView(const FocusModeDetailedView&) = delete;
  FocusModeDetailedView& operator=(const FocusModeDetailedView&) = delete;
  ~FocusModeDetailedView() override;

 private:
  // This view contains a description of the focus session, as well as a toggle
  // button for staring/ending focus mode.
  raw_ptr<RoundedContainer, ExperimentalAsh> toggle_view_ = nullptr;
  // This view contains the timer view for the user to adjust the focus session
  // duration.
  raw_ptr<RoundedContainer, ExperimentalAsh> timer_view_ = nullptr;
  // This view contains controls for selecting the focus scene (background +
  // audio), as well as volume controls.
  raw_ptr<RoundedContainer, ExperimentalAsh> scene_view_ = nullptr;
  // This view contains a toggle for turning on/off DND.
  raw_ptr<RoundedContainer, ExperimentalAsh> do_not_disturb_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_WM_FOCUS_MODE_FOCUS_MODE_DETAILED_VIEW_H_
