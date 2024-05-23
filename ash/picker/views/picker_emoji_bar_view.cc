// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_emoji_bar_view.h"

#include <memory>

#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_style.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_shadow.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kPickerEmojiBarHeight = 48;

// Padding around the more emojis icon button.
constexpr auto kMoreEmojisIconButtonPadding = gfx::Insets::TLBR(0, 8, 0, 12);

}  // namespace

PickerEmojiBarView::PickerEmojiBarView(
    PickerSearchResultsViewDelegate* delegate,
    int picker_view_width,
    PickerAssetFetcher* asset_fetcher)
    : delegate_(delegate), picker_view_width_(picker_view_width) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  SetBackground(views::CreateThemedRoundedRectBackground(
      kPickerContainerBackgroundColor, kPickerContainerBorderRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kPickerContainerBorderRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, kPickerContainerShadowType);
  shadow_->SetRoundedCornerRadius(kPickerContainerBorderRadius);

  // base::Unretained is safe here because this class owns
  // `more_emojis_button_`.
  more_emojis_button_ = AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&PickerEmojiBarView::OpenMoreEmojis,
                          base::Unretained(this)),
      IconButton::Type::kSmallFloating, &kPickerMoreEmojisIcon,
      IDS_PICKER_MORE_EMOJIS_BUTTON_ACCESSIBLE_NAME));
  more_emojis_button_->SetProperty(views::kMarginsKey,
                                   kMoreEmojisIconButtonPadding);

  item_row_ = AddChildViewAt(
      std::make_unique<PickerSectionView>(
          picker_view_width_ - more_emojis_button_->GetPreferredSize().width() -
              kMoreEmojisIconButtonPadding.width(),
          asset_fetcher),
      0);
}

PickerEmojiBarView::~PickerEmojiBarView() = default;

gfx::Size PickerEmojiBarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(picker_view_width_, kPickerEmojiBarHeight);
}

void PickerEmojiBarView::ClearSearchResults() {
  item_row_->ClearItems();
}

void PickerEmojiBarView::SetSearchResults(PickerSearchResultsSection section) {
  ClearSearchResults();
  for (const auto& result : section.results()) {
    // `base::Unretained` is safe here because `this` will own the item view
    // which takes this callback.
    item_row_->AddResult(
        result, /*preview_controller=*/nullptr,
        base::BindRepeating(&PickerEmojiBarView::SelectSearchResult,
                            base::Unretained(this), result));
  }
}

void PickerEmojiBarView::SelectSearchResult(const PickerSearchResult& result) {
  delegate_->SelectSearchResult(result);
}

void PickerEmojiBarView::OpenMoreEmojis() {
  delegate_->SelectMoreResults(PickerSectionType::kExpressions);
}

BEGIN_METADATA(PickerEmojiBarView)
END_METADATA

}  // namespace ash
