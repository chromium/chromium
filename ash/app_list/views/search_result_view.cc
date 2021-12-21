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

constexpr int kPreferredWidth = 640;
constexpr int kClassicViewHeight = 48;
constexpr int kDefaultViewHeight = 40;
constexpr int kAnswerCardViewHeight = 80;
constexpr int kPreferredIconViewWidth = 56;
constexpr int kTextTrailPadding = 16;
// Extra margin at the right of the rightmost action icon.
constexpr int kActionButtonRightMargin = 8;
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
constexpr gfx::Insets kAnswerCardBorder(12, 12, 12, 12);
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
  text_container_->GetViewAccessibility().OverrideIsIgnored(true);
  text_container_->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  SetSearchResultViewType(view_type);

  auto setup_flex_specifications = [](views::View* view) {
    view->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kScaleToMaximum));
  };

  title_label_ =
      text_container_->AddChildView(std::make_unique<views::Label>());
  title_label_->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_label_->SetVisible(false);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  setup_flex_specifications(title_label_);

  separator_label_ =
      text_container_->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_SEPARATOR)));
  separator_label_->SetBackgroundColor(SK_ColorTRANSPARENT);
  separator_label_->SetVisible(false);
  separator_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  setup_flex_specifications(separator_label_);

  details_label_ =
      text_container_->AddChildView(std::make_unique<views::Label>());
  details_label_->SetBackgroundColor(SK_ColorTRANSPARENT);
  details_label_->SetVisible(false);
  details_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  setup_flex_specifications(details_label_);

  rating_ = text_container_->AddChildView(std::make_unique<views::Label>());
  rating_->SetBackgroundColor(SK_ColorTRANSPARENT);
  rating_->SetVisible(false);
  rating_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  setup_flex_specifications(rating_);

  rating_star_ =
      text_container_->AddChildView(std::make_unique<views::ImageView>());
  rating_star_->SetCanProcessEventsWithinSubtree(false);
  rating_star_->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  rating_star_->SetVisible(false);
  rating_star_->SetImage(gfx::CreateVectorIcon(
      kBadgeRatingIcon, kSearchRatingStarSize,
      AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
          kDeprecatedSearchBoxTextDefaultColor)));
  rating_star_->SetBorder(
      views::CreateEmptyBorder(0, kSearchRatingStarPadding, 0, 0));

  title_label_->SetTextContext(CONTEXT_SEARCH_RESULT_VIEW);
  separator_label_->SetTextContext(CONTEXT_SEARCH_RESULT_VIEW);
  details_label_->SetTextContext(CONTEXT_SEARCH_RESULT_VIEW);
  rating_->SetTextContext(CONTEXT_SEARCH_RESULT_VIEW);
  switch (view_type_) {
    case SearchResultViewType::kClassic:
      title_label_->SetTextStyle(STYLE_CLASSIC_LAUNCHER);
      separator_label_->SetTextStyle(STYLE_CLASSIC_LAUNCHER);
      details_label_->SetTextStyle(STYLE_CLASSIC_LAUNCHER);
      rating_->SetTextStyle(STYLE_CLASSIC_LAUNCHER);
      break;
    case SearchResultViewType::kDefault:
    case SearchResultViewType::kAnswerCard:
      title_label_->SetTextStyle(STYLE_PRODUCTIVITY_LAUNCHER);
      separator_label_->SetTextStyle(STYLE_PRODUCTIVITY_LAUNCHER);
      details_label_->SetTextStyle(STYLE_PRODUCTIVITY_LAUNCHER);
      rating_->SetTextStyle(STYLE_PRODUCTIVITY_LAUNCHER);
  }
}

SearchResultView::~SearchResultView() = default;

void SearchResultView::OnResultChanged() {
  OnMetadataChanged();
  // Update tile, separator, and details text visibility.
  UpdateTitleText();
  UpdateDetailsText();
  UpdateRating();
  UpdateAccessibleName();
  SchedulePaint();
}

void SearchResultView::SetSearchResultViewType(SearchResultViewType type) {
  view_type_ = type;

  switch (view_type_) {
    case SearchResultViewType::kDefault:
      text_container_->SetOrientation(views::LayoutOrientation::kHorizontal);
      SetBorder(views::CreateEmptyBorder(gfx::Insets()));
      break;
    case SearchResultViewType::kClassic:
      text_container_->SetOrientation(views::LayoutOrientation::kVertical);
      SetBorder(views::CreateEmptyBorder(gfx::Insets()));
      break;
    case SearchResultViewType::kAnswerCard:
      text_container_->SetOrientation(views::LayoutOrientation::kVertical);
      SetBorder(views::CreateEmptyBorder(kAnswerCardBorder));
      break;
  }
}

views::LayoutOrientation SearchResultView::GetLayoutOrientationForTest() {
  return text_container_->GetOrientation();
}

int SearchResultView::PreferredHeight() const {
  switch (view_type_) {
    case SearchResultViewType::kClassic:
      return kClassicViewHeight;
    case SearchResultViewType::kDefault:
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

void SearchResultView::UpdateTitleText() {
  if (!result() || result()->title().empty()) {
    title_label_->SetText(std::u16string());
    text_container_->SetVisible(false);
    title_label_->SetVisible(false);
  } else {
    title_label_->SetText(result()->title());
    StyleTitleLabel();
    text_container_->SetVisible(true);
    title_label_->SetVisible(true);
  }
}

void SearchResultView::UpdateDetailsText() {
  if (!result() || result()->details().empty()) {
    details_label_->SetText(std::u16string());
    details_label_->SetVisible(false);
    separator_label_->SetVisible(false);
  } else {
    details_label_->SetText(result()->details());
    StyleDetailsLabel();
    details_label_->SetVisible(true);
    switch (view_type_) {
      case SearchResultViewType::kDefault:
        separator_label_->SetVisible(true);
        break;
      case SearchResultViewType::kClassic:
      case SearchResultViewType::kAnswerCard:

        separator_label_->SetVisible(false);
    }
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
  // Apply font weight styles.
  bool is_url = false;
  for (const auto& tag : tags) {
    bool has_url_tag = (tag.styles & SearchResult::Tag::URL);
    bool has_match_tag = (tag.styles & SearchResult::Tag::MATCH);
    is_url = has_url_tag || is_url;
    if (has_match_tag) {
      label->SetTextStyleRange(AshTextStyle::STYLE_EMPHASIZED, tag.range);
    }
  }
  // Apply font color styles.
  label->SetEnabledColor(
      is_url
          ? AppListColorProvider::Get()->GetTextColorURL()
          : is_title_label
                ? AppListColorProvider::Get()->GetSearchBoxTextColor(
                      kDeprecatedSearchBoxTextDefaultColor)
                : AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
                      kDeprecatedSearchBoxTextDefaultColor));
}

void SearchResultView::StyleTitleLabel() {
  StyleLabel(title_label_, true /*is_title_label*/, result()->title_tags());
}

void SearchResultView::StyleDetailsLabel() {
  StyleLabel(details_label_, false /*is_title_label*/,
             result()->details_tags());
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
  badge_icon_bounds = gfx::Rect(icon_bounds.right() - badge_icon_dimension / 2,
                                icon_bounds.bottom() - badge_icon_dimension / 2,
                                badge_icon_dimension, badge_icon_dimension);

  badge_icon_bounds.Intersect(rect);
  badge_icon_->SetBoundsRect(badge_icon_bounds);

  const int max_actions_width =
      (rect.right() - kActionButtonRightMargin - icon_bounds.right()) / 2;
  int actions_width =
      std::min(max_actions_width, actions_view()->GetPreferredSize().width());

  gfx::Rect actions_bounds(rect);
  actions_bounds.set_x(rect.right() - kActionButtonRightMargin - actions_width);
  actions_bounds.set_width(actions_width);
  actions_view()->SetBoundsRect(actions_bounds);

  gfx::Rect text_bounds(rect);
  text_bounds.set_x(kPreferredIconViewWidth);
  if (actions_view()->GetVisible()) {
    text_bounds.set_width(
        rect.width() - kPreferredIconViewWidth - kTextTrailPadding -
        actions_view()->bounds().width() -
        (actions_view()->children().empty() ? 0 : kActionButtonRightMargin));
  } else {
    text_bounds.set_width(rect.width() - kPreferredIconViewWidth -
                          kTextTrailPadding - kActionButtonRightMargin);
  }

  if (!title_label_->GetText().empty() && !details_label_->GetText().empty()) {
    switch (view_type_) {
      case SearchResultViewType::kDefault: {
        gfx::Size label_size(text_bounds.width(), PrimaryTextHeight());
        gfx::Rect centered_text_bounds(text_bounds);
        centered_text_bounds.ClampToCenteredSize(label_size);
        text_container_->SetBoundsRect(centered_text_bounds);
        break;
      }
      case SearchResultViewType::kClassic:
      case SearchResultViewType::kAnswerCard: {
        gfx::Size title_size(text_bounds.width(), PrimaryTextHeight());
        gfx::Size details_size(text_bounds.width(), SecondaryTextHeight());
        int total_height = title_size.height() + details_size.height();
        gfx::Size label_size(text_bounds.width(), total_height);
        gfx::Rect centered_text_bounds(text_bounds);
        centered_text_bounds.ClampToCenteredSize(label_size);
        text_container_->SetBoundsRect(centered_text_bounds);
      }
    }
  } else if (!title_label_->GetText().empty()) {
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

  if (selected() && !actions_view()->HasSelectedAction()) {
    switch (view_type_) {
      case SearchResultViewType::kDefault:
      case SearchResultViewType::kClassic:
        canvas->FillRect(
            content_rect,
            AppListColorProvider::Get()->GetSearchResultViewHighlightColor());
        PaintFocusBar(canvas, GetContentsBounds().origin(),
                      /*height=*/GetContentsBounds().height());
        break;
      case SearchResultViewType::kAnswerCard: {
        cc::PaintFlags flags;
        flags.setAntiAlias(true);
        flags.setColor(
            AppListColorProvider::Get()->GetSearchResultViewHighlightColor());
        canvas->DrawRoundRect(content_rect,
                              kAnswerCardCardBackgroundCornerRadius, flags);
        PaintFocusBar(canvas, gfx::Point(0, kAnswerCardFocusBarOffset),
                      kAnswerCardFocusBarHeight);
      } break;
    }
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
  if (result()) {
    if (!result()->title().empty())
      StyleTitleLabel();
    if (!result()->details().empty())
      StyleDetailsLabel();
  }
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
    const size_t dimension = result()->IconDimension();
    const int max = std::max(image.width(), image.height());
    const bool is_square = image.width() == image.height();
    const int width = is_square ? dimension : dimension * image.width() / max;
    const int height = is_square ? dimension : dimension * image.height() / max;
    SetIconImage(image, icon_, gfx::Size(width, height));
    icon_->set_shape(icon_info.shape);
  }

  // Updates |badge_icon_|.
  gfx::ImageSkia badge_icon_skia;
  if (result() && !result()->badge_icon().IsEmpty()) {
    const ui::ImageModel& badge_icon = result()->badge_icon();
    gfx::ImageSkia badge_icon_skia =
        views::GetImageSkiaFromImageModel(badge_icon, GetColorProvider());

    if (result()->use_badge_icon_background())
      badge_icon_skia =
          CreateIconWithCircleBackground(badge_icon_skia, SK_ColorWHITE);
  }

  if (badge_icon_skia.isNull()) {
    badge_icon_->SetVisible(false);
  } else {
    const int dimension =
        SharedAppListConfig::instance().search_list_badge_icon_dimension();
    SetIconImage(badge_icon_skia, badge_icon_, gfx::Size(dimension, dimension));
    badge_icon_->SetVisible(true);
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
            result()->title(),
            base::BindOnce(&SearchResultView::OnQueryRemovalAccepted,
                           weak_ptr_factory_.GetWeakPtr()));
      } else {
        dialog = std::make_unique<LegacyRemoveQueryConfirmationDialog>(
            result()->title(),
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
