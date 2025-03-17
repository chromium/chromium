// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_emoji_bar_view.h"

#include <memory>
#include <utility>
#include <variant>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/views/quick_insert_emoji_bar_view_delegate.h"
#include "ash/quick_insert/views/quick_insert_emoji_item_view.h"
#include "ash/quick_insert/views/quick_insert_item_view.h"
#include "ash/quick_insert/views/quick_insert_pseudo_focus.h"
#include "ash/quick_insert/views/quick_insert_strings.h"
#include "ash/quick_insert/views/quick_insert_style.h"
#include "ash/quick_insert/views/quick_insert_traversable_item_container.h"
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
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
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

constexpr int kQuickInsertEmojiBarHeight = 48;

// Padding around the emoji bar content.
constexpr auto kEmojiBarMargins = gfx::Insets::TLBR(8, 16, 8, 12);

// Gap between the item row (containing emojis) and the gif button.
constexpr int kItemRowAndGifsSpacing = 12;

// Gap between the gif button and the more emojis button.
constexpr int kGifsAndMoreEmojisSpacing = 12;

constexpr gfx::Size kEmojiBarItemPreferredSize(32, 32);

constexpr auto kGifsButtonCornerRadius = 12;

// Horizontal padding between items in the item row.
constexpr int kItemGap = 8;

// Horizontal gap between the GIFs button icon and the label.
constexpr int kGifsButtonIconLabelSpacing = 2;

// Size of the GIFs button icon.
constexpr int kGifsButtonIconSize = 16;

// Height of the GIFs button.
constexpr int kGifsButtonHeight = 24;

std::unique_ptr<views::View> CreateEmptyCell() {
  auto cell_view = std::make_unique<views::View>();
  cell_view->SetUseDefaultFillLayout(true);
  cell_view->GetViewAccessibility().SetRole(ax::mojom::Role::kGridCell);
  return cell_view;
}

std::u16string GetTooltipForEmojiResult(const QuickInsertEmojiResult& result) {
  switch (result.type) {
    case QuickInsertEmojiResult::Type::kEmoji:
      return l10n_util::GetStringFUTF16(IDS_PICKER_EMOJI_ITEM_ACCESSIBLE_NAME,
                                        result.name);
    case QuickInsertEmojiResult::Type::kSymbol:
      return result.name;
    case QuickInsertEmojiResult::Type::kEmoticon:
      return l10n_util::GetStringFUTF16(
          IDS_PICKER_EMOTICON_ITEM_ACCESSIBLE_NAME, result.name);
  }
  NOTREACHED();
}

// Creates an item view for a search result. Only supports results that can be
// added to the emoji bar, i.e. emojis, symbols and emoticons.
std::unique_ptr<QuickInsertItemView> CreateItemView(
    const QuickInsertEmojiResult& result,
    base::RepeatingClosure select_result_callback) {
  std::unique_ptr<QuickInsertItemView> item_view;
  switch (result.type) {
    case QuickInsertEmojiResult::Type::kEmoji:
      item_view = std::make_unique<QuickInsertEmojiItemView>(
          QuickInsertEmojiItemView::Style::kEmoji,
          std::move(select_result_callback), result.text);
      item_view->SetPreferredSize(kEmojiBarItemPreferredSize);
      break;
    case QuickInsertEmojiResult::Type::kSymbol:
      item_view = std::make_unique<QuickInsertEmojiItemView>(
          QuickInsertEmojiItemView::Style::kSymbol,
          std::move(select_result_callback), result.text);
      item_view->SetPreferredSize(kEmojiBarItemPreferredSize);
      break;
    case QuickInsertEmojiResult::Type::kEmoticon:
      item_view = std::make_unique<QuickInsertEmojiItemView>(
          QuickInsertEmojiItemView::Style::kEmoticon,
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
  // `pressed_callback` takes in whether the GIFs button is checked or not
  // (after the press).
  explicit GifsButton(base::RepeatingCallback<void(bool)> pressed_callback) {
    views::Builder<views::LabelButton>(this)
        .SetText(GetLabelForQuickInsertCategory(QuickInsertCategory::kGifs))
        .SetCallback(base::BindRepeating(&GifsButton::OnButtonPressed,
                                         base::Unretained(this))
                         .Then(std::move(pressed_callback)))
        .SetEnabledTextColors(cros_tokens::kCrosSysOnSurface)
        .SetImageLabelSpacing(kGifsButtonIconLabelSpacing)
        .BuildChildren();
    label()->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
        TypographyToken::kCrosLabel1));
    label()->SetLineHeight(TypographyProvider::Get()->ResolveLineHeight(
        TypographyToken::kCrosLabel1));
    label()->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);
    StyleUtil::SetUpInkDropForButton(this);
    StyleUtil::InstallRoundedCornerHighlightPathGenerator(
        this, gfx::RoundedCornersF(kGifsButtonCornerRadius));
    UpdateBackground();
    SetProperty(views::kElementIdentifierKey, kQuickInsertGifElementId);

    if (base::FeatureList::IsEnabled(features::kPickerGifs)) {
      SetMinSize(gfx::Size(0, kGifsButtonHeight));
      SetMaxSize(gfx::Size(0, kGifsButtonHeight));
      GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
      GetViewAccessibility().SetCheckedState(ax::mojom::CheckedState::kFalse);
    }
  }
  GifsButton(const GifsButton&) = delete;
  GifsButton& operator=(const GifsButton&) = delete;
  ~GifsButton() override = default;

  // views::LabelButton:
  void StateChanged(ButtonState old_state) override {
    views::LabelButton::StateChanged(old_state);
    UpdateBackground();
  }
  gfx::Size GetMaximumSize() const override {
    if (!base::FeatureList::IsEnabled(features::kPickerGifs)) {
      return GetPreferredSize();
    }
    const int max_height = views::LabelButton::GetMaximumSize().height();
    return gfx::Size(
        GetPreferredSize().width() +
            (is_checked_ ? 0
                         : kGifsButtonIconSize + kGifsButtonIconLabelSpacing),
        max_height);
  }
  void PaintButtonContents(gfx::Canvas* canvas) override {
    views::LabelButton::PaintButtonContents(canvas);

    if (is_checked_ &&
        GetState() == views::Button::ButtonState::STATE_HOVERED) {
      SkPath mask;
      mask.addRoundRect(gfx::RectToSkRect(GetLocalBounds()),
                        kGifsButtonCornerRadius, kGifsButtonCornerRadius);
      canvas->ClipPath(mask, true);
      canvas->DrawColor(
          GetColorProvider()->GetColor(cros_tokens::kCrosSysHoverOnSubtle));
    }
  }

  void UpdateBackground() {
    SetBackground(views::CreateRoundedRectBackground(
        (is_checked_ ? cros_tokens::kCrosSysSystemPrimaryContainer
                     : (GetState() == views::Button::ButtonState::STATE_HOVERED
                            ? cros_tokens::kCrosSysHoverOnSubtle
                            : cros_tokens::kCrosSysSystemOnBase)),
        kGifsButtonCornerRadius));
  }

  // Returns whether the GIFs button is checked or not after the button press.
  bool OnButtonPressed() {
    if (!base::FeatureList::IsEnabled(features::kPickerGifs)) {
      return false;
    }

    is_checked_ = !is_checked_;
    SetImageModel(
        views::Button::ButtonState::STATE_NORMAL,
        is_checked_
            ? std::make_optional(ui::ImageModel::FromVectorIcon(
                  kCheckIcon, cros_tokens::kCrosSysSystemOnPrimaryContainer,
                  kGifsButtonIconSize))
            : std::nullopt);
    PreferredSizeChanged();
    GetViewAccessibility().SetCheckedState(
        is_checked_ ? ax::mojom::CheckedState::kTrue
                    : ax::mojom::CheckedState::kFalse);
    return is_checked_;
  }

 private:
  bool is_checked_ = false;
};

BEGIN_METADATA(GifsButton)
END_METADATA

}  // namespace

QuickInsertEmojiBarView::QuickInsertEmojiBarView(
    QuickInsertEmojiBarViewDelegate* delegate,
    int quick_insert_view_width,
    bool is_gifs_enabled)
    : delegate_(delegate), quick_insert_view_width_(quick_insert_view_width) {
  SetUseDefaultFillLayout(true);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGrid);
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      is_gifs_enabled ? IDS_PICKER_EMOJI_BAR_WITH_GIFS_GRID_ACCESSIBLE_NAME
                      : IDS_PICKER_EMOJI_BAR_GRID_ACCESSIBLE_NAME));
  SetProperty(views::kElementIdentifierKey, kQuickInsertEmojiBarElementId);
  SetBackground(views::CreateRoundedRectBackground(
      kQuickInsertContainerBackgroundColor, kQuickInsertContainerBorderRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kQuickInsertContainerBorderRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, kQuickInsertContainerShadowType);
  shadow_->SetRoundedCornerRadius(kQuickInsertContainerBorderRadius);

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
                          std::make_unique<GifsButton>(base::BindRepeating(
                              &QuickInsertEmojiBarView::ToggleGifs,
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
              base::BindRepeating(&QuickInsertEmojiBarView::OpenMoreEmojis,
                                  base::Unretained(this)),
              IconButton::Type::kSmallFloating, &kQuickInsertMoreEmojisIcon,
              is_gifs_enabled
                  ? IDS_PICKER_MORE_EMOJIS_AND_GIFS_BUTTON_ACCESSIBLE_NAME
                  : IDS_PICKER_MORE_EMOJIS_BUTTON_ACCESSIBLE_NAME));
  more_emojis_button_->SetProperty(views::kElementIdentifierKey,
                                   kQuickInsertMoreEmojisElementId);

  StyleUtil::SetUpInkDropForButton(more_emojis_button_, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);
}

QuickInsertEmojiBarView::~QuickInsertEmojiBarView() = default;

gfx::Size QuickInsertEmojiBarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(quick_insert_view_width_, kQuickInsertEmojiBarHeight);
}

views::View* QuickInsertEmojiBarView::GetTopItem() {
  return GetLeftmostItem();
}

views::View* QuickInsertEmojiBarView::GetBottomItem() {
  return GetLeftmostItem();
}

views::View* QuickInsertEmojiBarView::GetItemAbove(views::View* item) {
  return nullptr;
}

views::View* QuickInsertEmojiBarView::GetItemBelow(views::View* item) {
  return nullptr;
}

views::View* QuickInsertEmojiBarView::GetItemLeftOf(views::View* item) {
  views::View* item_left_of = GetNextQuickInsertPseudoFocusableView(
      item, QuickInsertPseudoFocusDirection::kBackward, /*should_loop=*/false);
  return Contains(item_left_of) ? item_left_of : nullptr;
}

views::View* QuickInsertEmojiBarView::GetItemRightOf(views::View* item) {
  views::View* item_right_of = GetNextQuickInsertPseudoFocusableView(
      item, QuickInsertPseudoFocusDirection::kForward, /*should_loop=*/false);
  return Contains(item_right_of) ? item_right_of : nullptr;
}

bool QuickInsertEmojiBarView::ContainsItem(views::View* item) {
  return Contains(item);
}

void QuickInsertEmojiBarView::ClearSearchResults() {
  item_row_->RemoveAllChildViews();
}

void QuickInsertEmojiBarView::SetSearchResults(
    std::vector<QuickInsertEmojiResult> results) {
  ClearSearchResults();
  int item_row_width = 0;
  // This may be slow to calculate, so only `CHECK` on debug builds.
  DCHECK_EQ(item_row_width, item_row_->GetPreferredSize().width());
  const int available_item_row_width = CalculateAvailableWidthForItemRow();
  for (const auto& result : results) {
    // `base::Unretained` is safe here because `this` owns the item view.
    auto item_view = CreateItemView(
        result,
        base::BindRepeating(&QuickInsertEmojiBarView::SelectSearchResult,
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

size_t QuickInsertEmojiBarView::GetNumItems() const {
  return item_row_->children().size();
}

void QuickInsertEmojiBarView::SelectSearchResult(
    const QuickInsertSearchResult& result) {
  delegate_->SelectSearchResult(result);
}

void QuickInsertEmojiBarView::OpenMoreEmojis() {
  delegate_->ShowEmojiPicker(ui::EmojiPickerCategory::kEmojis);
}

void QuickInsertEmojiBarView::ToggleGifs(bool is_checked) {
  // `delegate_` might be null in tests.
  if (delegate_ != nullptr) {
    delegate_->ToggleGifs(is_checked);
  }
}

int QuickInsertEmojiBarView::CalculateAvailableWidthForItemRow() {
  return quick_insert_view_width_ - kEmojiBarMargins.width() -
         kItemRowAndGifsSpacing - gifs_button_->GetMaximumSize().width() -
         kGifsAndMoreEmojisSpacing -
         more_emojis_button_->GetPreferredSize().width();
}

views::View* QuickInsertEmojiBarView::GetLeftmostItem() {
  if (GetFocusManager() == nullptr) {
    return nullptr;
  }
  views::View* leftmost_item = GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), /*reverse=*/false,
      /*dont_loop=*/false);
  return Contains(leftmost_item) ? leftmost_item : nullptr;
}

views::View::Views QuickInsertEmojiBarView::GetItemsForTesting() const {
  views::View::Views items;
  for (views::View* child : item_row_->children()) {
    items.push_back(child->children().front());
  }
  return items;
}

BEGIN_METADATA(QuickInsertEmojiBarView)
END_METADATA

}  // namespace ash
