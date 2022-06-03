// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_view.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/remove_query_confirmation_dialog.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_actions_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/image_model_utils.h"
#include "ui/views/style/typography.h"

namespace ash {

namespace {

constexpr int kPreferredWidth = 640;
constexpr int kClassicViewHeight = 48;
constexpr int kDefaultViewHeight = 40;
constexpr int kInlineAnswerViewHeight = 80;
constexpr int kPreferredIconViewWidth = 56;
constexpr int kTextTrailPadding = 16;
// Extra margin at the right of the rightmost action icon.
constexpr int kActionButtonRightMargin = 8;
// Text line height in the search result.
constexpr int kPrimaryTextHeight = 20;
constexpr int kInlineAnswerDetailsLineHeight = 18;

// Corner radius for downloaded image icons.
constexpr int kImageIconCornerRadius = 4;
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

SearchResultView::SearchResultView(SearchResultListView* list_view,
                                   AppListViewDelegate* view_delegate,
                                   SearchResultViewType view_type)
    : list_view_(list_view),
      view_delegate_(view_delegate),
      view_type_(view_type) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
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

  set_context_menu_controller(this);
  SetNotifyEnterExitOnChild(true);

  title_label_ = AddChildView(std::make_unique<views::StyledLabel>());
  title_label_->SetDisplayedOnBackgroundColor(SK_ColorTRANSPARENT);
  title_label_->SetVisible(false);

  details_label_ = AddChildView(std::make_unique<views::StyledLabel>());
  details_label_->SetDisplayedOnBackgroundColor(SK_ColorTRANSPARENT);
  details_label_->SetVisible(false);

  separator_label_ = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_SEARCH_RESULT_SEPARATOR),
      CONTEXT_SEARCH_RESULT_VIEW, STYLE_PRODUCTIVITY_LAUNCHER));
  separator_label_->SetBackgroundColor(SK_ColorTRANSPARENT);
  separator_label_->SetVisible(false);
  separator_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

SearchResultView::~SearchResultView() = default;

void SearchResultView::OnResultChanged() {
  OnMetadataChanged();
  UpdateTitleText();
  UpdateDetailsText();
  UpdateAccessibleName();
  SchedulePaint();
}

int SearchResultView::PreferredHeight() const {
  switch (view_type_) {
    case SearchResultViewType::kClassic:
      return kClassicViewHeight;
    case SearchResultViewType::kDefault:
      return kDefaultViewHeight;
    case SearchResultViewType::kInlineAnswer:
      return kInlineAnswerViewHeight;
  }
}
int SearchResultView::PrimaryTextHeight() const {
  switch (view_type_) {
    case SearchResultViewType::kClassic:
    case SearchResultViewType::kDefault:
    case SearchResultViewType::kInlineAnswer:
      return kPrimaryTextHeight;
  }
}
int SearchResultView::SecondaryTextHeight() const {
  switch (view_type_) {
    case SearchResultViewType::kClassic:
    case SearchResultViewType::kInlineAnswer:
      return kInlineAnswerDetailsLineHeight;
    case SearchResultViewType::kDefault:
      return kPrimaryTextHeight;
  }
}

void SearchResultView::UpdateTitleText() {
  if (!result() || result()->title().empty()) {
    title_label_->SetText(std::u16string());
  } else {
    title_label_->SetText(result()->title());
    StyleTitleLabel();
  }
}

void SearchResultView::UpdateDetailsText() {
  if (!result() || result()->details().empty()) {
    details_label_->SetText(std::u16string());
  } else {
    details_label_->SetText(result()->details());
    StyleDetailsLabel();
  }
}

void SearchResultView::StyleTitleLabel() {
  title_label_->ClearStyleRanges();
  views::StyledLabel::RangeStyleInfo title_style;

  switch (view_type_) {
    case SearchResultViewType::kClassic:
      title_label_->SetTextContext(CONTEXT_SEARCH_RESULT_VIEW);
      title_label_->SetDefaultTextStyle(STYLE_CLASSIC_LAUNCHER);
      break;
    case SearchResultViewType::kInlineAnswer:
    case SearchResultViewType::kDefault:
      title_label_->SetTextContext(CONTEXT_SEARCH_RESULT_VIEW);
      title_label_->SetDefaultTextStyle(STYLE_PRODUCTIVITY_LAUNCHER);
  }
  title_style.override_color =
      AppListColorProvider::Get()->GetSearchBoxTextColor(
          kDeprecatedSearchBoxTextDefaultColor);

  title_style.disable_line_wrapping = true;
  title_label_->AddStyleRange(gfx::Range(0, result()->title().size()),
                              title_style);

  // Apply styling options for title_label_.
  const SearchResult::Tags& tags = result()->title_tags();
  for (const auto& tag : tags) {
    if (tag.styles & SearchResult::Tag::URL) {
      views::StyledLabel::RangeStyleInfo url_text_color;
      url_text_color.override_color =
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kTextColorURL);
      title_label_->AddStyleRange(tag.range, url_text_color);
    }
    if (tag.styles & SearchResult::Tag::MATCH) {
      views::StyledLabel::RangeStyleInfo selected_text_bold;
      selected_text_bold.text_style = ash::AshTextStyle::STYLE_EMPHASIZED;
      title_label_->AddStyleRange(tag.range, selected_text_bold);
    }
  }
}

void SearchResultView::StyleDetailsLabel() {
  details_label_->ClearStyleRanges();
  views::StyledLabel::RangeStyleInfo details_style;
  switch (view_type_) {
    case SearchResultViewType::kClassic:
      details_label_->SetTextContext(CONTEXT_SEARCH_RESULT_VIEW);
      details_label_->SetDefaultTextStyle(STYLE_CLASSIC_LAUNCHER);
      break;
    case SearchResultViewType::kInlineAnswer:
      details_label_->SetTextContext(
          CONTEXT_SEARCH_RESULT_VIEW_INLINE_ANSWER_DETAILS);
      details_label_->SetDefaultTextStyle(STYLE_PRODUCTIVITY_LAUNCHER);
      break;
    case SearchResultViewType::kDefault:
      details_label_->SetTextContext(CONTEXT_SEARCH_RESULT_VIEW);
      details_label_->SetDefaultTextStyle(STYLE_PRODUCTIVITY_LAUNCHER);
  }
  details_style.override_color =
      AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
          kDeprecatedSearchBoxTextDefaultColor);
  details_style.disable_line_wrapping = true;
  details_label_->AddStyleRange(gfx::Range(0, details_label_->GetText().size()),
                                details_style);

  // Apply styling options for details_label_.
  const SearchResult::Tags& tags = result()->details_tags();
  for (const auto& tag : tags) {
    if (tag.styles & SearchResult::Tag::URL) {
      views::StyledLabel::RangeStyleInfo url_text_color;
      url_text_color.override_color =
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kTextColorURL);
      details_label_->AddStyleRange(tag.range, url_text_color);
    }
    if (tag.styles & SearchResult::Tag::MATCH) {
      views::StyledLabel::RangeStyleInfo selected_text_bold;
      selected_text_bold.text_style = ash::AshTextStyle::STYLE_EMPHASIZED;
      details_label_->AddStyleRange(tag.range, selected_text_bold);
    }
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

  RecordZeroStateSearchResultRemovalHistogram(
      accepted ? ZeroStateSearchResutRemovalConfirmation::kRemovalConfirmed
               : ZeroStateSearchResutRemovalConfirmation::kRemovalCanceled);
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
        gfx::Rect title_rect(text_bounds);
        title_rect.ClampToCenteredSize(label_size);
        title_label_->SetBoundsRect(title_rect);
        title_label_->SetVisible(true);

        // Create Separator label.
        int title_width = title_label_->CalculatePreferredSize().width();
        gfx::Rect separator_rect(text_bounds);
        separator_rect.ClampToCenteredSize(label_size);
        separator_rect.set_x(title_rect.x() + title_width);
        separator_rect.set_width(separator_rect.width() - title_width);
        separator_label_->SetBoundsRect(separator_rect);
        separator_label_->SetVisible(true);

        // Create details label shifted to the right.

        // TODO(yulunwu) Reimplement with a layout manager.
        int title_separator_width =
            title_width + separator_label_->CalculatePreferredSize().width();
        gfx::Rect details_rect(text_bounds);
        details_rect.ClampToCenteredSize(label_size);
        details_rect.set_x(details_rect.x() + title_separator_width);
        details_rect.set_width(details_rect.width() - title_separator_width);
        details_label_->SetBoundsRect(details_rect);
        details_label_->SetVisible(true);
        break;
      }
      case SearchResultViewType::kClassic:
      case SearchResultViewType::kInlineAnswer: {
        gfx::Size title_size(text_bounds.width(), PrimaryTextHeight());
        gfx::Size details_size(text_bounds.width(), SecondaryTextHeight());
        int total_height = title_size.height() + details_size.height();
        int y = text_bounds.y() + (text_bounds.height() - total_height) / 2;

        title_label_->SetBoundsRect(
            gfx::Rect(gfx::Point(text_bounds.x(), y), title_size));
        title_label_->SetVisible(true);

        y += title_size.height();
        details_label_->SetBoundsRect(
            gfx::Rect(gfx::Point(text_bounds.x(), y), details_size));
        details_label_->SetVisible(true);
        separator_label_->SetVisible(false);
      }
    }
  } else if (!title_label_->GetText().empty()) {
    gfx::Size title_size(text_bounds.width(), PrimaryTextHeight());
    gfx::Rect centered_title_rect(text_bounds);
    centered_title_rect.ClampToCenteredSize(title_size);
    title_label_->SetBoundsRect(centered_title_rect);
    title_label_->SetVisible(true);
    details_label_->SetVisible(false);
    separator_label_->SetVisible(false);
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

  // Possibly call FillRect a second time (these colours are partially
  // transparent, so the previous FillRect is not redundant).
  if (selected() && !actions_view()->HasSelectedAction()) {
    // Fill search result view row item.
    canvas->FillRect(
        content_rect,
        AppListColorProvider::Get()->GetSearchResultViewHighlightColor());
    PaintFocusBar(canvas, GetContentsBounds().origin(),
                  /*height=*/GetContentsBounds().height());
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
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, selected());
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

  if (result()->is_omnibox_search()) {
    SearchResultActionType button_action = GetSearchResultActionType(index);

    switch (button_action) {
      case SearchResultActionType::kRemove: {
        RecordZeroStateSearchResultUserActionHistogram(
            ZeroStateSearchResultUserActionType::kRemoveResult);
        auto dialog = std::make_unique<RemoveQueryConfirmationDialog>(
            result()->title(),
            base::BindOnce(&SearchResultView::OnQueryRemovalAccepted,
                           weak_ptr_factory_.GetWeakPtr()));
        list_view_->app_list_main_view()
            ->contents_view()
            ->search_result_page_view()
            ->ShowAnchoredDialog(std::move(dialog));
        break;
      }
      case SearchResultActionType::kAppend:
        RecordZeroStateSearchResultUserActionHistogram(
            ZeroStateSearchResultUserActionType::kAppendResult);
        list_view_->SearchResultActionActivated(this, button_action);
        break;
      case SearchResultActionType::kSearchResultActionTypeMax:
        NOTREACHED();
    }
  }
}

bool SearchResultView::IsSearchResultHoveredOrSelected() {
  return IsMouseHovered() || selected();
}

void SearchResultView::OnMenuClosed() {
  // Release menu since its menu model delegate (AppContextMenu) could be
  // released as a result of menu command execution.
  context_menu_.reset();
}

void SearchResultView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // |result()| could be nullptr when result list is changing.
  if (!result())
    return;

  view_delegate_->GetSearchResultContextMenuModel(
      result()->id(), base::BindOnce(&SearchResultView::OnGetContextMenu,
                                     weak_ptr_factory_.GetWeakPtr(), source,
                                     point, source_type));
}

void SearchResultView::OnGetContextMenu(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type,
    std::unique_ptr<ui::SimpleMenuModel> menu_model) {
  if (!menu_model || (context_menu_ && context_menu_->IsShowingMenu()))
    return;

  AppLaunchedMetricParams metric_params = {
      AppListLaunchedFrom::kLaunchedFromSearchBox,
      AppListLaunchType::kSearchResult};
  view_delegate_->GetAppLaunchedMetricParams(&metric_params);

  context_menu_ = std::make_unique<AppListMenuModelAdapter>(
      std::string(), std::move(menu_model), GetWidget(), source_type,
      metric_params, AppListMenuModelAdapter::SEARCH_RESULT,
      base::BindOnce(&SearchResultView::OnMenuClosed,
                     weak_ptr_factory_.GetWeakPtr()),
      view_delegate_->IsInTabletMode());
  context_menu_->Run(gfx::Rect(point, gfx::Size()),
                     views::MenuAnchorPosition::kTopLeft,
                     views::MenuRunner::HAS_MNEMONICS);
  source->RequestFocus();
}

bool SearchResultView::IsRichImage() const {
  return result() &&
         result()->omnibox_type() == SearchResultOmniboxDisplayType::kRichImage;
}

}  // namespace ash
