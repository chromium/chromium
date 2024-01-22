// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/style/typography.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr auto kSectionTitlePadding = gfx::Insets::VH(8, 16);

}  // namespace

PickerSectionView::PickerSectionView(const std::u16string& title_text) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  title_ = AddChildView(
      bubble_utils::CreateLabel(TypographyToken::kCrosAnnotation2, title_text,
                                cros_tokens::kCrosSysOnSurfaceVariant));
  title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_->SetBorder(views::CreateEmptyBorder(kSectionTitlePadding));
}

PickerSectionView::~PickerSectionView() = default;

void PickerSectionView::AddItemView(std::unique_ptr<PickerItemView> item_view) {
  item_views_.push_back(AddChildView(std::move(item_view)));
}

BEGIN_METADATA(PickerSectionView)
END_METADATA

}  // namespace ash
