// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_emoji_bar_view.h"

#include <memory>
#include <utility>
#include <variant>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/views/picker_emoji_bar_view_delegate.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_style.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "base/check_op.h"
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
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
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

std::unique_ptr<views::View> CreateEmptyCell() {
  auto cell_view = std::make_unique<views::View>();
  cell_view->SetUseDefaultFillLayout(true);
  cell_view->GetViewAccessibility().SetRole(ax::mojom::Role::kGridCell);
  return cell_view;
}

std::u16string GetTooltipForEmojiResult(const PickerEmojiResult& result) {
  switch (result.type) {
    case PickerEmojiResult::Type::kEmoji:
      return l10n_util::GetStringFUTF16(IDS_PICKER_EMOJI_ITEM_ACCESSIBLE_NAME,
                                        result.name);
    case PickerEmojiResult::Type::kSymbol:
      return result.name;
    case PickerEmojiResult::Type::kEmoticon:
      return l10n_util::GetStringFUTF16(
          IDS_PICKER_EMOTICON_ITEM_ACCESSIBLE_NAME, result.name);
  }
  NOTREACHED();
}

// Creates an item view for a search result. Only supports results that can be
// added to the emoji bar, i.e. emojis, symbols and emoticons.
std::unique_ptr<PickerItemView> CreateItemView(
    const PickerEmojiResult& result,
    base::RepeatingClosure select_result_callback) {
  std::unique_ptr<PickerItemView> item_view;
  switch (result.type) {
    case PickerEmojiResult::Type::kEmoji:
      item_view = std::make_unique<PickerEmojiItemView>(
          PickerEmojiItemView::Style::kEmoji, std::move(select_result_callback),
          result.text);
      item_view->SetPreferredSize(kEmojiBarItemPreferredSize);
      break;
    case PickerEmojiResult::Type::kSymbol:
      item_view = std::make_unique<PickerEmojiItemView>(
          PickerEmojiItemView::Style::kSymbol,
          std::move(select_result_callback), result.text);
      item_view->SetPreferredSize(kEmojiBarItemPreferredSize);
      break;
    case PickerEmojiResult::Type::kEmoticon:
      item_view = std::make_unique<PickerEmojiItemView>(
          PickerEmojiItemView::Style::kEmoticon,
          std::move(select_result_callback), result.text);
      item_view->SetPreferredSize(
          gfx::Size(std::max(item_view->GetPreferredSize().width(),
                             kEmojiBarItemPreferredSize.width()),
                    kEmojiBarItemPreferredSize.height()));
      break;
  }

  if (!result.name.empty()) {
    std::u16string tooltip = GetTooltipForEmojiResult(result);
    item_view->SetTooltipText(tooltip);
    item_view->SetAccessibleName(std::move(tooltip));
  }

  return item_view;
}

std::unique_ptr<views::View> CreateItemRow() {
  std::unique_ptr<views::View> view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetBetweenChildSpacing(kItemGap)
          .Build();
  view->GetViewAccessibility().SetRole(ax::mojom::Role::kNone);
  return view;
}

class GifsButton : public views::LabelButton {
  METADATA_HEADER(GifsButton, views::LabelButton)

 public:
  explicit GifsButton(base::RepeatingClosure pressed_callback) {
    // The label is not translated to keep the width constant. Treat it as an
    // icon.
    views::Builder<views::LabelButton>(this)
        .SetText(u"GIF")
        .SetCallback(std::move(pressed_callback))
        .SetEnabledTextColorIds(cros_tokens::kCrosSysOnSurface)
        .BuildChildren();
    label()->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosLabel1));
    label()->SetLineHeight(ash::TypographyProvider::Get()->ResolveLineHeight(
        ash::TypographyToken::kCrosLabel1));
    label()->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);
    StyleUtil::SetUpInkDropForButton(this);
    StyleUtil::InstallRoundedCornerHighlightPathGenerator(
        this, gfx::RoundedCornersF(kGifsButtonCornerRadius));
    UpdateBackground();
    SetProperty(views::kElementIdentifierKey, kPickerGifElementId);
  }
  GifsButton(const GifsButton&) = delete;
  GifsButton& operator=(const GifsButton&) = delete;
  ~GifsButton() override = default;

  // views::LabelButton:
  void StateChanged(ButtonState old_state) override {
    views::LabelButton::StateChanged(old_state);
    UpdateBackground();
  }

  void UpdateBackground() {
    SetBackground(views::CreateThemedRoundedRectBackground(
        GetState() == views::Button::ButtonState::STATE_HOVERED
            ? cros_tokens::kCrosSysHoverOnSubtle
            : cros_tokens::kCrosSysSystemOnBase,
        kGifsButtonCornerRadius));
  }
};

BEGIN_METADATA(GifsButton)
END_METADATA

}  // namespace

PickerEmojiBarView::PickerEmojiBarView(PickerEmojiBarViewDelegate* delegate,
                                       int picker_view_width,
                                       bool is_gifs_enabled)
    : delegate_(delegate), picker_view_width_(picker_view_width) {
  SetUseDefaultFillLayout(true);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGrid);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      is_gifs_enabled ? IDS_PICKER_EMOJI_BAR_WITH_GIFS_GRID_ACCESSIBLE_NAME
                      : IDS_PICKER_EMOJI_BAR_GRID_ACCESSIBLE_NAME));
  SetProperty(views::kElementIdentifierKey, kPickerEmojiBarElementId);
  SetBackground(views::CreateThemedRoundedRectBackground(
      kPickerContainerBackgroundColor, kPickerContainerBorderRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kPickerContainerBorderRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, kPickerContainerShadowType);
  shadow_->SetRoundedCornerRadius(kPickerContainerBorderRadius);

  auto* row =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::LayoutOrientation::kHorizontal)
                       .SetInsideBorderInsets(kEmojiBarMargins)
                       .SetBetweenChildSpacing(kGifsAndMoreEmojisSpacing)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
                       .Build());
  row->GetViewAccessibility().SetRole(ax::mojom::Role::kRow);

  auto* item_row_and_gifs_container = row->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetBetweenChildSpacing(kItemRowAndGifsSpacing)
          .SetProperty(views::kBoxLayoutFlexKey,
                       views::BoxLayoutFlexSpecification().WithWeight(1))
          .AddChildren(
              views::Builder<views::View>(CreateItemRow())
                  .CopyAddressTo(&item_row_),
              views::Builder<views::View>(CreateEmptyCell())
                  .AddChild(
                      // base::Unretained is safe here because this class owns
                      // `gifs_button_`.
                      views::Builder<views::Button>(
                          std::make_unique<GifsButton>(
                              base::BindRepeating(&PickerEmojiBarView::OpenGifs,
                                                  base::Unretained(this))))
                          .SetVisible(is_gifs_enabled)
                          .CopyAddressTo(&gifs_button_)))
          .Build());
  item_row_and_gifs_container->GetViewAccessibility().SetRole(
      ax::mojom::Role::kNone);

  // base::Unretained is safe here because this class owns
  // `more_emojis_button_`.
  more_emojis_button_ =
      row->AddChildView(CreateEmptyCell())
          ->AddChildView(std::make_unique<IconButton>(
              base::BindRepeating(&PickerEmojiBarView::OpenMoreEmojis,
                                  base::Unretained(this)),
              IconButton::Type::kSmallFloating, &kPickerMoreEmojisIcon,
              is_gifs_enabled
                  ? IDS_PICKER_MORE_EMOJIS_AND_GIFS_BUTTON_ACCESSIBLE_NAME
                  : IDS_PICKER_MORE_EMOJIS_BUTTON_ACCESSIBLE_NAME));
  more_emojis_button_->SetProperty(views::kElementIdentifierKey,
                                   kPickerMoreEmojisElementId);

  StyleUtil::SetUpInkDropForButton(more_emojis_button_, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);
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
    std::vector<PickerEmojiResult> results) {
  ClearSearchResults();
  int item_row_width = 0;
  // This may be slow to calculate, so only `CHECK` on debug builds.
  DCHECK_EQ(item_row_width, item_row_->GetPreferredSize().width());
  const int available_item_row_width = CalculateAvailableWidthForItemRow();
  for (const auto& result : results) {
    // `base::Unretained` is safe here because `this` owns the item view.
    auto item_view = CreateItemView(
        result, base::BindRepeating(&PickerEmojiBarView::SelectSearchResult,
                                    base::Unretained(this), result));
    int new_item_row_width =
        item_row_width + item_view->GetPreferredSize().width();
    if (item_row_width != 0) {
      new_item_row_width += kItemGap;
    }

    // Add the item if there is enough space in the row.
    if (new_item_row_width <= available_item_row_width) {
      item_row_->AddChildView(CreateEmptyCell())
          ->AddChildView(std::move(item_view));
      item_row_width = new_item_row_width;

      DCHECK_EQ(item_row_width, item_row_->GetPreferredSize().width());
      DCHECK_EQ(available_item_row_width, CalculateAvailableWidthForItemRow());
    } else {
      // A narrower item after this one may fit, but we should not show it
      // because we always want to show a contiguous prefix of `results`.
      break;
    }
  }
}

size_t PickerEmojiBarView::GetNumItems() const {
  return item_row_->children().size();
}

void PickerEmojiBarView::SelectSearchResult(const PickerSearchResult& result) {
  delegate_->SelectSearchResult(result);
}

void PickerEmojiBarView::OpenMoreEmojis() {
  delegate_->ShowEmojiPicker(ui::EmojiPickerCategory::kEmojis);
}

void PickerEmojiBarView::OpenGifs() {
  delegate_->ToggleGifs();
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

views::View::Views PickerEmojiBarView::GetItemsForTesting() const {
  views::View::Views items;
  for (views::View* child : item_row_->children()) {
    items.push_back(child->children().front());
  }
  return items;
}

BEGIN_METADATA(PickerEmojiBarView)
END_METADATA

}  // namespace ash
