// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_item_view.h"

#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/style/ash_color_id.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"

namespace ash {

GlanceablesClassroomItemView::GlanceablesClassroomItemView(
    const GlanceablesClassroomStudentAssignment* assignment) {
  SetPreferredSize(gfx::Size(50, 50));
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, 4));
  SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 2, 0));

  views::Label* label = AddChildView(std::make_unique<views::Label>(
      base::UTF8ToUTF16(assignment->ToString())));
  label->SetAutoColorReadabilityEnabled(false);
}

GlanceablesClassroomItemView::~GlanceablesClassroomItemView() = default;

BEGIN_METADATA(GlanceablesClassroomItemView, views::View)
END_METADATA

}  // namespace ash
