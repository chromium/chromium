// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_emoji_bar_view.h"

#include <memory>
#include <utility>
#include <variant>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/views/picker_emoji_bar_view_delegate.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_style.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kPickerEmojiBarHeight = 48;

// Padding around the emoji bar content.
constexpr auto kEmojiBarMargins = gfx::Insets::TLBR(8, 16, 8, 12);

// Gap between the item row (containing emojis) and the gif button.
constexpr int kItemRowAndGifsSpacing = 12;

// Gap between the gif button and the more emojis button.
constexpr int kGifsAndMoreEmojisSpacing = 12;

constexpr gfx::Size kEmojiBarItemPreferredSize(32, 32);

constexpr auto kGifsButtonCornerRadius = 12;

// Horizontal padding between items in the item row.
constexpr int kItemGap = 12;

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
  return views::Builder<views::BoxLayoutView>()
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetBetweenChildSpacing(kItemGap)
      .Build();
}

class GifsButton : public views::LabelButton {
  METADATA_HEADER(GifsButton, views::LabelButton)

 public:
  explicit GifsButton(base::RepeatingClosure pressed_callback) {
    views::Builder<views::LabelButton>(this)
        .SetText(l10n_util::GetStringUTF16(IDS_PICKER_GIFS_BUTTON_LABEL))
        .SetCallback(std::move(pressed_callback))
        .SetEnabledTextColorIds(cros_tokens::kCrosSysOnSurface)
        .SetBackground(views::CreateThemedRoundedRectBackground(
            cros_tokens::kCrosSysSystemOnBase, kGifsButtonCornerRadius))
        .BuildChildren();
    label()->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosLabel1));
    label()->SetLineHeight(ash::TypographyProvider::Get()->ResolveLineHeight(
        ash::TypographyToken::kCrosLabel1));
    StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                     /*highlight_on_hover=*/true,
                                     /*highlight_on_focus=*/true);
    StyleUtil::InstallRoundedCornerHighlightPathGenerator(
        this, gfx::RoundedCornersF(kGifsButtonCornerRadius));
  }
  GifsButton(const GifsButton&) = delete;
  GifsButton& operator=(const GifsButton&) = delete;
  ~GifsButton() override = default;
};

BEGIN_METADATA(GifsButton)
END_METADATA

}  // namespace

PickerEmojiBarView::PickerEmojiBarView(PickerEmojiBarViewDelegate* delegate,
                                       int picker_view_width)
    : delegate_(delegate), picker_view_width_(picker_view_width) {
  SetProperty(views::kElementIdentifierKey, kPickerEmojiBarElementId);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
                       views::LayoutOrientation::kHorizontal,
                       /*inside_border_insets=*/kEmojiBarMargins,
                       /*between_child_spacing=*/kGifsAndMoreEmojisSpacing))
      ->set_cross_axis_alignment(views::LayoutAlignment::kCenter);

  SetBackground(views::CreateThemedRoundedRectBackground(
      kPickerContainerBackgroundColor, kPickerContainerBorderRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kPickerContainerBorderRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, kPickerContainerShadowType);
  shadow_->SetRoundedCornerRadius(kPickerContainerBorderRadius);

  auto* container = AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetBetweenChildSpacing(kItemRowAndGifsSpacing)
          .SetProperty(views::kBoxLayoutFlexKey,
                       views::BoxLayoutFlexSpecification().WithWeight(1))
          .Build());
  item_row_ = container->AddChildView(CreateItemRow());

  // base::Unretained is safe here because this class owns `gifs_button_`.
  gifs_button_ =
      container->AddChildView(std::make_unique<GifsButton>(base::BindRepeating(
          &PickerEmojiBarView::OpenGifs, base::Unretained(this))));

  // base::Unretained is safe here because this class owns
  // `more_emojis_button_`.
  more_emojis_button_ = AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(&PickerEmojiBarView::OpenMoreEmojis,
                          base::Unretained(this)),
      IconButton::Type::kSmallFloating, &kPickerMoreEmojisIcon,
      IDS_PICKER_MORE_EMOJIS_BUTTON_ACCESSIBLE_NAME));
}

PickerEmojiBarView::~PickerEmojiBarView() = default;

gfx::Size PickerEmojiBarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(picker_view_width_, kPickerEmojiBarHeight);
}

views::View* PickerEmojiBarView::GetTopItem() {
  return GetLeftmostItem();
}

views::View* PickerEmojiBarView::GetBottomItem() {
  return GetLeftmostItem();
}

views::View* PickerEmojiBarView::GetItemAbove(views::View* item) {
  return nullptr;
}

views::View* PickerEmojiBarView::GetItemBelow(views::View* item) {
  return nullptr;
}

views::View* PickerEmojiBarView::GetItemLeftOf(views::View* item) {
  views::View* item_left_of = GetNextPickerPseudoFocusableView(
      item, PickerPseudoFocusDirection::kBackward, /*should_loop=*/false);
  return Contains(item_left_of) ? item_left_of : nullptr;
}

views::View* PickerEmojiBarView::GetItemRightOf(views::View* item) {
  views::View* item_right_of = GetNextPickerPseudoFocusableView(
      item, PickerPseudoFocusDirection::kForward, /*should_loop=*/false);
  return Contains(item_right_of) ? item_right_of : nullptr;
}

bool PickerEmojiBarView::ContainsItem(views::View* item) {
  return Contains(item);
}

void PickerEmojiBarView::ClearSearchResults() {
  item_row_->RemoveAllChildViews();
}

void PickerEmojiBarView::SetSearchResults(
    std::vector<PickerSearchResult> results) {
  ClearSearchResults();
  for (const auto& result : results) {
    // `base::Unretained` is safe here because `this` owns the item view.
    auto item_view = CreateItemView(
        result, base::BindRepeating(&PickerEmojiBarView::SelectSearchResult,
                                    base::Unretained(this), result));
    // Add the item if there is enough space in the row.
    if (item_row_->GetPreferredSize().width() + kItemGap +
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
  delegate_->ShowEmojiPicker(ui::EmojiPickerCategory::kEmojis);
}

void PickerEmojiBarView::OpenGifs() {
  delegate_->ShowEmojiPicker(ui::EmojiPickerCategory::kGifs);
}

int PickerEmojiBarView::CalculateAvailableWidthForItemRow() {
  return picker_view_width_ - kEmojiBarMargins.width() -
         kItemRowAndGifsSpacing - gifs_button_->GetPreferredSize().width() -
         kGifsAndMoreEmojisSpacing -
         more_emojis_button_->GetPreferredSize().width();
}

views::View* PickerEmojiBarView::GetLeftmostItem() {
  if (GetFocusManager() == nullptr) {
    return nullptr;
  }
  views::View* leftmost_item = GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/false,
      /*dont_loop=*/false);
  return Contains(leftmost_item) ? leftmost_item : nullptr;
}

BEGIN_METADATA(PickerEmojiBarView)
END_METADATA

}  // namespace ash
