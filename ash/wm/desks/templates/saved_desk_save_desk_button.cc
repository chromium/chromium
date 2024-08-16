// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/highlight_border.h"

namespace ash {

SavedDeskSaveDeskButton::SavedDeskSaveDeskButton(
    base::RepeatingClosure callback,
    const std::u16string& text,
    DeskTemplateType type,
    const gfx::VectorIcon* icon)
    : PillButton(callback,
                 text,
                 PillButton::Type::kDefaultElevatedWithIconLeading,
                 icon),
      callback_(callback),
      type_(type) {
  CHECK(type == DeskTemplateType::kTemplate ||
        type == DeskTemplateType::kSaveAndRecall);

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  SetBorder(std::make_unique<views::HighlightBorder>(
      kSaveDeskCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderNoShadow));
  SetEnableBackgroundBlur(true);
}

SavedDeskSaveDeskButton::~SavedDeskSaveDeskButton() = default;

BEGIN_METADATA(SavedDeskSaveDeskButton)
END_METADATA

}  // namespace ash
