// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_emoji_bar_view.h"

#include <memory>
#include <utility>
#include <variant>

#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_style.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/system_shadow.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kPickerEmojiBarHeight = 48;

constexpr gfx::Size kEmojiBarItemPreferredSize(32, 32);

// Padding around the more emojis icon button.
constexpr auto kMoreEmojisIconButtonPadding = gfx::Insets::TLBR(0, 8, 0, 12);

// Padding around the item row.
constexpr auto kItemRowMargins = gfx::Insets::TLBR(8, 16, 8, 0);

// Horizontal padding between items in the item row.
constexpr auto kItemMargins = gfx::Insets::VH(0, 12);

// Creates an item view for a search result. Only supports results that can be
// added to the emoji bar, i.e. emojis, symbols and emoticons.
std::unique_ptr<PickerItemView> CreateItemView(
    const PickerSearchResult& result,
    base::RepeatingClosure select_result_callback) {
  using ReturnType = std::unique_ptr<PickerItemView>;
  return std::visit(
      base::Overloaded{
          [&](const PickerSearchResult::EmojiData& data) -> ReturnType {
            auto emoji_item = std::make_unique<PickerEmojiItemView>(
                std::move(select_result_callback), data.emoji);
            emoji_item->SetPreferredSize(kEmojiBarItemPreferredSize);
            return emoji_item;
          },
          [&](const PickerSearchResult::SymbolData& data) -> ReturnType {
            auto symbol_item = std::make_unique<PickerSymbolItemView>(
                std::move(select_result_callback), data.symbol);
            symbol_item->SetPreferredSize(kEmojiBarItemPreferredSize);
            return symbol_item;
          },
          [&](const PickerSearchResult::EmoticonData& data) -> ReturnType {
            auto emoticon_item = std::make_unique<PickerEmoticonItemView>(
                std::move(select_result_callback), data.emoticon);
            emoticon_item->SetPreferredSize(
                gfx::Size(std::max(emoticon_item->GetPreferredSize().width(),
                                   kEmojiBarItemPreferredSize.width()),
                          kEmojiBarItemPreferredSize.height()));
            return emoticon_item;
          },
          [&](const auto& data) -> ReturnType { NOTREACHED_NORETURN(); },
      },
      result.data());
}

std::unique_ptr<views::View> CreateItemRow() {
  auto row = views::Builder<views::FlexLayoutView>()
                 .SetOrientation(views::LayoutOrientation::kHorizontal)
                 .SetMainAxisAlignment(views::LayoutAlignment::kStart)
                 .SetCollapseMargins(true)
                 .SetIgnoreDefaultMainAxisMargins(true)
                 .SetInteriorMargin(kItemRowMargins)
                 .SetProperty(views::kFlexBehaviorKey,
                              views::FlexSpecification(
                                  views::MinimumFlexSizeRule::kScaleToMinimum,
                                  views::MaximumFlexSizeRule::kUnbounded))
                 .Build();
  row->SetDefault(views::kMarginsKey, kItemMargins);
  return row;
}

}  // namespace

PickerEmojiBarView::PickerEmojiBarView(
    PickerSearchResultsViewDelegate* delegate,
    int picker_view_width)
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

  item_row_ = AddChildView(CreateItemRow());

  // base::Unretained is safe here because this class owns
  // `more_emojis_button_`.
  more_emojis_button_ = AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&PickerEmojiBarView::OpenMoreEmojis,
                          base::Unretained(this)),
      IconButton::Type::kSmallFloating, &kPickerMoreEmojisIcon,
      IDS_PICKER_MORE_EMOJIS_BUTTON_ACCESSIBLE_NAME));
  more_emojis_button_->SetProperty(views::kMarginsKey,
                                   kMoreEmojisIconButtonPadding);
}

PickerEmojiBarView::~PickerEmojiBarView() = default;

gfx::Size PickerEmojiBarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(picker_view_width_, kPickerEmojiBarHeight);
}

void PickerEmojiBarView::ClearSearchResults() {
  item_row_->RemoveAllChildViews();
}

void PickerEmojiBarView::SetSearchResults(PickerSearchResultsSection section) {
  ClearSearchResults();
  for (const auto& result : section.results()) {
    // `base::Unretained` is safe here because `this` owns the item view.
    auto item_view = CreateItemView(
        result, base::BindRepeating(&PickerEmojiBarView::SelectSearchResult,
                                    base::Unretained(this), result));
    // Add the item if there is enough space in the row.
    if (item_row_->GetPreferredSize().width() + kItemMargins.left() +
            item_view->GetPreferredSize().width() <=
        CalculateAvailableWidthForItemRow()) {
      item_row_->AddChildView(std::move(item_view));
    }
  }
}

void PickerEmojiBarView::SelectSearchResult(const PickerSearchResult& result) {
  delegate_->SelectSearchResult(result);
}

void PickerEmojiBarView::OpenMoreEmojis() {
  delegate_->SelectMoreResults(PickerSectionType::kExpressions);
}

int PickerEmojiBarView::CalculateAvailableWidthForItemRow() {
  return picker_view_width_ - more_emojis_button_->GetPreferredSize().width() -
         kMoreEmojisIconButtonPadding.width();
}

BEGIN_METADATA(PickerEmojiBarView)
END_METADATA

}  // namespace ash
