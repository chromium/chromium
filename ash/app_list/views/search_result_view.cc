// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "ash/app_list/views/legacy_remove_query_confirmation_dialog.h"
#include "ash/app_list/views/remove_query_confirmation_dialog.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_actions_view.h"
#include "ash/app_list/views/search_result_inline_icon_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
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
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/image_model_utils.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"

namespace ash {

namespace {

constexpr int kBadgeIconShadowWidth = 1;
constexpr int kPreferredWidth = 640;
constexpr int kClassicViewHeight = 48;
constexpr int kDefaultViewHeight = 40;
constexpr int kAnswerCardViewHeight = 80;
constexpr int kKeyboardShortcutViewHeight = 64;
constexpr int kPreferredIconViewWidth = 56;
constexpr int kTextTrailPadding = 16;
// Extra margin at the right of the rightmost action icon.
constexpr int kClassicActionButtonRightMargin = 8;
// Extra margin at the right of the rightmost action icon.
constexpr int kDefaultActionButtonRightMargin = 12;
// Text line height in the search result.
constexpr int kPrimaryTextHeight = 20;
constexpr int kAnswerCardDetailsLineHeight = 18;

constexpr int kAnswerCardCardBackgroundCornerRadius = 12;
constexpr int kAnswerCardFocusBarOffset = 24;
constexpr int kAnswerCardFocusBarHeight = 32;

// Corner radius for downloaded image icons.
constexpr int kImageIconCornerRadius = 4;

constexpr int kSearchRatingStarPadding = 4;
constexpr int kSearchRatingStarSize = 16;
constexpr int kKeyboardShortcutTopMargin = 6;
constexpr int kAnswerCardBorderMargin = 12;
constexpr gfx::Insets kAnswerCardBorder(kAnswerCardBorderMargin,
                                        kAnswerCardBorderMargin,
                                        kAnswerCardBorderMargin,
                                        kAnswerCardBorderMargin);
constexpr gfx::Insets kBigTitleBorder(0, 0, 0, kAnswerCardBorderMargin);

views::ImageView* SetupChildImageView(views::FlexLayoutView* parent) {
  views::ImageView* image_view =
      parent->AddChildView(std::make_unique<views::ImageView>());
  image_view->SetCanProcessEventsWithinSubtree(false);
  image_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  image_view->SetVisible(false);
  return image_view;
}

views::Label* SetupChildLabelView(
    views::FlexLayoutView* parent,
    SearchResultView::SearchResultViewType view_type,
    SearchResultView::LabelType label_type) {
  // Create and setup label.
  views::Label* label = parent->AddChildView(std::make_unique<views::Label>());
  label->SetBackgroundColor(SK_ColorTRANSPARENT);
  label->SetVisible(false);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kScaleToMaximum));

  // Apply label text styling.
  label->SetTextContext(label_type == SearchResultView::LabelType::kBigTitle
                            ? CONTEXT_SEARCH_RESULT_BIG_TITLE
                            : CONTEXT_SEARCH_RESULT_VIEW);
  switch (view_type) {
    case SearchResultView::SearchResultViewType::kClassic:
      label->SetTextStyle(STYLE_CLASSIC_LAUNCHER);
      break;
    case SearchResultView::SearchResultViewType::kDefault:
    case SearchResultView::SearchResultViewType::kAnswerCard:
      label->SetTextStyle(STYLE_PRODUCTIVITY_LAUNCHER);
  }
  return label;
}

SearchResultInlineIconView* SetupChildInlineIconView(
    views::FlexLayoutView* parent) {
  SearchResultInlineIconView* inline_icon_view =
      parent->AddChildView(std::make_unique<SearchResultInlineIconView>());
  inline_icon_view->SetCanProcessEventsWithinSubtree(false);
  inline_icon_view->SetVisible(false);
  inline_icon_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kScaleToMaximum));
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

class SearchResultView::LabelAndTag {
 public:
  LabelAndTag(views::Label* label, SearchResult::Tags tags)
      : label_(label), tags_(tags) {}

  LabelAndTag(const LabelAndTag& other) = default;

  LabelAndTag& operator=(const LabelAndTag& other) = default;

  ~LabelAndTag() = default;

  views::Label* GetLabel() { return label_; }
  SearchResult::Tags GetTags() { return tags_; }

 private:
  views::Label* label_;  // Owned by views hierarchy.
  SearchResult::Tags tags_;
};

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

  // TODO(crbug.com/1218186): Remove this, this is in place temporarily to be
  // able to submit accessibility checks, but this focusable View needs to
  // add a name so that the screen reader knows what to announce.
  SetProperty(views::kSkipAccessibilityPaintChecks, true);
  SetCallback(base::BindRepeating(&SearchResultView::OnButtonPressed,
                                  base::Unretained(this)));

  icon_ = AddChildView(std::make_unique<MaskedImageView>());
  badge_icon_ = AddChildView(std::make_unique<views::ImageView>());
  auto* actions_view =
      AddChildView(std::make_unique<SearchResultActionsView>(this));
  set_actions_view(actions_view);

  icon_->SetCanProcessEventsWithinSubtree(false);
  badge_icon_->SetCanProcessEventsWithinSubtree(false);

  SetNotifyEnterExitOnChild(true);

  text_container_ = AddChildView(std::make_unique<views::FlexLayoutView>());
  // View contents are announced as part of the result view's accessible name.
  text_container_->GetViewAccessibility().OverrideIsLeaf(true);
  text_container_->GetViewAccessibility().OverrideIsIgnored(true);
  text_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  text_container_->SetOrientation(views::LayoutOrientation::kHorizontal);

  big_title_container_ =
      text_container_->AddChildView(std::make_unique<views::FlexLayoutView>());
  big_title_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  big_title_container_->SetBorder(views::CreateEmptyBorder(kBigTitleBorder));

  body_text_container_ =
      text_container_->AddChildView(std::make_unique<views::FlexLayoutView>());
  body_text_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  body_text_container_->SetOrientation(views::LayoutOrientation::kVertical);

  title_and_details_container_ = body_text_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  title_and_details_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStretch);
  SetSearchResultViewType(view_type_);

  keyboard_shortcut_container_ = body_text_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  keyboard_shortcut_container_->SetCrossAxisAlignment(
      views::LayoutAlignment::kStretch);
  keyboard_shortcut_container_->SetBorder(
      views::CreateEmptyBorder(kKeyboardShortcutTopMargin, 0, 0, 0));

  title_container_ = title_and_details_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  title_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  title_container_->SetOrientation(views::LayoutOrientation::kHorizontal);

  separator_label_ = SetupChildLabelView(title_and_details_container_,
                                         view_type_, LabelType::kDetails);
  separator_label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_SEPARATOR));

  details_container_ = title_and_details_container_->AddChildView(
      std::make_unique<views::FlexLayoutView>());
  details_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  details_container_->SetOrientation(views::LayoutOrientation::kHorizontal);

  rating_ = SetupChildLabelView(title_and_details_container_, view_type_,
                                LabelType::kDetails);

  rating_star_ = SetupChildImageView(title_and_details_container_);
  rating_star_->SetImage(gfx::CreateVectorIcon(
      kBadgeRatingIcon, kSearchRatingStarSize,
      AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
          kDeprecatedSearchBoxTextDefaultColor)));
  rating_star_->SetBorder(
      views::CreateEmptyBorder(0, kSearchRatingStarPadding, 0, 0));
}

SearchResultView::~SearchResultView() = default;

void SearchResultView::OnResultChanging(SearchResult* new_result) {
  if (result_changed_)
    return;
  if (!new_result || !result()) {
    result_changed_ = new_result;
    return;
  }
  result_changed_ = new_result->id() != result()->id();
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
      SetBorder(views::CreateEmptyBorder(gfx::Insets()));
      big_title_container_->RemoveAllChildViews();
      big_title_label_tags_.clear();
      big_title_container_->SetVisible(false);

      break;
    case SearchResultViewType::kClassic:
      title_and_details_container_->SetOrientation(
          views::LayoutOrientation::kVertical);
      SetBorder(views::CreateEmptyBorder(gfx::Insets()));
      big_title_container_->RemoveAllChildViews();
      big_title_label_tags_.clear();
      big_title_container_->SetVisible(false);

      break;
    case SearchResultViewType::kAnswerCard:
      title_and_details_container_->SetOrientation(
          views::LayoutOrientation::kVertical);
      SetBorder(views::CreateEmptyBorder(kAnswerCardBorder));
      break;
  }
}

views::LayoutOrientation SearchResultView::TitleAndDetailsOrientationForTest() {
  return title_and_details_container_->GetOrientation();
}

int SearchResultView::PreferredHeight() const {
  switch (view_type_) {
    case SearchResultViewType::kClassic:
      return kClassicViewHeight;
    case SearchResultViewType::kDefault:
      if (has_keyboard_shortcut_contents_)
        return kKeyboardShortcutViewHeight;
      return kDefaultViewHeight;
    case SearchResultViewType::kAnswerCard:
      return kAnswerCardViewHeight;
  }
}
int SearchResultView::PrimaryTextHeight() const {
  switch (view_type_) {
    case SearchResultViewType::kClassic:
    case SearchResultViewType::kDefault:
    case SearchResultViewType::kAnswerCard:
      return kPrimaryTextHeight;
  }
}
int SearchResultView::SecondaryTextHeight() const {
  switch (view_type_) {
    case SearchResultViewType::kClassic:
    case SearchResultViewType::kAnswerCard:
      return kAnswerCardDetailsLineHeight;
    case SearchResultViewType::kDefault:
      return kPrimaryTextHeight;
  }
}

int SearchResultView::ActionButtonRightMargin() const {
  switch (view_type_) {
    case SearchResultViewType::kClassic:
      return kClassicActionButtonRightMargin;
    case SearchResultViewType::kAnswerCard:
    case SearchResultViewType::kDefault:
      return kDefaultActionButtonRightMargin;
  }
}

bool SearchResultView::GetAndResetResultChanged() {
  bool result_changed = result_changed_;
  result_changed_ = false;
  return result_changed;
}

std::vector<SearchResultView::LabelAndTag>
SearchResultView::SetupContainerViewForTextVector(
    views::FlexLayoutView* parent,
    const std::vector<SearchResult::TextItem>& text_vector,
    LabelType label_type) {
  std::vector<LabelAndTag> label_tags;
  for (auto& span : text_vector) {
    switch (span.GetType()) {
      case SearchResultTextItemType::kString: {
        views::Label* label =
            SetupChildLabelView(parent, view_type_, label_type);
        if (label_type == LabelType::kDetails) {
          // We should only show a separator label when the details container
          // has valid contents.
          should_show_separator_label_ =
              should_show_separator_label_ || (!span.GetText().empty());
        }
        label->SetText(span.GetText());
        label->SetVisible(true);
        label_tags.push_back(LabelAndTag(label, span.GetTextTags()));
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

  gfx::ImageSkia badge_icon_skia = views::GetImageSkiaFromImageModel(
      result()->badge_icon(), GetColorProvider());

  if (result()->use_badge_icon_background()) {
    badge_icon_skia =
        CreateIconWithCircleBackground(badge_icon_skia, SK_ColorWHITE);
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
  big_title_container_->RemoveAllChildViews();
  big_title_label_tags_.clear();
  if (!result() || result()->big_title_text_vector().empty()) {
    big_title_container_->SetVisible(false);
  } else {
    // Create title labels from text vector metadata.
    big_title_label_tags_ = SetupContainerViewForTextVector(
        big_title_container_, result()->big_title_text_vector(),
        LabelType::kBigTitle);
    StyleBigTitleContainer();
    big_title_container_->SetVisible(true);
  }
}

void SearchResultView::UpdateTitleContainer() {
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
        title_container_, result()->title_text_vector(), LabelType::kTitle);
    StyleTitleContainer();
    text_container_->SetVisible(true);
    title_and_details_container_->SetVisible(true);
    title_container_->SetVisible(true);
  }
}

void SearchResultView::UpdateDetailsContainer() {
  should_show_separator_label_ = false;
  details_container_->RemoveAllChildViews();
  details_label_tags_.clear();
  if (!result() || result()->details_text_vector().empty()) {
    details_container_->SetVisible(false);
    separator_label_->SetVisible(false);
  } else {
    // Create details labels from text vector metadata.
    details_label_tags_ = SetupContainerViewForTextVector(
        details_container_, result()->details_text_vector(),
        LabelType::kDetails);
    StyleDetailsContainer();
    details_container_->SetVisible(true);
    switch (view_type_) {
      case SearchResultViewType::kDefault:
        // Show `separator_label_` when SetupContainerViewForTextVector gets
        // valid contents in `result()->details_text_vector()`.
        separator_label_->SetVisible(should_show_separator_label_);
        break;
      case SearchResultViewType::kClassic:
        separator_label_->SetVisible(false);
        break;
      case SearchResultViewType::kAnswerCard:
        // Show `separator_label_` when SetupContainerViewForTextVector gets
        // valid contents in `result()->details_text_vector()` and
        // `has_keyboard_shortcut_contents_` is set.
        separator_label_->SetVisible(should_show_separator_label_ &&
                                     has_keyboard_shortcut_contents_);
    }
  }
}

void SearchResultView::UpdateKeyboardShortcutContainer() {
  keyboard_shortcut_container_->RemoveAllChildViews();
  keyboard_shortcut_container_tags_.clear();

  DCHECK(view_type_ != SearchResultViewType::kClassic);
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
      case SearchResultViewType::kClassic:
      case SearchResultViewType::kAnswerCard:
        title_and_details_container_->SetOrientation(
            views::LayoutOrientation::kVertical);
        break;
    }
  } else {
    keyboard_shortcut_container_tags_ = SetupContainerViewForTextVector(
        keyboard_shortcut_container_, result()->keyboard_shortcut_text_vector(),
        LabelType::kKeyboardShortcut);
    StyleKeyboardShortcutContainer();
    keyboard_shortcut_container_->SetVisible(true);
    has_keyboard_shortcut_contents_ = true;
    // Override `title_and_details_container_` orientation if the keyboard
    // shortcut text vector has valid contents.
    title_and_details_container_->SetOrientation(
        views::LayoutOrientation::kHorizontal);
  }
}

void SearchResultView::UpdateRating() {
  if (!result() || !result()->rating() || result()->rating() < 0) {
    rating_->SetText(std::u16string());
    rating_->SetVisible(false);
    rating_star_->SetVisible(false);
    return;
  }

  rating_->SetText(base::FormatDouble(result()->rating(), 1));
  rating_->SetVisible(true);
  rating_star_->SetVisible(true);
}

void SearchResultView::StyleLabel(views::Label* label,
                                  bool is_title_label,
                                  const SearchResult::Tags& tags) {
  // Reset font weight styling for label.
  label->ApplyBaselineTextStyle();
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

    bool has_match_tag = (tag.styles & SearchResult::Tag::MATCH);
    if (has_match_tag) {
      switch (view_type_) {
        case SearchResultViewType::kClassic:
          label->SetTextStyleRange(AshTextStyle::STYLE_EMPHASIZED, tag.range);
          break;
        case SearchResultViewType::kDefault:
          ABSL_FALLTHROUGH_INTENDED;
        case SearchResultViewType::kAnswerCard:
          label->SetTextStyleRange(AshTextStyle::STYLE_HIGHLIGHT, tag.range);
          break;
      }
    }
  }

  switch (color_tag) {
    case SearchResult::Tag::NONE:
      ABSL_FALLTHROUGH_INTENDED;
    case SearchResult::Tag::DIM:
      ABSL_FALLTHROUGH_INTENDED;
    case SearchResult::Tag::MATCH:
      label->SetEnabledColor(
          is_title_label
              ? AppListColorProvider::Get()->GetSearchBoxTextColor(
                    kDeprecatedSearchBoxTextDefaultColor)
              : AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
                    kDeprecatedSearchBoxTextDefaultColor));
      break;
    case SearchResult::Tag::URL:
      label->SetEnabledColor(AppListColorProvider::Get()->GetTextColorURL());
      break;
    case SearchResult::Tag::GREEN:
      label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPositive));
      break;
    case SearchResult::Tag::RED:
      label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorAlert));
      break;
  }
}

void SearchResultView::StyleBigTitleContainer() {
  for (auto& span : big_title_label_tags_) {
    StyleLabel(span.GetLabel(), true /*is_title_label*/, span.GetTags());
  }
}

void SearchResultView::StyleTitleContainer() {
  for (auto& span : title_label_tags_) {
    StyleLabel(span.GetLabel(), true /*is_title_label*/, span.GetTags());
  }
}

void SearchResultView::StyleDetailsContainer() {
  for (auto& span : details_label_tags_) {
    StyleLabel(span.GetLabel(), false /*is_title_label*/, span.GetTags());
  }
}

void SearchResultView::StyleKeyboardShortcutContainer() {
  for (auto& span : keyboard_shortcut_container_tags_) {
    StyleLabel(span.GetLabel(), false /*is_title_label*/, span.GetTags());
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

  // Record different dialog action metric depending on productivity launcher
  // state - productivity launcher does not show zero-state search results, so
  // zero-state specific metric is not suitable. On the other hand, removal
  // action outside of zero-state search UI is only allowed if the productivity
  // launcher feature is on.
  if (features::IsProductivityLauncherEnabled()) {
    RecordSearchResultRemovalDialogDecision(
        accepted ? SearchResultRemovalConfirmation::kRemovalConfirmed
                 : SearchResultRemovalConfirmation::kRemovalCanceled);
  } else {
    RecordZeroStateSearchResultRemovalHistogram(
        accepted ? SearchResultRemovalConfirmation::kRemovalConfirmed
                 : SearchResultRemovalConfirmation::kRemovalCanceled);
  }
}

const char* SearchResultView::GetClassName() const {
  return kViewClassName;
}

gfx::Size SearchResultView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidth, PreferredHeight());
}

void SearchResultView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect icon_bounds(rect);

  int left_right_padding =
      (kPreferredIconViewWidth - icon_->GetImage().width()) / 2;
  int top_bottom_padding = (rect.height() - icon_->GetImage().height()) / 2;
  icon_bounds.set_width(kPreferredIconViewWidth);
  icon_bounds.Inset(left_right_padding, top_bottom_padding);
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
        break;
      }
      case SearchResultViewType::kClassic:
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

    switch (view_type_) {
      case SearchResultViewType::kDefault:
      case SearchResultViewType::kClassic:
        if (selected() && !actions_view()->HasSelectedAction()) {
          canvas->FillRect(
              content_rect,
              AppListColorProvider::Get()->GetSearchResultViewHighlightColor());
          PaintFocusBar(canvas, GetContentsBounds().origin(),
                        /*height=*/GetContentsBounds().height());
        }
        break;
      case SearchResultViewType::kAnswerCard: {
        cc::PaintFlags flags;
        flags.setAntiAlias(true);
        flags.setColor(
            AppListColorProvider::Get()->GetSearchResultViewHighlightColor());
        canvas->DrawRoundRect(content_rect,
                              kAnswerCardCardBackgroundCornerRadius, flags);
        if (selected()) {
          PaintFocusBar(canvas, gfx::Point(0, kAnswerCardFocusBarOffset),
                        kAnswerCardFocusBarHeight);
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
  node_data->SetName(GetAccessibleName());
}

void SearchResultView::VisibilityChanged(View* starting_from, bool is_visible) {
  NotifyAccessibilityEvent(ax::mojom::Event::kLayoutComplete, true);
}

void SearchResultView::OnThemeChanged() {
  if (!big_title_label_tags_.empty())
    StyleBigTitleContainer();
  if (!title_label_tags_.empty())
    StyleTitleContainer();
  if (!details_label_tags_.empty())
    StyleDetailsContainer();
  if (!keyboard_shortcut_container_tags_.empty())
    StyleKeyboardShortcutContainer();

  separator_label_->SetEnabledColor(
      AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
          kDeprecatedSearchBoxTextDefaultColor));
  rating_->SetEnabledColor(
      AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
          kDeprecatedSearchBoxTextDefaultColor));
  rating_star_->SetImage(gfx::CreateVectorIcon(
      kBadgeRatingIcon, kSearchRatingStarSize,
      AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
          kDeprecatedSearchBoxTextDefaultColor)));
  views::View::OnThemeChanged();
}

void SearchResultView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_LONG_PRESS:
      if (actions_view()->IsValidActionIndex(SearchResultActionType::kRemove)) {
        ScrollRectToVisible(GetLocalBounds());
        SetSelected(true, absl::nullopt);
        confirm_remove_by_long_press_ = true;
        OnSearchResultActionActivated(SearchResultActionType::kRemove);
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
  if (view_type_ == SearchResultViewType::kAnswerCard)
    UpdateBigTitleContainer();
  if (view_type_ != SearchResultViewType::kClassic &&
      app_list_features::IsSearchResultInlineIconEnabled()) {
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
      // Zero state suggestions are only available when productivity launcher
      // is not enabled, so don't record zero-state metric when the feature is
      // turned on.
      if (!features::IsProductivityLauncherEnabled()) {
        RecordZeroStateSearchResultUserActionHistogram(
            ZeroStateSearchResultUserActionType::kRemoveResult);
      }
      std::unique_ptr<views::WidgetDelegate> dialog;
      if (features::IsProductivityLauncherEnabled()) {
        dialog = std::make_unique<RemoveQueryConfirmationDialog>(
            base::BindOnce(&SearchResultView::OnQueryRemovalAccepted,
                           weak_ptr_factory_.GetWeakPtr()));
      } else {
        dialog = std::make_unique<LegacyRemoveQueryConfirmationDialog>(
            base::BindOnce(&SearchResultView::OnQueryRemovalAccepted,
                           weak_ptr_factory_.GetWeakPtr()));
      }
      dialog_controller_->Show(std::move(dialog));
      break;
    }
    case SearchResultActionType::kAppend:
      // Zero state suggestions are only available when productivity launcher
      // is not enabled, so don't record zero-state metric when the feature is
      // turned on.
      if (!features::IsProductivityLauncherEnabled()) {
        RecordZeroStateSearchResultUserActionHistogram(
            ZeroStateSearchResultUserActionType::kAppendResult);
      }
      list_view_->SearchResultActionActivated(this, button_action);
      break;
    case SearchResultActionType::kSearchResultActionTypeMax:
      NOTREACHED();
  }
}

bool SearchResultView::IsSearchResultHoveredOrSelected() {
  return IsMouseHovered() || selected();
}

}  // namespace ash
