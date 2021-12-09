// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_name_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

namespace {

// The font size increase for the template name view.
constexpr int kNameFontSizeDeltaDp = 4;

}  // namespace

DesksTemplatesNameView::DesksTemplatesNameView() {
  // TODO(richui): We need to shift the alignment of the `name_view_` in the
  // `DesksTemplatesItemView` so that the text lines up with the other UI
  // elements. This will be done by refactoring `WmHighlightItemBorder` to
  // adjust the border, which we update here.
  // TODO(richui): This initial change is to add the styling of the textfield.
  // Subsequent CLs will be added to implement the renaming functionality. At
  // that time, we will re-evaulate if this class is necessary, or if we can
  // move all this logic into helper functions in `DesksTemplatesItemView`.
  auto border = std::make_unique<WmHighlightItemBorder>(
      LabelTextfield::kLabelTextfieldBorderRadius);
  border_ptr_ = border.get();

  views::Builder<DesksTemplatesNameView>(this)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetBorder(std::move(border))
      .BuildChildren();

  SetFontList(GetFontList().Derive(kNameFontSizeDeltaDp, gfx::Font::NORMAL,
                                   gfx::Font::Weight::BOLD));
}

DesksTemplatesNameView::~DesksTemplatesNameView() = default;

BEGIN_METADATA(DesksTemplatesNameView, LabelTextfield)
END_METADATA

}  // namespace ash
