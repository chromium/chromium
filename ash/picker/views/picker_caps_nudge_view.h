// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_CAPS_NUDGE_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_CAPS_NUDGE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// View for the caps notifier in the picker zero state
class ASH_EXPORT PickerCapsNudgeView : public views::View {
  METADATA_HEADER(PickerCapsNudgeView, views::View)

 public:
  PickerCapsNudgeView();
  PickerCapsNudgeView(const PickerCapsNudgeView&) = delete;
  PickerCapsNudgeView& operator=(const PickerCapsNudgeView&) = delete;
  ~PickerCapsNudgeView() override;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_CAPS_NUDGE_VIEW_H_
