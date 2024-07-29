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
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/app_shortcut_image.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"

namespace ash {

namespace {

constexpr int kBadgeIconShadowWidth = 1;
constexpr int kPreferredWidth = 640;
constexpr int kMultilineLabelWidth = 544;
constexpr int kDefaultViewHeight = 40;
constexpr int kKeyboardShortcutViewHeight = 64;
constexpr int kPreferredIconViewWidth = 56;
constexpr int kTextTrailPadding = 16;
// Extra margin at the right of the rightmost action icon.
constexpr int kDefaultActionButtonRightMargin = 12;
// Text line height in the search result.
constexpr int kPrimaryTextHeight = 20;
constexpr int kAnswerCardDetailsLineHeight = 18;

// Corner radius for downloaded image icons.
constexpr int kImageIconCornerRadius = 4;

// The maximum number of lines that can be shown in the details text.
constexpr int kMultiLineLimit = 3;

// For the progress bar.
constexpr int kProgressBarWidth = 536;
constexpr int kProgressBarHeight = 8;
constexpr int kBarChartAnswerCardVerticalUpperOffset = 8;
constexpr int kBarChartAnswerCardVerticalLowerOffset = 4;

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
constexpr int kAnswerCardBorderMargin = 16;
constexpr gfx::Insets kAnswerCardBorder(kAnswerCardBorderMargin);
constexpr int kDefaultAnswerCardViewHeight = 56 + 2 * kAnswerCardBorderMargin;

constexpr int kAnswerCardCardBackgroundCornerRadius = 12;
constexpr int kAnswerCardFocusBarHorizontalOffset = kAnswerCardBorderMargin;
constexpr int kAnswerCardFocusBarVerticalOffset =
    kAnswerCardCardBackgroundCornerRadius + kAnswerCardBorderMargin;

constexpr int kSearchListHostBadgeContainerDimension = 14;

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

ui::ColorId GetLabelColorId(SearchResultView::LabelType label_type,
                            const SearchResult::Tags& tags) {
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
      switch (label_type) {
        case SearchResultView::LabelType::kBigTitle:
        case SearchResultView::LabelType::kBigTitleSuperscript:
        case SearchResultView::LabelType::kTitle:
          return cros_tokens::kCrosSysOnSurface;
        case SearchResultView::LabelType::kDetails:
          return cros_tokens::kCrosSysOnSurfaceVariant;
        case SearchResultView::LabelType::kKeyboardShortcut:
          return cros_tokens::kCrosSysPrimary;
      }
      return IsTitleLabel(label_type) ? kColorAshTextColorPrimary
                                      : kColorAshTextColorSecondary;
    case SearchResult::Tag::URL:
      return cros_tokens::kCrosSysPrimary;
    case SearchResult::Tag::GREEN:
      return cros_tokens::kCrosSysPositive;
    case SearchResult::Tag::RED:
      return cros_tokens::kCrosSysError;
  }
}

std::optional<TypographyToken> GetTypographyToken(
    SearchResultView::LabelType label_type,
    bool is_match,
    bool is_inline_detail) {
  if (is_match) {
    return IsTitleLabel(label_type) ? TypographyToken::kCrosButton1
                                    : TypographyToken::kCrosBody1;
  }

  switch (label_type) {
    case SearchResultView::LabelType::kBigTitle:
      return TypographyToken::kCrosDisplay2;
    case SearchResultView::LabelType::kBigTitleSuperscript:
      return TypographyToken::kCrosDisplay7;
    case SearchResultView::LabelType::kTitle:
      return TypographyToken::kCrosBody1;
    case SearchResultView::LabelType::kDetails:
      // has_keyboard_shortcut_contents forces inline title and details text
      // for answer cards so title and details text should use the same
      // context.
      if (is_inline_detail) {
        return TypographyToken::kCrosBody1;
      }
      ABSL_FALLTHROUGH_INTENDED;
    case SearchResultView::LabelType::kKeyboardShortcut:
      return TypographyToken::kCrosAnnotation1;
  }

  return std::nullopt;
}

views::ImageView* SetupChildImageView(views::FlexLayoutView* parent) {
  views::ImageView* image_view =
      parent->AddChildView(std::make_unique<views::ImageView>());
  image_view->GetViewAccessibility().SetIsIgnored(true);
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
    std::optional<TypographyToken> typography_token,
    int flex_order,
    bool has_keyboard_shortcut_contents,
    bool is_multi_line,
    SearchResultTextItem::OverflowBehavior overflow_behavior) {
  // Create and setup label.
  views::Label* label = parent->AddChildView(std::make_unique<views::Label>());
  // Ignore labels for accessibility - the result accessible name is defined on
  // the whole result view.
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->GetViewAccessibility().SetIsIgnored(true);
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColorId(color_id);
  label->SetVisible(false);
  label->SetElideBehavior(overflow_behavior ==
                                  SearchResultTextItem::OverflowBehavior::kElide
                              ? gfx::ELIDE_TAIL
                              : gfx::NO_ELIDE);
  label->SetMultiLine(is_multi_line);
  if (is_multi_line) {
    label->SetMaxLines(kMultiLineLimit);
  }

  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          overflow_behavior == SearchResultTextItem::OverflowBehavior::kHide
              ? views::MinimumFlexSizeRule::kPreferredSnapToZero
              : views::MinimumFlexSizeRule::kScaleToMinimum,
          views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(flex_order));

  if (label_type == SearchResultView::LabelType::kBigTitleSuperscript) {
    // kBigTitleSuperscript labels are top-aligned to support superscripting.
    label->SetVerticalAlignment(gfx::ALIGN_TOP);
  }

  // Apply label text styling.
  if (typography_token.has_value()) {
    TypographyProvider::Get()->StyleLabel(typography_token.value(), *label);
  } else {
    ash::AshTextContext text_context;
    switch (label_type) {
      case SearchResultView::LabelType::kBigTitle:
        text_context = CONTEXT_SEARCH_RESULT_BIG_TITLE;
        break;
      case SearchResultView::LabelType::kBigTitleSuperscript:
        text_context = CONTEXT_SEARCH_RESULT_BIG_TITLE_SUPERSCRIPT;
        break;
      case SearchResultView::LabelType::kTitle:
        text_context = CONTEXT_SEARCH_RESULT_VIEW;
        break;
      case SearchResultView::LabelType::kDetails:
        // has_keyboard_shortcut_contents forces inline title and details text
        // for answer cards so title and details text should use the same
        // context.
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
  }

  return label;
}

views::ProgressBar* SetupChildProgressBarView(
    views::FlexLayoutView* parent,
    double value,
    std::optional<double> upper_warning_limit,
    std::optional<double> lower_warning_limit) {
  views::ProgressBar* progress_bar_view =
      parent->AddChildView(std::make_unique<views::ProgressBar>());
  progress_bar_view->GetViewAccessibility().SetIsIgnored(true);
  progress_bar_view->SetCanProcessEventsWithinSubtree(false);
  progress_bar_view->SetPreferredSize(
      gfx::Size(kProgressBarWidth, kProgressBarHeight));
  progress_bar_view->SizeToPreferredSize();
  progress_bar_view->SetValue(value);
  progress_bar_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/false));

  auto foreground_color =
      ((upper_warning_limit.has_value() &&
        value * 100 >= upper_warning_limit.value()) ||
       (lower_warning_limit.has_value() &&
        value * 100 <= lower_warning_limit.value()))
          ? kColorAshSystemInfoBarChartWarningColorForeground
          : kColorAshSystemInfoBarChartColorForeground;
  progress_bar_view->SetForegroundColorId(foreground_color);
  progress_bar_view->SetBackgroundColorId(
      kColorAshSystemInfoBarChartColorBackground);
  return progress_bar_view;
}

SearchResultInlineIconView* SetupChildInlineIconView(
    views::FlexLayoutView* parent,
    bool alterante_icon_and_text_styling) {
  SearchResultInlineIconView* inline_icon_view =
      parent->AddChildView(std::make_unique<SearchResultInlineIconView>(
          alterante_icon_and_text_styling));
  inline_icon_view->SetCanProcessEventsWithinSubtree(false);
  inline_icon_view->GetViewAccessibility().SetIsIgnored(true);
  inline_icon_view->SetVisible(false);
  inline_icon_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
  return inline_icon_view;
}

gfx::Rect GetIconViewBounds(const gfx::Rect& content_bounds,
                            int width,
                            int height) {
  gfx::Rect shortcut_bounds = content_bounds;
  shortcut_bounds.set_width(kPreferredIconViewWidth);
  shortcut_bounds.ClampToCenteredSize(gfx::Size(width, height));
  return shortcut_bounds;
}

}  // namespace

// An ImageView that optionally masks the image into a circle or rectangle with
// rounded corners.
class MaskedImageView : public views::ImageView {
  METADATA_HEADER(MaskedImageView, views::ImageView)

 public:
  MaskedImageView() = default;

  MaskedImageView(const MaskedImageView&) = delete;
  MaskedImageView& operator=(const MaskedImageView&) = delete;

  void set_shape(SearchResult::IconShape shape) {
    if (shape_ == shape) {
      return;
    }
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

BEGIN_METADATA(MaskedImageView)
END_METADATA

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
  SetCallback(base::BindRepeating(&SearchResultView::OnButtonPressed,
                                  base::Unretained(this)));

  icon_view_ = AddChildView(std::make_unique<MaskedImageView>());
  icon_view_->SetCanProcessEventsWithinSubtree(false);
  icon_view_->GetViewAccessibility().SetIsIgnored(true);

  badge_icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  badge_icon_view_->SetCanProcessEventsWithinSubtree(false);
  badge_icon_view_->GetViewAccessibility().SetIsIgnored(true);

  auto* actions_view =
      AddChildView(std::make_unique<SearchResultActionsView>(this));
  set_actions_view(actions_view);

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

  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace
  body_text_container_->SetLayoutManagerUseConstrainedSpace(false);

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

  progress_bar_container_ = title_and_details_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  progress_bar_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStretch);
  progress_bar_container_->SetOrientation(
      views::LayoutOrientation::kHorizontal);
  progress_bar_container_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kBarChartAnswerCardVerticalUpperOffset, 0,
                        kBarChartAnswerCardVerticalLowerOffset, 0)));

  system_details_container_ = title_and_details_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  system_details_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStretch);
  system_details_container_->SetOrientation(
      views::LayoutOrientation::kHorizontal);
  system_details_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kScaleToMaximum));

  left_details_container_ = system_details_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  left_details_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStretch);
  left_details_container_->SetOrientation(
      views::LayoutOrientation::kHorizontal);
  left_details_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(TitleDetailContainerOrder)
          .WithWeight(1));
  left_details_container_->SetMainAxisAlignment(views::LayoutAlignment::kStart);

  right_details_container_ = system_details_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  right_details_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStretch);
  right_details_container_->SetMainAxisAlignment(views::LayoutAlignment::kEnd);
  right_details_container_->SetOrientation(
      views::LayoutOrientation::kHorizontal);

  right_details_container_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(TitleDetailContainerOrder)
          .WithWeight(1));

  result_text_separator_label_ = SetupChildLabelView(
      title_and_details_container_, view_type_, LabelType::kDetails,
      kColorAshTextColorSecondary,
      GetTypographyToken(LabelType::kDetails, /*is_match=*/false,
                         IsInlineSearchResult()),
      kSeparatorOrder, has_keyboard_shortcut_contents_,
      /*is_multi_line=*/false,
      SearchResultTextItem::OverflowBehavior::kNoElide);
  result_text_separator_label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_SEPARATOR));
  result_text_separator_label_->GetViewAccessibility().SetIsIgnored(true);

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

  rating_separator_label_ = SetupChildLabelView(
      title_and_details_container_, view_type_, LabelType::kDetails,
      kColorAshTextColorSecondary,
      GetTypographyToken(LabelType::kDetails, /*is_match=*/false,
                         IsInlineSearchResult()),
      kSeparatorOrder, has_keyboard_shortcut_contents_,
      /*is_multi_line=*/false,
      SearchResultTextItem::OverflowBehavior::kNoElide);
  rating_separator_label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_SEPARATOR));
  rating_separator_label_->GetViewAccessibility().SetIsIgnored(true);

  rating_ = SetupChildLabelView(
      title_and_details_container_, view_type_, LabelType::kDetails,
      kColorAshTextColorSecondary,
      GetTypographyToken(LabelType::kDetails, /*is_match=*/false,
                         IsInlineSearchResult()),
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

bool SearchResultView::IsInlineSearchResult() {
  // has_keyboard_shortcut_contents_ forces inline title and details text for
  // answer cards.
  return view_type_ != SearchResultView::SearchResultViewType::kAnswerCard ||
         has_keyboard_shortcut_contents_;
}

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
      if (has_keyboard_shortcut_contents_) {
        return kKeyboardShortcutViewHeight;
      }
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
  if (multi_line_title_height_ > 0) {
    return multi_line_title_height_;
  }
  switch (view_type_) {
    case SearchResultViewType::kDefault:
    case SearchResultViewType::kAnswerCard:
      return kPrimaryTextHeight;
  }
}

int SearchResultView::SecondaryTextHeight() const {
  if (has_keyboard_shortcut_contents_) {
    return kPrimaryTextHeight;
  }
  if (multi_line_details_height_ > 0) {
    return multi_line_details_height_;
  }
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
            GetLabelColorId(label_type, span.GetTextTags()),
            GetTypographyToken(label_type, /*is_match=*/false,
                               IsInlineSearchResult()),
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
        // Text labels for keyboard shortcuts have additional left/right
        // padding.
        if (label_type == LabelType::kKeyboardShortcut) {
          label->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 6, 0, 6));
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
            SetupChildInlineIconView(parent,
                                     span.GetAlternateIconAndTextStyling());
        iconified_text_view->SetText(span.GetText());
        iconified_text_view->SetVisible(true);
      } break;
      case SearchResultTextItemType::kIconCode: {
        SearchResultInlineIconView* icon_view = SetupChildInlineIconView(
            parent, span.GetAlternateIconAndTextStyling());
        icon_view->SetIcon(*span.GetIconFromCode());
        icon_view->SetVisible(true);
      } break;
      case SearchResultTextItemType::kCustomImage:
        break;
    }
  }
  return label_tags;
}

void SearchResultView::UpdateIconAndBadgeIcon() {
  // Updates `icon_view_`.
  // Note: this might leave `icon_view_` with an old icon image. But it is
  // needed to avoid flash when a SearchResult's icon is loaded asynchronously.
  // In this case, it looks nicer to keep the stale icon for a little while on
  // screen instead of clearing it out. It should work correctly as long as the
  // SearchResult does not forget to SetIcon when it's ready.

  if (!result() || result()->icon().icon.IsEmpty()) {
    return;
  }

  const auto* color_provider = GetColorProvider();

  if (!GetColorProvider()) {
    return;
  }

  const SkColor background_color =
      color_provider->GetColor(cros_tokens::kCrosSysSystemOnBaseOpaque);
  const gfx::ImageSkia& icon_image =
      result()->icon().icon.Rasterize(color_provider);

  const gfx::Size icon_size = CalculateRegularIconImageSize(icon_image);

  if (result()->badge_icon().IsEmpty()) {
    SetIconImage(std::move(icon_image), icon_view_, std::move(icon_size));
    icon_view_->set_shape(result()->icon().shape);
    badge_icon_view_->SetVisible(false);
    return;
  }

  const gfx::Size badge_icon_size =
      gfx::Size(kSearchListHostBadgeContainerDimension,
                kSearchListHostBadgeContainerDimension);

  const gfx::ImageSkia& badge_icon_image =
      result()->badge_icon().Rasterize(color_provider);

  gfx::ImageSkia resized_badge_icon_image =
      gfx::ImageSkiaOperations::CreateResizedImage(
          badge_icon_image, skia::ImageOperations::RESIZE_BEST,
          badge_icon_size);

  if (result()->use_badge_icon_background()) {
    // Badge icon that isn't part of App Shortcuts needs to add an independent
    // halo if using background.
    gfx::ImageSkia badge_icon_with_background =
        gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
            kSearchListHostBadgeContainerDimension / 2, background_color,
            std::move(resized_badge_icon_image));
    badge_icon_view_->SetImage(std::move(badge_icon_with_background));
  } else {
    // Badge icon that isn't part of App Shortcuts or using background needs
    // to add shadows.
    gfx::ShadowValues shadow_values = {
        gfx::ShadowValue(gfx::Vector2d(0, kBadgeIconShadowWidth), 0,
                         SkColorSetARGB(0x33, 0, 0, 0)),
        gfx::ShadowValue(gfx::Vector2d(0, kBadgeIconShadowWidth), 2,
                         SkColorSetARGB(0x33, 0, 0, 0))};

    gfx::ImageSkia badge_icon_with_shadow =
        gfx::ImageSkiaOperations::CreateImageWithDropShadow(
            std::move(resized_badge_icon_image), std::move(shadow_values));
    badge_icon_view_->SetImage(std::move(badge_icon_with_shadow));
  }
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
  right_details_container_->RemoveAllChildViews();
  left_details_container_->RemoveAllChildViews();
  right_details_label_tags_.clear();

  // Hide details container for answer cards with multiline titles.
  bool hide_details_container_for_answer_card =
      view_type_ == SearchResultViewType::kAnswerCard &&
      multi_line_title_height_ > kPrimaryTextHeight;

  if (!result() || result()->details_text_vector().empty() ||
      hide_details_container_for_answer_card) {
    details_container_->SetVisible(false);
    result_text_separator_label_->SetVisible(false);
  } else if (result() && result()->has_extra_system_data_details()) {
    details_container_->SetVisible(false);
    details_label_tags_ = SetupContainerViewForTextVector(
        left_details_container_, result()->details_text_vector(),
        LabelType::kDetails, has_keyboard_shortcut_contents_,
        /*is_multi_line=*/result()->multiline_details());

    std::optional<std::u16string> right_details =
        result()->system_info_extra_details();
    ash::SearchResultTextItem text_item(SearchResultTextItemType::kString);
    text_item.SetText(right_details.value());
    text_item.SetTextTags({});

    right_details_label_tags_ = SetupContainerViewForTextVector(
        right_details_container_, {text_item}, LabelType::kDetails,
        has_keyboard_shortcut_contents_,
        /*is_multi_line=*/result()->multiline_details());
    StyleDetailsContainer();

    left_details_container_->SetVisible(true);
    system_details_container_->SetVisible(true);
    right_details_container_->SetVisible(true);

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

  if (!result() || result()->keyboard_shortcut_text_vector().empty()) {
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

void SearchResultView::UpdateProgressBarContainer() {
  progress_bar_container_->RemoveAllChildViews();
  if (result() && result()->is_system_info_card_bar_chart()) {
    is_progress_bar_answer_card_ = true;
    progress_bar_ = SetupChildProgressBarView(
        progress_bar_container_, result()->bar_chart_value().value() / 100.0,
        result()->upper_limit_for_bar_chart(),
        result()->lower_limit_for_bar_chart());
    text_container_->SetVisible(true);
    title_container_->SetVisible(false);
    title_and_details_container_->SetVisible(true);
    progress_bar_container_->SetVisible(true);
  } else {
    is_progress_bar_answer_card_ = false;
    progress_bar_container_->SetVisible(false);
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
  for (const auto& tag : tags) {
    bool has_match_tag = (tag.styles & SearchResult::Tag::MATCH);
    if (has_match_tag) {
      label->SetTextStyleRange(AshTextStyle::STYLE_HIGHLIGHT, tag.range);
    }
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
  if (result() && result()->has_extra_system_data_details()) {
    for (auto& span : right_details_label_tags_) {
      StyleLabel(span.GetLabel(), span.GetTags());
    }
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
    SetSelected(false, std::nullopt);
  }

  RecordSearchResultRemovalDialogDecision(
      accepted ? SearchResultRemovalConfirmation::kRemovalConfirmed
               : SearchResultRemovalConfirmation::kRemovalCanceled);
}

void SearchResultView::OnSelectedResultChanged() {
  if (!selected()) {
    actions_view()->HideActions();
  }
}

gfx::Size SearchResultView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kPreferredWidth, PreferredHeight());
}

gfx::Size SearchResultView::CalculateRegularIconImageSize(
    const gfx::ImageSkia& icon_image) const {
  // Calculate the icon image dimensions. Images could be rectangular, and we
  // should preserve the aspect ratio.
  const size_t dimension = result()->icon().dimension;
  const int max = std::max(icon_image.width(), icon_image.height());
  const bool is_square = icon_image.width() == icon_image.height();
  const int width =
      is_square ? dimension : dimension * icon_image.width() / max;
  const int height =
      is_square ? dimension : dimension * icon_image.height() / max;
  return gfx::Size(width, height);
}

gfx::Rect SearchResultView::GetIconBadgeViewBounds(
    const gfx::Rect& icon_view_bounds) const {
  const gfx::Size host_badge_container_view_size =
      gfx::Size(kSearchListHostBadgeContainerDimension,
                kSearchListHostBadgeContainerDimension);
  return gfx::Rect(icon_view_bounds.CenterPoint(),
                   std::move(host_badge_container_view_size));
}

void SearchResultView::Layout(PassKey) {
  // TODO(crbug.com/40220083) add test coverage for search result view layout.
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty()) {
    return;
  }

  icon_view_->SetBoundsRect(GetIconViewBounds(
      rect, icon_view_->GetImage().width(), icon_view_->GetImage().height()));

  badge_icon_view_->SetBoundsRect(GetIconBadgeViewBounds(icon_view_->bounds()));

  const int max_actions_width = (rect.right() - ActionButtonRightMargin() -
                                 (icon_view_->bounds()).right()) /
                                2;
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
            result_text_separator_label_
                ->GetPreferredSize(views::SizeBounds(
                    result_text_separator_label_->width(), {}))
                .width(),
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
  } else if (!details_label_tags_.empty() && is_progress_bar_answer_card_ &&
             result() && result()->is_system_info_card_bar_chart()) {
    gfx::Size label_size(text_bounds.width(),
                         PrimaryTextHeight() + SecondaryTextHeight());
    gfx::Rect centered_text_bounds(text_bounds);
    centered_text_bounds.ClampToCenteredSize(label_size);
    text_container_->SetBoundsRect(centered_text_bounds);
  }
}

bool SearchResultView::OnKeyPressed(const ui::KeyEvent& event) {
  // result() could be null when result list is changing.
  if (!result()) {
    return false;
  }

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
      if (!actions_view()->IsValidActionIndex(
              SearchResultActionType::kRemove)) {
        return false;
      }

      // Allows alt+(back or delete) to trigger the 'remove result' dialog.
      OnSearchResultActionActivated(SearchResultActionType::kRemove);
      return true;
    default:
      return false;
  }
}

void SearchResultView::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty()) {
    return;
  }

  gfx::Rect content_rect(rect);

  const SkColor focus_bar_color =
      GetColorProvider()->GetColor(cros_tokens::kCrosSysFocusRing);
  const SkColor highlight_color =
      GetColorProvider()->GetColor(cros_tokens::kCrosSysHoverOnSubtle);
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

void SearchResultView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateIconAndBadgeIcon();
  rating_star_->SetImage(gfx::CreateVectorIcon(
      kBadgeRatingIcon, kSearchRatingStarSize,
      GetColorProvider()->GetColor(kColorAshTextColorSecondary)));
  SchedulePaint();
}

void SearchResultView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::EventType::kGestureLongPress:
      if (actions_view()->IsValidActionIndex(SearchResultActionType::kRemove)) {
        ScrollRectToVisible(GetLocalBounds());
        SetSelected(true, std::nullopt);
        confirm_remove_by_long_press_ = true;
        event->SetHandled();
      }
      break;
    default:
      break;
  }
  if (!event->handled()) {
    Button::OnGestureEvent(event);
  }
}

void SearchResultView::OnMetadataChanged() {
  TRACE_EVENT0("ui", "SearchResultView::OnMetadataChanged");
  if (view_type_ == SearchResultViewType::kAnswerCard) {
    UpdateBigTitleContainer();
    UpdateBigTitleSuperscriptContainer();
  }
  UpdateKeyboardShortcutContainer();
  UpdateTitleContainer();
  UpdateProgressBarContainer();
  UpdateDetailsContainer();
  UpdateAccessibleName();
  UpdateRating();
  UpdateIconAndBadgeIcon();

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
  TRACE_EVENT0("ui", "SearchResultView::SetIconImage");
  gfx::ImageSkia image(source);
  image = gfx::ImageSkiaOperations::CreateResizedImage(
      source, skia::ImageOperations::RESIZE_BEST, size);
  icon->SetImage(image);
  icon->SetImageSize(size);
}

void SearchResultView::OnSearchResultActionActivated(size_t index) {
  // |result()| could be nullptr when result list is changing.
  if (!result()) {
    return;
  }

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

BEGIN_METADATA(SearchResultView)
END_METADATA

}  // namespace ash
