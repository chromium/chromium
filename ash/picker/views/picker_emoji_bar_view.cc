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
#include "ash/style/system_shadow.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view.h"

namespace ash {

PickerEmojiBarView::PickerEmojiBarView(
    PickerSearchResultsViewDelegate* delegate,
    int picker_view_width,
    PickerAssetFetcher* asset_fetcher)
    : delegate_(delegate) {
  SetUseDefaultFillLayout(true);
  SetBackground(views::CreateThemedRoundedRectBackground(
      kPickerContainerBackgroundColor, kPickerContainerBorderRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kPickerContainerBorderRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, kPickerContainerShadowType);
  shadow_->SetRoundedCornerRadius(kPickerContainerBorderRadius);

  item_row_ = AddChildView(
      std::make_unique<PickerSectionView>(picker_view_width, asset_fetcher));
}

PickerEmojiBarView::~PickerEmojiBarView() = default;

void PickerEmojiBarView::ClearSearchResults() {
  item_row_->ClearItems();
}

void PickerEmojiBarView::SetSearchResults(PickerSearchResultsSection section) {
  ClearSearchResults();
  for (const auto& result : section.results()) {
    // `base::Unretained` is safe here because `this` will own the item view
    // which takes this callback.
    item_row_->AddResult(
        result, base::BindRepeating(&PickerEmojiBarView::SelectSearchResult,
                                    base::Unretained(this), result));
  }
}

void PickerEmojiBarView::SelectSearchResult(const PickerSearchResult& result) {
  delegate_->SelectSearchResult(result);
}

BEGIN_METADATA(PickerEmojiBarView)
END_METADATA

}  // namespace ash
