// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_BADGE_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_BADGE_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace ash {

// View for a badge containing a label with an icon after it.
class ASH_EXPORT PickerBadgeView : public views::FlexLayoutView {
  METADATA_HEADER(PickerBadgeView, views::FlexLayoutView)

 public:
  PickerBadgeView();
  PickerBadgeView(const PickerBadgeView&) = delete;
  PickerBadgeView& operator=(const PickerBadgeView&) = delete;
  ~PickerBadgeView() override;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, PickerBadgeView, views::FlexLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PickerBadgeView)

#endif  // ASH_PICKER_VIEWS_PICKER_BADGE_VIEW_H_
