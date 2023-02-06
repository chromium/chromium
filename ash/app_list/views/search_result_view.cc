// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/remove_query_confirmation_dialog.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_actions_view.h"
#include "ash/app_list/views/search_result_inline_icon_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"

namespace ash {

namespace {

constexpr int kBadgeIconShadowWidth = 1;
constexpr int kPreferredWidth = 640;
constexpr int kMultilineLabelWidth = 544;
constexpr int kDefaultViewHeight = 40;
constexpr int kDefaultAnswerCardViewHeight = 80;
constexpr int kKeyboardShortcutViewHeight = 64;
constexpr int kPreferredIconViewWidth = 56;
constexpr int kTextTrailPadding = 16;
// Extra margin at the right of the rightmost action icon.
constexpr int kDefaultActionButtonRightMargin = 12;
// Text line height in the search result.
constexpr int kPrimaryTextHeight = 20;
constexpr int kAnswerCardDetailsLineHeight = 18;

constexpr int kAnswerCardCardBackgroundCornerRadius = 12;
constexpr int kAnswerCardFocusBarHorizontalOffset = 12;
constexpr int kAnswerCardFocusBarVerticalOffset = 24;

// Corner radius for downloaded image icons.
constexpr int kImageIconCornerRadius = 4;

// The maximum number of lines that can be shown in the details text.
constexpr int kMultiLineLimit = 3;

// Flex layout orders detailing how container views are prioritized.
constexpr int kSeparatorOrder = 1;
constexpr int kRatingOrder = 1;
constexpr int TitleDetailContainerOrder = 1;
constexpr int kTitleDetailsLabelOrderNoElide = 1;
constexpr int kTitleDetailsLabelOrderElide = 2;
// Non-elidable labels are of order 1 to prioritize them.
constexpr int kNonElideLabelOrder = 1;
// Elidable labels are assigned monotonically increasing orders to prioritize
// items that appear close to the front of the TextVector.
constexpr int kElidableLabelOrderStart = 2;

constexpr int kSearchRatingStarPadding = 4;
constexpr int kSearchRatingStarSize = 16;
constexpr int kKeyboardShortcutTopMargin = 6;
constexpr int kAnswerCardBorderMargin = 12;
constexpr gfx::Insets kAnswerCardBorder(kAnswerCardBorderMargin);
// The superscript container has a 3px top margin to shift the text up so the
// it lines up with the text in `big_title_main_text_container_`.
constexpr auto kBigTitleSuperscriptBorder =
    gfx::Insets::TLBR(3, 4, 0, kAnswerCardBorderMargin);

// The fraction of total text space allocated to the details label when both the
// title and the details label need to be elided.
constexpr float kDetailsElideRatio = 0.25f;

bool IsTitleLabel(SearchResultView::LabelType label_type) {
  switch (label_type) {
    case SearchResultView::LabelType::kDetails:
    case SearchResultView::LabelType::kKeyboardShortcut:
      return false;
    case SearchResultView::LabelType::kTitle:
    case SearchResultView::LabelType::kBigTitle:
    case SearchResultView::LabelType::kBigTitleSuperscript:
      return true;
  }
}

ui::ColorId GetLabelColorId(bool is_title, const SearchResult::Tags& tags) {
  auto color_tag = SearchResult::Tag::NONE;
  for (const auto& tag : tags) {
    // Each label only supports one type of color tag. `color_tag` should only
    // be set once.
    if (tag.styles & SearchResult::Tag::URL) {
      DCHECK(color_tag == SearchResult::Tag::NONE ||
             color_tag == SearchResult::Tag::URL);
      color_tag = SearchResult::Tag::URL;
    }
    if (tag.styles & SearchResult::Tag::GREEN) {
      DCHECK(color_tag == SearchResult::Tag::NONE ||
             color_tag == SearchResult::Tag::GREEN);
      color_tag = SearchResult::Tag::GREEN;
    }
    if (tag.styles & SearchResult::Tag::RED) {
      DCHECK(color_tag == SearchResult::Tag::NONE ||
             color_tag == SearchResult::Tag::RED);
      color_tag = SearchResult::Tag::RED;
    }
  }

  switch (color_tag) {
    case SearchResult::Tag::NONE:
      ABSL_FALLTHROUGH_INTENDED;
    case SearchResult::Tag::DIM:
      ABSL_FALLTHROUGH_INTENDED;
    case SearchResult::Tag::MATCH:
      return is_title ? kColorAshTextColorPrimary : kColorAshTextColorSecondary;
    case SearchResult::Tag::URL:
      return kColorAshTextColorURL;
    case SearchResult::Tag::GREEN:
      return kColorAshTextColorPositive;
    case SearchResult::Tag::RED:
      return kColorAshTextColorAlert;
  }
}

views::ImageView* SetupChildImageView(views::FlexLayoutView* parent) {
  views::ImageView* image_view =
      parent->AddChildView(std::make_unique<views::ImageView>());
  image_view->GetViewAccessibility().OverrideIsIgnored(true);
  image_view->SetCanProcessEventsWithinSubtree(false);
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view->SetVisible(false);
  return image_view;
}

views::Label* SetupChildLabelView(
    views::FlexLayoutView* parent,
    SearchResultView::SearchResultViewType view_type,
    SearchResultView::LabelType label_type,
    ui::ColorId color_id,
    int flex_order,
    bool has_keyboard_shortcut_contents,
    bool is_multi_line,
    SearchResultTextItem::OverflowBehavior overflow_behavior) {
  // Create and setup label.
  views::Label* label = parent->AddChildView(std::make_unique<views::Label>());
  // Ignore labels for accessibility - the result accessible name is defined on
  // the whole result view.
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->GetViewAccessibility().OverrideIsIgnored(true);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColorId(color_id);
  label->SetVisible(false);
  label->SetElideBehavior(overflow_behavior ==
                                  SearchResultTextItem::OverflowBehavior::kElide
                              ? gfx::ELIDE_TAIL
                              : gfx::NO_ELIDE);
  label->SetMultiLine(is_multi_line);
  if (is_multi_line)
    label->SetMaxLines(kMultiLineLimit);

  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          overflow_behavior == SearchResultTextItem::OverflowBehavior::kHide
              ? views::MinimumFlexSizeRule::kPreferredSnapToZero
              : views::MinimumFlexSizeRule::kScaleToMinimum,
          views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(flex_order));

  // Apply label text styling.
  ash::AshTextContext text_context;
  switch (label_type) {
    case SearchResultView::LabelType::kBigTitle:
      text_context = CONTEXT_SEARCH_RESULT_BIG_TITLE;
      break;
    case SearchResultView::LabelType::kBigTitleSuperscript:
      // kBigTitleSuperscript labels are top-aligned to support superscripting.
      label->SetVerticalAlignment(gfx::ALIGN_TOP);
      text_context = CONTEXT_SEARCH_RESULT_BIG_TITLE_SUPERSCRIPT;
      break;
    case SearchResultView::LabelType::kTitle:
      text_context = CONTEXT_SEARCH_RESULT_VIEW;
      break;
    case SearchResultView::LabelType::kDetails:
      // has_keyboard_shortcut_contents forces inline title and details text for
      // answer cards so title and details text should use the same context.
      if (view_type == SearchResultView::SearchResultViewType::kAnswerCard &&
          !has_keyboard_shortcut_contents) {
        text_context = CONTEXT_SEARCH_RESULT_VIEW_INLINE_ANSWER_DETAILS;
      } else {
        text_context = CONTEXT_SEARCH_RESULT_VIEW;
      }
      break;
    case SearchResultView::LabelType::kKeyboardShortcut:
      text_context = CONTEXT_SEARCH_RESULT_VIEW;
      break;
  }
  label->SetTextContext(text_context);
  label->SetTextStyle(STYLE_LAUNCHER);
  return label;
}

SearchResultInlineIconView* SetupChildInlineIconView(
    views::FlexLayoutView* parent) {
  SearchResultInlineIconView* inline_icon_view =
      parent->AddChildView(std::make_unique<SearchResultInlineIconView>());
  inline_icon_view->SetCanProcessEventsWithinSubtree(false);
  inline_icon_view->GetViewAccessibility().OverrideIsIgnored(true);
  inline_icon_view->SetVisible(false);
  inline_icon_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
  return inline_icon_view;
}

}  // namespace

// static
const char SearchResultView::kViewClassName[] = "ui/app_list/SearchResultView";

// An ImageView that optionally masks the image into a circle or rectangle with
// rounded corners.
class MaskedImageView : public views::ImageView {
 public:
  MaskedImageView() = default;

  MaskedImageView(const MaskedImageView&) = delete;
  MaskedImageView& operator=(const MaskedImageView&) = delete;

  void set_shape(SearchResult::IconShape shape) {
    if (shape_ == shape)
      return;
    shape_ = shape;
    SchedulePaint();
  }

 protected:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override {
    SkPath mask;
    const gfx::Rect& bounds = GetImageBounds();

    switch (shape_) {
      case SearchResult::IconShape::kDefault:
      case SearchResult::IconShape::kRectangle:
        // Noop.
        break;
      case SearchResult::IconShape::kRoundedRectangle:
        mask.addRoundRect(gfx::RectToSkRect(bounds), kImageIconCornerRadius,
                          kImageIconCornerRadius);
        canvas->ClipPath(mask, true);
        break;
      case SearchResult::IconShape::kCircle:
        // Calculate the radius of the circle based on the minimum of width and
        // height in case the icon isn't square.
        mask.addCircle(bounds.x() + bounds.width() / 2,
                       bounds.y() + bounds.height() / 2,
                       std::min(bounds.width(), bounds.height()) / 2);
        canvas->ClipPath(mask, true);
        break;
    }

    ImageView::OnPaint(canvas);
  }

 private:
  SearchResult::IconShape shape_;
};

SearchResultView::LabelAndTag::LabelAndTag(views::Label* label,
                                           SearchResult::Tags tags)
    : label_(label), tags_(tags) {}

SearchResultView::LabelAndTag::LabelAndTag(
    const SearchResultView::LabelAndTag& other) = default;

SearchResultView::LabelAndTag& SearchResultView::LabelAndTag::operator=(
    const SearchResultView::LabelAndTag& other) = default;

SearchResultView::LabelAndTag::~LabelAndTag() = default;

SearchResultView::SearchResultView(
    SearchResultListView* list_view,
    AppListViewDelegate* view_delegate,
    SearchResultPageDialogController* dialog_controller,
    SearchResultViewType view_type)
    : list_view_(list_view),
      view_delegate_(view_delegate),
      dialog_controller_(dialog_controller),
      view_type_(view_type) {
  // Result views are not expected to be focused - while the results UI is shown
  // the focus is kept within the `SearchBoxView`, which manages result
  // selection state in response to keyboard navigation keys, and forwards
  // all relevant key events (e.g. ENTER key for result activation) to search
  // result views as needed.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);

  SetCallback(base::BindRepeating(&SearchResultView::OnButtonPressed,
                                  base::Unretained(this)));

  icon_ = AddChildView(std::make_unique<MaskedImageView>());
  badge_icon_ = AddChildView(std::make_unique<views::ImageView>());
  auto* actions_view =
      AddChildView(std::make_unique<SearchResultActionsView>(this));
  set_actions_view(actions_view);

  icon_->SetCanProcessEventsWithinSubtree(false);
  icon_->GetViewAccessibility().OverrideIsIgnored(true);
  badge_icon_->SetCanProcessEventsWithinSubtree(false);
  badge_icon_->GetViewAccessibility().OverrideIsIgnored(true);

  SetNotifyEnterExitOnChild(true);

  text_container_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  text_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  text_container_->SetOrientation(views::LayoutOrientation::kHorizontal);
  text_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));

  big_title_container_ =
      text_container_->AddChildView(std::make_unique<views::FlexLayoutView>());
  big_title_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  big_title_container_->SetOrientation(views::LayoutOrientation::kHorizontal);

  big_title_main_text_container_ = big_title_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  big_title_main_text_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStretch);
  big_title_main_text_container_->SetOrientation(
      views::LayoutOrientation::kHorizontal);

  big_title_superscript_container_ = big_title_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  big_title_superscript_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStretch);
  big_title_superscript_container_->SetOrientation(
      views::LayoutOrientation::kHorizontal);

  body_text_container_ =
      text_container_->AddChildView(std::make_unique<views::FlexLayoutView>());
  body_text_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  body_text_container_->SetOrientation(views::LayoutOrientation::kVertical);
  body_text_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));

  title_and_details_container_ = body_text_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  title_and_details_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStretch);
  title_and_details_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));
  SetSearchResultViewType(view_type_);

  title_container_ = title_and_details_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  title_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  title_container_->SetOrientation(views::LayoutOrientation::kHorizontal);
  title_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(TitleDetailContainerOrder)
          .WithWeight(1));
  title_container_->SetFlexAllocationOrder(
      views::FlexAllocationOrder::kReverse);

  result_text_separator_label_ =
      SetupChildLabelView(title_and_details_container_, view_type_,
                          LabelType::kDetails, kColorAshTextColorSecondary,
                          kSeparatorOrder, has_keyboard_shortcut_contents_,
                          /*is_multi_line=*/false,
                          SearchResultTextItem::OverflowBehavior::kNoElide);
  result_text_separator_label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_SEPARATOR));
  result_text_separator_label_->GetViewAccessibility().OverrideIsIgnored(true);

  details_container_ = title_and_details_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  details_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  details_container_->SetOrientation(views::LayoutOrientation::kHorizontal);
  details_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true)
          .WithOrder(TitleDetailContainerOrder)
          .WithWeight(1));

  rating_separator_label_ =
      SetupChildLabelView(title_and_details_container_, view_type_,
                          LabelType::kDetails, kColorAshTextColorSecondary,
                          kSeparatorOrder, has_keyboard_shortcut_contents_,
                          /*is_multi_line=*/false,
                          SearchResultTextItem::OverflowBehavior::kNoElide);
  rating_separator_label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_SEPARATOR));
  rating_separator_label_->GetViewAccessibility().OverrideIsIgnored(true);

  rating_ =
      SetupChildLabelView(title_and_details_container_, view_type_,
                          LabelType::kDetails, kColorAshTextColorSecondary,
                          kRatingOrder, has_keyboard_shortcut_contents_,
                          /*is_multi_line=*/false,
                          SearchResultTextItem::OverflowBehavior::kNoElide);

  rating_star_ = SetupChildImageView(title_and_details_container_);
  rating_star_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(0, kSearchRatingStarPadding, 0, 0)));

  keyboard_shortcut_container_ = body_text_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  keyboard_shortcut_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStretch);
  keyboard_shortcut_container_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kKeyboardShortcutTopMargin, 0, 0, 0)));
}

SearchResultView::~SearchResultView() = default;

void SearchResultView::OnResultChanged() {
  OnMetadataChanged();
  SchedulePaint();
}

void SearchResultView::SetSearchResultViewType(SearchResultViewType type) {
  view_type_ = type;
  switch (view_type_) {
    case SearchResultViewType::kDefault:
      title_and_details_container_->SetOrientation(
          views::LayoutOrientation::kHorizontal);
      ClearBigTitleContainer();
      break;
    case SearchResultViewType::kAnswerCard:
      title_and_details_container_->SetOrientation(
          views::LayoutOrientation::kVertical);
      SetBorder(views::CreateEmptyBorder(kAnswerCardBorder));
      break;
  }
}

void SearchResultView::ClearBigTitleContainer() {
  SetBorder(views::CreateEmptyBorder(0));
  big_title_main_text_container_->RemoveAllChildViews();
  big_title_label_tags_.clear();
  big_title_main_text_container_->SetVisible(false);
  big_title_superscript_container_->RemoveAllChildViews();
  big_title_superscript_label_tags_.clear();
  big_title_superscript_container_->SetVisible(false);
  big_title_superscript_container_->SetBorder(views::CreateEmptyBorder(0));
}

views::LayoutOrientation SearchResultView::TitleAndDetailsOrientationForTest() {
  return title_and_details_container_->GetOrientation();
}

int SearchResultView::PreferredHeight() const {
  switch (view_type_) {
    case SearchResultViewType::kDefault:
      if (has_keyboard_shortcut_contents_)
        return kKeyboardShortcutViewHeight;
      return kDefaultViewHeight;
    case SearchResultViewType::kAnswerCard:
      int height = kDefaultAnswerCardViewHeight;
      if (multi_line_details_height_ > 0) {
        // kDefaultAnswerCardViewHeight is adjusted to accommodate multi-line
        // result's height. The assumed kAnswerCardDetailsLineHeight is replaced
        // with the multi-line label's height.
        height =
            height + multi_line_details_height_ - kAnswerCardDetailsLineHeight;
      }
      if (multi_line_title_height_ > 0) {
        // kDefaultAnswerCardViewHeight is adjusted to accommodate multi-line
        // title's height. The assumed kPrimaryTextHeight is replaced
        // with the multi-line label's height.
        height = height + multi_line_title_height_ - kPrimaryTextHeight;
      }
      return height;
  }
}

int SearchResultView::PrimaryTextHeight() const {
  if (multi_line_title_height_ > 0)
    return multi_line_title_height_;
  switch (view_type_) {
    case SearchResultViewType::kDefault:
    case SearchResultViewType::kAnswerCard:
      return kPrimaryTextHeight;
  }
}

int SearchResultView::SecondaryTextHeight() const {
  if (has_keyboard_shortcut_contents_)
    return kPrimaryTextHeight;
  if (multi_line_details_height_ > 0)
    return multi_line_details_height_;
  switch (view_type_) {
    case SearchResultViewType::kAnswerCard:
      return kAnswerCardDetailsLineHeight;
    case SearchResultViewType::kDefault:
      return kPrimaryTextHeight;
  }
}

int SearchResultView::ActionButtonRightMargin() const {
  return kDefaultActionButtonRightMargin;
}

// static
int SearchResultView::GetTargetTitleWidth(int total_width,
                                          int separator_width,
                                          int target_details_width) {
  // Allocate all remaining space to the title container.
  const int target_title_width =
      total_width - separator_width - target_details_width;
  DCHECK_GT(target_title_width, 0);
  return target_title_width;
}

// static
int SearchResultView::GetMinimumDetailsWidth(int total_width,
                                             int details_width,
                                             int details_no_elide_width) {
  // Calculate the minimum width for the title and details containers
  // assuming both will need eliding.
  // We must allocate enough space to show the no_elide text. Otherwise
  // show the entire details text up to total_width*kDetailsElideRatio.
  const int target_details_width =
      std::max(details_no_elide_width,
               std::min(static_cast<int>(total_width * kDetailsElideRatio),
                        details_width));
  DCHECK_GT(target_details_width, 0);
  return target_details_width;
}

// static
void SearchResultView::SetFlexBehaviorForTextContents(
    int total_width,
    int separator_width,
    int non_elided_details_width,
    views::FlexLayoutView* title_container,
    views::FlexLayoutView* details_container) {
  const int title_width = title_container->GetPreferredSize().width();
  const int details_width = details_container->GetPreferredSize().width();

  // If the result view has enough space to accommodate text contents at their
  // preferred size, we don't need to elide either view. Set their weights to 1
  // and order to `kTitleDetailsLabelOrderNoElide`.
  if (title_width + details_width + separator_width <= total_width) {
    title_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred)
            .WithOrder(kTitleDetailsLabelOrderNoElide)
            .WithWeight(1));
    details_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred)
            .WithOrder(kTitleDetailsLabelOrderNoElide)
            .WithWeight(1));
    return;
  }

  const int min_details_width = GetMinimumDetailsWidth(
      total_width, details_width, non_elided_details_width);

  // If the result view has enough space to layout details to it's minimum size
  // after laying out the title, we should only take away space from the details
  // view. We do this by setting the details view to a lower order
  // `kTitleDetailsLabelOrderElide`.
  if (total_width - separator_width - title_width >= min_details_width) {
    title_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred)
            .WithOrder(kTitleDetailsLabelOrderNoElide)
            .WithWeight(1));
    details_container->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred)
            .WithOrder(kTitleDetailsLabelOrderElide)
            .WithWeight(1));
    return;
  }

  // If excess space needs to be taken from both the title and details view
  // to accommodate the minimum details view width, set flex weights to properly
  // take away space from the title and details view.
  const int target_title_width =
      GetTargetTitleWidth(total_width, separator_width, min_details_width);

  // Flex weights are set based on the amount of space that needs to be removed
  // from an associated view because flex layout *takes space away* based on
  // weight when it is unable to accommodate the view's preferred size.
  // calculate the number of px we need to take away from the title/details
  // text.
  const int detail_extra_space = details_width - min_details_width;
  const int title_extra_space = title_width - target_title_width;
  title_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(kTitleDetailsLabelOrderElide)
          .WithWeight(std::max(title_extra_space, 0)));
  details_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(kTitleDetailsLabelOrderElide)
          .WithWeight(std::max(detail_extra_space, 0)));
}

std::vector<SearchResultView::LabelAndTag>
SearchResultView::SetupContainerViewForTextVector(
    views::FlexLayoutView* parent,
    const std::vector<SearchResult::TextItem>& text_vector,
    LabelType label_type,
    bool has_keyboard_shortcut_contents,
    bool is_multi_line) {
  std::vector<LabelAndTag> label_tags;
  int label_count = 0;
  for (auto& span : text_vector) {
    switch (span.GetType()) {
      case SearchResultTextItemType::kString: {
        bool elidable = span.GetOverflowBehavior() !=
                        SearchResultTextItem::OverflowBehavior::kNoElide;
        views::Label* label = SetupChildLabelView(
            parent, view_type_, label_type,
            GetLabelColorId(IsTitleLabel(label_type), span.GetTextTags()),
            !elidable ? kNonElideLabelOrder
                      : kElidableLabelOrderStart + label_count,
            has_keyboard_shortcut_contents,
            /*is_multi_line=*/is_multi_line, span.GetOverflowBehavior());
        // Elidable label orders are monotonically increasing. Adjust the order
        // of the label by the number of labels in this container.
        if (elidable) {
          ++label_count;
        }
        if (label_type == LabelType::kDetails) {
          // We should only show a separator label when the details container
          // has valid contents.
          should_show_result_text_separator_label_ =
              should_show_result_text_separator_label_ ||
              (!span.GetText().empty());
        }
        label->SetText(span.GetText());
        label->SetVisible(true);
        if (!elidable) {
          // Each search result can have up to one non-elided label in its
          // details text.
          DCHECK_EQ(label_type, LabelType::kDetails);
          non_elided_details_label_width_ = label->GetPreferredSize().width();
        }
        if (is_multi_line) {
          switch (label_type) {
            case LabelType::kDetails:
              multi_line_details_height_ =
                  label->GetHeightForWidth(kMultilineLabelWidth);
              break;
            case LabelType::kTitle:
              multi_line_title_height_ =
                  label->GetHeightForWidth(kMultilineLabelWidth);
              break;
            case LabelType::kBigTitle:
            case LabelType::kBigTitleSuperscript:
            case LabelType::kKeyboardShortcut:
              // Multiline behavior is not supported for these label types.
              break;
          }
        }

        label_tags.emplace_back(label, span.GetTextTags());
      } break;
      case SearchResultTextItemType::kIconifiedText: {
        SearchResultInlineIconView* iconified_text_view =
            SetupChildInlineIconView(parent);
        iconified_text_view->SetText(span.GetText());
        iconified_text_view->SetVisible(true);
      } break;
      case SearchResultTextItemType::kIconCode: {
        SearchResultInlineIconView* icon_view =
            SetupChildInlineIconView(parent);
        icon_view->SetIcon(*span.GetIconFromCode());
        icon_view->SetVisible(true);
      } break;
      case SearchResultTextItemType::kCustomImage:
        break;
    }
  }
  return label_tags;
}

void SearchResultView::UpdateBadgeIcon() {
  if (!result() || result()->badge_icon().IsEmpty()) {
    badge_icon_->SetVisible(false);
    return;
  }

  gfx::ImageSkia badge_icon_skia =
      result()->badge_icon().Rasterize(GetColorProvider());

  if (result()->use_badge_icon_background()) {
    badge_icon_skia = CreateIconWithCircleBackground(badge_icon_skia);
  }

  gfx::ImageSkia resized_badge_icon(
      gfx::ImageSkiaOperations::CreateResizedImage(
          badge_icon_skia, skia::ImageOperations::RESIZE_BEST,
          SharedAppListConfig::instance().search_list_badge_icon_size()));

  gfx::ShadowValues shadow_values;
  shadow_values.push_back(
      gfx::ShadowValue(gfx::Vector2d(0, kBadgeIconShadowWidth), 0,
                       SkColorSetARGB(0x33, 0, 0, 0)));
  shadow_values.push_back(
      gfx::ShadowValue(gfx::Vector2d(0, kBadgeIconShadowWidth), 2,
                       SkColorSetARGB(0x33, 0, 0, 0)));
  badge_icon_->SetImage(gfx::ImageSkiaOperations::CreateImageWithDropShadow(
      resized_badge_icon, shadow_values));
  badge_icon_->SetVisible(true);
}

void SearchResultView::UpdateBigTitleContainer() {
  DCHECK_EQ(view_type_, SearchResultViewType::kAnswerCard);
  // Big title is only shown for answer card views.
  big_title_main_text_container_->RemoveAllChildViews();
  big_title_label_tags_.clear();
  if (!result() || result()->big_title_text_vector().empty()) {
    big_title_main_text_container_->SetVisible(false);
  } else {
    // Create big title labels from text vector metadata.
    big_title_label_tags_ = SetupContainerViewForTextVector(
        big_title_main_text_container_, result()->big_title_text_vector(),
        LabelType::kBigTitle, has_keyboard_shortcut_contents_,
        /*is_multi_line=*/false);
    StyleBigTitleContainer();
    big_title_main_text_container_->SetVisible(true);
  }
}

void SearchResultView::UpdateBigTitleSuperscriptContainer() {
  DCHECK_EQ(view_type_, SearchResultViewType::kAnswerCard);
  // Big title superscript is only shown for answer card views.
  big_title_superscript_container_->RemoveAllChildViews();
  big_title_superscript_label_tags_.clear();
  if (!result() || result()->big_title_superscript_text_vector().empty()) {
    big_title_superscript_container_->SetVisible(false);
  } else {
    // Create big title superscript labels from text vector metadata.
    big_title_superscript_label_tags_ = SetupContainerViewForTextVector(
        big_title_superscript_container_,
        result()->big_title_superscript_text_vector(),
        LabelType::kBigTitleSuperscript, has_keyboard_shortcut_contents_,
        /*is_multi_line=*/false);
    StyleBigTitleSuperscriptContainer();
    big_title_superscript_container_->SetVisible(true);
    big_title_superscript_container_->SetBorder(
        views::CreateEmptyBorder(kBigTitleSuperscriptBorder));
  }
}

void SearchResultView::UpdateTitleContainer() {
  // Updating the title label should reset `multi_line_details_height_`.
  multi_line_title_height_ = 0;
  title_container_->RemoveAllChildViews();
  title_label_tags_.clear();
  if (!result() || result()->title_text_vector().empty()) {
    // The entire text container should be hidden when there is no title.
    text_container_->SetVisible(false);
    title_and_details_container_->SetVisible(false);
    title_container_->SetVisible(false);
  } else {
    // Create title labels from text vector metadata.
    title_label_tags_ = SetupContainerViewForTextVector(
        title_container_, result()->title_text_vector(), LabelType::kTitle,
        has_keyboard_shortcut_contents_,
        /*is_multi_line=*/result()->multiline_title());
    StyleTitleContainer();
    text_container_->SetVisible(true);
    title_and_details_container_->SetVisible(true);
    title_container_->SetVisible(true);
  }
}

void SearchResultView::UpdateDetailsContainer() {
  should_show_result_text_separator_label_ = false;
  // Updating the details label should reset `multi_line_details_height_` and
  // `non_elided_details_label_width_`.
  multi_line_details_height_ = 0;
  non_elided_details_label_width_ = 0;
  details_container_->RemoveAllChildViews();
  details_label_tags_.clear();

  // Hide details container for answer cards with multiline titles.
  bool hide_details_container_for_answer_card =
      view_type_ == SearchResultViewType::kAnswerCard &&
      multi_line_title_height_ > kPrimaryTextHeight;

  if (!result() || result()->details_text_vector().empty() ||
      hide_details_container_for_answer_card) {
    details_container_->SetVisible(false);
    result_text_separator_label_->SetVisible(false);
  } else {
    // Create details labels from text vector metadata.
    details_label_tags_ = SetupContainerViewForTextVector(
        details_container_, result()->details_text_vector(),
        LabelType::kDetails, has_keyboard_shortcut_contents_,
        /*is_multi_line=*/result()->multiline_details());
    StyleDetailsContainer();
    details_container_->SetVisible(true);
    switch (view_type_) {
      case SearchResultViewType::kDefault:
        // Show `separator_label_` when SetupContainerViewForTextVector gets
        // valid contents in `result()->details_text_vector()`.
        result_text_separator_label_->SetVisible(
            should_show_result_text_separator_label_);
        break;
      case SearchResultViewType::kAnswerCard:
        // Show `separator_label_` when SetupContainerViewForTextVector gets
        // valid contents in `result()->details_text_vector()` and
        // `has_keyboard_shortcut_contents_` is set.
        result_text_separator_label_->SetVisible(
            should_show_result_text_separator_label_ &&
            has_keyboard_shortcut_contents_);
    }
  }
}

void SearchResultView::UpdateKeyboardShortcutContainer() {
  keyboard_shortcut_container_->RemoveAllChildViews();
  keyboard_shortcut_container_tags_.clear();

  if (!app_list_features::IsSearchResultInlineIconEnabled() || !result() ||
      result()->keyboard_shortcut_text_vector().empty()) {
    keyboard_shortcut_container_->SetVisible(false);
    has_keyboard_shortcut_contents_ = false;
    // Reset `title_and_details_container_` orientation.
    switch (view_type_) {
      case SearchResultViewType::kDefault:
        title_and_details_container_->SetOrientation(
            views::LayoutOrientation::kHorizontal);
        break;
      case SearchResultViewType::kAnswerCard:
        title_and_details_container_->SetOrientation(
            views::LayoutOrientation::kVertical);
        break;
    }
  } else {
    has_keyboard_shortcut_contents_ = true;
    keyboard_shortcut_container_tags_ = SetupContainerViewForTextVector(
        keyboard_shortcut_container_, result()->keyboard_shortcut_text_vector(),
        LabelType::kKeyboardShortcut, has_keyboard_shortcut_contents_,
        /*is_multi_line=*/false);
    StyleKeyboardShortcutContainer();
    keyboard_shortcut_container_->SetVisible(true);
    // Override `title_and_details_container_` orientation if the keyboard
    // shortcut text vector has valid contents.
    title_and_details_container_->SetOrientation(
        views::LayoutOrientation::kHorizontal);
  }
}

void SearchResultView::UpdateRating() {
  if (!result() || !result()->rating() || result()->rating() < 0) {
    rating_separator_label_->SetVisible(false);
    rating_->SetText(std::u16string());
    rating_->SetVisible(false);
    rating_star_->SetVisible(false);
    return;
  }

  rating_separator_label_->SetVisible(true);
  rating_->SetText(base::FormatDouble(result()->rating(), 1));
  rating_->SetVisible(true);
  rating_star_->SetVisible(true);
}

void SearchResultView::StyleLabel(views::Label* label,
                                  const SearchResult::Tags& tags) {
  // Reset font weight styling for label.
  label->ApplyBaselineTextStyle();

  for (const auto& tag : tags) {
    bool has_match_tag = (tag.styles & SearchResult::Tag::MATCH);
    if (has_match_tag)
      label->SetTextStyleRange(AshTextStyle::STYLE_HIGHLIGHT, tag.range);
  }
}

void SearchResultView::StyleBigTitleContainer() {
  for (auto& span : big_title_label_tags_) {
    StyleLabel(span.GetLabel(), span.GetTags());
  }
}

void SearchResultView::StyleBigTitleSuperscriptContainer() {
  for (auto& span : big_title_superscript_label_tags_) {
    StyleLabel(span.GetLabel(), span.GetTags());
  }
}

void SearchResultView::StyleTitleContainer() {
  for (auto& span : title_label_tags_) {
    StyleLabel(span.GetLabel(), span.GetTags());
  }
}

void SearchResultView::StyleDetailsContainer() {
  for (auto& span : details_label_tags_) {
    StyleLabel(span.GetLabel(), span.GetTags());
  }
}

void SearchResultView::StyleKeyboardShortcutContainer() {
  for (auto& span : keyboard_shortcut_container_tags_) {
    StyleLabel(span.GetLabel(), span.GetTags());
  }
}

void SearchResultView::OnQueryRemovalAccepted(bool accepted) {
  if (accepted) {
    list_view_->SearchResultActionActivated(this,
                                            SearchResultActionType::kRemove);
  }

  if (confirm_remove_by_long_press_) {
    confirm_remove_by_long_press_ = false;
    SetSelected(false, absl::nullopt);
  }

  RecordSearchResultRemovalDialogDecision(
      accepted ? SearchResultRemovalConfirmation::kRemovalConfirmed
               : SearchResultRemovalConfirmation::kRemovalCanceled);
}

void SearchResultView::OnSelectedResultChanged() {
  if (!selected())
    actions_view()->HideActions();
}

const char* SearchResultView::GetClassName() const {
  return kViewClassName;
}

gfx::Size SearchResultView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidth, PreferredHeight());
}

void SearchResultView::Layout() {
  // TODO(crbug/1311101) add test coverage for search result view layout.
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect icon_bounds(rect);

  int left_right_padding =
      (kPreferredIconViewWidth - icon_->GetImage().width()) / 2;
  int top_bottom_padding = (rect.height() - icon_->GetImage().height()) / 2;
  icon_bounds.set_width(kPreferredIconViewWidth);
  icon_bounds.Inset(gfx::Insets::VH(top_bottom_padding, left_right_padding));
  icon_bounds.Intersect(rect);
  icon_->SetBoundsRect(icon_bounds);

  gfx::Rect badge_icon_bounds;

  const int badge_icon_dimension =
      SharedAppListConfig::instance().search_list_badge_icon_dimension();
  badge_icon_bounds = gfx::Rect(icon_bounds.right() - badge_icon_dimension,
                                icon_bounds.bottom() - badge_icon_dimension,
                                badge_icon_dimension, badge_icon_dimension);
  badge_icon_bounds.Inset(-kBadgeIconShadowWidth);
  badge_icon_bounds.Intersect(rect);
  badge_icon_->SetBoundsRect(badge_icon_bounds);

  const int max_actions_width =
      (rect.right() - ActionButtonRightMargin() - icon_bounds.right()) / 2;
  int actions_width =
      std::min(max_actions_width, actions_view()->GetPreferredSize().width());

  gfx::Rect actions_bounds(rect);
  actions_bounds.set_x(rect.right() - ActionButtonRightMargin() -
                       actions_width);
  actions_bounds.set_width(actions_width);
  actions_view()->SetBoundsRect(actions_bounds);

  gfx::Rect text_bounds(rect);
  // Text bounds need to be shifted over by kAnswerCardBorderMargin for answer
  // card views to make room for the kAnswerCardBorder.
  text_bounds.set_x(kPreferredIconViewWidth +
                    (view_type_ == SearchResultViewType::kAnswerCard
                         ? kAnswerCardBorderMargin
                         : 0));
  if (actions_view()->GetVisible()) {
    text_bounds.set_width(
        rect.width() - kPreferredIconViewWidth - kTextTrailPadding -
        actions_view()->bounds().width() -
        (actions_view()->children().empty() ? 0 : ActionButtonRightMargin()));
  } else {
    text_bounds.set_width(rect.width() - kPreferredIconViewWidth -
                          kTextTrailPadding - ActionButtonRightMargin());
  }

  if (!title_label_tags_.empty() && !details_label_tags_.empty()) {
    switch (view_type_) {
      case SearchResultViewType::kDefault: {
        // SearchResultView needs additional space when
        // `has_keyboard_shortcut_contents_` is set to accommodate the
        // `keyboard_shortcut_container_`.
        gfx::Size label_size(
            text_bounds.width(),
            PrimaryTextHeight() +
                (has_keyboard_shortcut_contents_
                     ? kKeyboardShortcutTopMargin + SecondaryTextHeight()
                     : 0));
        gfx::Rect centered_text_bounds(text_bounds);
        centered_text_bounds.ClampToCenteredSize(label_size);
        text_container_->SetBoundsRect(centered_text_bounds);

        SetFlexBehaviorForTextContents(
            centered_text_bounds.width(),
            result_text_separator_label_->GetPreferredSize().width(),
            non_elided_details_label_width_, title_container_,
            details_container_);
        break;
      }

      case SearchResultViewType::kAnswerCard: {
        gfx::Size label_size(
            text_bounds.width(),
            PrimaryTextHeight() + SecondaryTextHeight() +
                (has_keyboard_shortcut_contents_ ? kKeyboardShortcutTopMargin
                                                 : 0));
        gfx::Rect centered_text_bounds(text_bounds);
        centered_text_bounds.ClampToCenteredSize(label_size);
        text_container_->SetBoundsRect(centered_text_bounds);
      }
    }
  } else if (!title_label_tags_.empty()) {
    gfx::Size text_size(text_bounds.width(), PrimaryTextHeight());
    if (view_type_ == SearchResultViewType::kAnswerCard &&
        has_keyboard_shortcut_contents_) {
      // Increase height for answer cards with keyboard shortcut contents.
      text_size.Enlarge(
          /*grow_width=*/0,
          /*grow_height=*/SecondaryTextHeight() + kKeyboardShortcutTopMargin);
    }
    gfx::Rect centered_text_bounds(text_bounds);
    centered_text_bounds.ClampToCenteredSize(text_size);
    text_container_->SetBoundsRect(centered_text_bounds);
  }
}

bool SearchResultView::OnKeyPressed(const ui::KeyEvent& event) {
  // result() could be null when result list is changing.
  if (!result())
    return false;

  switch (event.key_code()) {
    case ui::VKEY_RETURN:
      if (actions_view()->HasSelectedAction()) {
        OnSearchResultActionActivated(static_cast<SearchResultActionType>(
            actions_view()->GetSelectedAction()));
      } else {
        list_view_->SearchResultActivated(this, event.flags(),
                                          false /* by_button_press */);
      }
      return true;
    case ui::VKEY_DELETE:
    case ui::VKEY_BROWSER_BACK:
      // Allows alt+(back or delete) to trigger the 'remove result' dialog.
      OnSearchResultActionActivated(SearchResultActionType::kRemove);
      return true;
    default:
      return false;
  }
}

void SearchResultView::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect content_rect(rect);

  const SkColor focus_bar_color =
      GetColorProvider()->GetColor(ui::kColorAshFocusRing);
  const SkColor highlight_color =
      GetColorProvider()->GetColor(kColorAshHighlightColorHover);
  switch (view_type_) {
    case SearchResultViewType::kDefault:
      if (selected() && !actions_view()->HasSelectedAction()) {
        canvas->FillRect(content_rect, highlight_color);
        PaintFocusBar(canvas, GetContentsBounds().origin(),
                      /*height=*/GetContentsBounds().height(), focus_bar_color);
      }
      break;
    case SearchResultViewType::kAnswerCard: {
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(highlight_color);
      canvas->DrawRoundRect(content_rect, kAnswerCardCardBackgroundCornerRadius,
                            flags);
      if (selected()) {
        // Dynamically calculate the height of the answer card focus bar to
        // accommodate different heights for multi-line results.
        PaintFocusBar(canvas,
                      gfx::Point(kAnswerCardFocusBarHorizontalOffset,
                                 kAnswerCardFocusBarVerticalOffset),
                      PreferredHeight() -
                          kAnswerCardCardBackgroundCornerRadius * 2 -
                          kAnswerCardFocusBarVerticalOffset,
                      focus_bar_color);
      }
    } break;
  }
}

void SearchResultView::OnMouseEntered(const ui::MouseEvent& event) {
  actions_view()->UpdateButtonsOnStateChanged();
}

void SearchResultView::OnMouseExited(const ui::MouseEvent& event) {
  actions_view()->UpdateButtonsOnStateChanged();
}

void SearchResultView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (!GetVisible())
    return;

  // Mark the result is a list item in the list of search results.
  // Also avoids an issue with the nested button case(append and remove
  // button are child button of SearchResultView), which is not supported by
  // ChromeVox. see details in crbug.com/924776.
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);

  // It is possible for the view to be visible but lack a result. When this
  // happens, GetAccessibleName() will return an empty string. Because the
  // focusable state is set in the constructor and not updated when the
  // result is removed, the accessibility paint checks will fail.
  if (!result()) {
    node_data->SetNameExplicitlyEmpty();
    return;
  }

  node_data->SetName(GetAccessibleName());
}

void SearchResultView::VisibilityChanged(View* starting_from, bool is_visible) {
  NotifyAccessibilityEvent(ax::mojom::Event::kLayoutComplete, true);
}

void SearchResultView::OnThemeChanged() {
  views::View::OnThemeChanged();
  rating_star_->SetImage(gfx::CreateVectorIcon(
      kBadgeRatingIcon, kSearchRatingStarSize,
      GetColorProvider()->GetColor(kColorAshTextColorSecondary)));
  SchedulePaint();
}

void SearchResultView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_LONG_PRESS:
      if (actions_view()->IsValidActionIndex(SearchResultActionType::kRemove)) {
        ScrollRectToVisible(GetLocalBounds());
        SetSelected(true, absl::nullopt);
        confirm_remove_by_long_press_ = true;
        event->SetHandled();
      }
      break;
    default:
      break;
  }
  if (!event->handled())
    Button::OnGestureEvent(event);
}

void SearchResultView::OnMetadataChanged() {
  if (view_type_ == SearchResultViewType::kAnswerCard) {
    UpdateBigTitleContainer();
    UpdateBigTitleSuperscriptContainer();
  }
  if (app_list_features::IsSearchResultInlineIconEnabled()) {
    UpdateKeyboardShortcutContainer();
  }
  UpdateTitleContainer();
  UpdateDetailsContainer();
  UpdateAccessibleName();
  UpdateBadgeIcon();
  UpdateRating();
  // Updates |icon_|.
  // Note: this might leave the view with an old icon. But it is needed to avoid
  // flash when a SearchResult's icon is loaded asynchronously. In this case, it
  // looks nicer to keep the stale icon for a little while on screen instead of
  // clearing it out. It should work correctly as long as the SearchResult does
  // not forget to SetIcon when it's ready.
  if (result() && !result()->icon().icon.isNull()) {
    const SearchResult::IconInfo& icon_info = result()->icon();
    const gfx::ImageSkia& image = icon_info.icon;

    // Calculate the image dimensions. Images could be rectangular, and we
    // should preserve the aspect ratio.
    const size_t dimension = result()->icon().dimension;
    const int max = std::max(image.width(), image.height());
    const bool is_square = image.width() == image.height();
    const int width = is_square ? dimension : dimension * image.width() / max;
    const int height = is_square ? dimension : dimension * image.height() / max;
    SetIconImage(image, icon_, gfx::Size(width, height));
    icon_->set_shape(icon_info.shape);
  }

  // Updates |actions_view()|.
  actions_view()->SetActions(result() ? result()->actions()
                                      : SearchResult::Actions());
}

void SearchResultView::OnButtonPressed(const ui::Event& event) {
  list_view_->SearchResultActivated(this, event.flags(),
                                    true /* by_button_press */);
}

void SearchResultView::SetIconImage(const gfx::ImageSkia& source,
                                    views::ImageView* const icon,
                                    const gfx::Size& size) {
  gfx::ImageSkia image(source);
  image = gfx::ImageSkiaOperations::CreateResizedImage(
      source, skia::ImageOperations::RESIZE_BEST, size);
  icon->SetImage(image);
  icon->SetImageSize(size);
}

void SearchResultView::OnSearchResultActionActivated(size_t index) {
  // |result()| could be nullptr when result list is changing.
  if (!result())
    return;

  DCHECK_LT(index, result()->actions().size());

  SearchResultActionType button_action = result()->actions()[index].type;

  switch (button_action) {
    case SearchResultActionType::kRemove: {
      std::unique_ptr<views::WidgetDelegate> dialog =
          std::make_unique<RemoveQueryConfirmationDialog>(
              base::BindOnce(&SearchResultView::OnQueryRemovalAccepted,
                             weak_ptr_factory_.GetWeakPtr()),
              result()->title());
      dialog_controller_->Show(std::move(dialog));
      break;
    }
  }
}

bool SearchResultView::IsSearchResultHoveredOrSelected() {
  return IsMouseHovered() || selected();
}

}  // namespace ash
