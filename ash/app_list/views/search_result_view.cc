// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_view.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
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
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/bind.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

namespace {

constexpr int kPreferredWidth = 640;
constexpr int kPreferredHeight = 48;
constexpr int kPreferredIconViewWidth = 56;
constexpr int kTextTrailPadding = 16;
// Extra margin at the right of the rightmost action icon.
constexpr int kActionButtonRightMargin = 8;
// Text line height in the search result.
constexpr int kTitleLineHeight = 20;
constexpr int kDetailsLineHeight = 16;

// URL color.
constexpr SkColor kUrlColor = gfx::kGoogleBlue600;
// Search result border color.
constexpr SkColor kResultBorderColor = SkColorSetARGB(0xFF, 0xE5, 0xE5, 0xE5);

// Delta applied to font size of all AppListSearchResult titles.
constexpr int kSearchResultTitleTextSizeDelta = 2;

}  // namespace

// static
const char SearchResultView::kViewClassName[] = "ui/app_list/SearchResultView";

SearchResultView::SearchResultView(SearchResultListView* list_view,
                                   AppListViewDelegate* view_delegate)
    : list_view_(list_view), view_delegate_(view_delegate) {
  SetFocusBehavior(FocusBehavior::ALWAYS);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  display_icon_ = AddChildView(std::make_unique<views::ImageView>());
  badge_icon_ = AddChildView(std::make_unique<views::ImageView>());
  auto* actions_view =
      AddChildView(std::make_unique<SearchResultActionsView>(this));
  set_actions_view(actions_view);

  icon_->SetCanProcessEventsWithinSubtree(false);
  display_icon_->SetCanProcessEventsWithinSubtree(false);
  SetDisplayIcon(gfx::ImageSkia());
  badge_icon_->SetCanProcessEventsWithinSubtree(false);

  set_context_menu_controller(this);
  SetNotifyEnterExitOnChild(true);
}

SearchResultView::~SearchResultView() = default;

void SearchResultView::OnResultChanged() {
  OnMetadataChanged();
  UpdateTitleText();
  UpdateDetailsText();
  SchedulePaint();
}

void SearchResultView::UpdateTitleText() {
  if (!result() || result()->title().empty())
    title_text_.reset();
  else
    CreateTitleRenderText();

  UpdateAccessibleName();
}

void SearchResultView::UpdateDetailsText() {
  if (!result() || result()->details().empty())
    details_text_.reset();
  else
    CreateDetailsRenderText();

  UpdateAccessibleName();
}

void SearchResultView::CreateTitleRenderText() {
  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  render_text->SetText(result()->title());
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  render_text->SetFontList(
      rb.GetFontList(AppListConfig::instance().search_result_title_font_style())
          .DeriveWithSizeDelta(kSearchResultTitleTextSizeDelta));
  // When result is an omnibox non-url search, the matched tag indicates
  // proposed query. For all other cases, the matched tag indicates typed search
  // query.
  render_text->SetColor(AppListColorProvider::Get()->GetSearchBoxTextColor(
      kDeprecatedSearchBoxTextDefaultColor));
  const SearchResult::Tags& tags = result()->title_tags();
  for (const auto& tag : tags) {
    if (tag.styles & SearchResult::Tag::URL) {
      render_text->ApplyColor(kUrlColor, tag.range);
    } else if (tag.styles & SearchResult::Tag::MATCH) {
      render_text->ApplyColor(
          AppListColorProvider::Get()->GetSearchBoxTextColor(
              kDeprecatedSearchBoxTextDefaultColor),
          tag.range);
    }
  }
  title_text_ = std::move(render_text);
}

void SearchResultView::CreateDetailsRenderText() {
  // Ensures single line row for omnibox non-url search result.
  if (result()->is_omnibox_search()) {
    details_text_.reset();
    return;
  }
  std::unique_ptr<gfx::RenderText> render_text =
      gfx::RenderText::CreateRenderText();
  render_text->SetText(result()->details());
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  render_text->SetFontList(rb.GetFontList(ui::ResourceBundle::BaseFont));
  render_text->SetColor(AppListColorProvider::Get()->GetSearchBoxTextColor(
      kDeprecatedSearchBoxTextDefaultColor));
  const SearchResult::Tags& tags = result()->details_tags();
  for (const auto& tag : tags) {
    if (tag.styles & SearchResult::Tag::URL)
      render_text->ApplyColor(kUrlColor, tag.range);
  }
  details_text_ = std::move(render_text);
}

void SearchResultView::OnQueryRemovalAccepted(int event_flags, bool accepted) {
  if (accepted) {
    list_view_->SearchResultActionActivated(
        this, OmniBoxZeroStateAction::kRemoveSuggestion, event_flags);
  }

  if (confirm_remove_by_long_press_) {
    confirm_remove_by_long_press_ = false;
    SetSelected(false, base::nullopt);
  }

  RecordZeroStateSearchResultRemovalHistogram(
      accepted ? ZeroStateSearchResutRemovalConfirmation::kRemovalConfirmed
               : ZeroStateSearchResutRemovalConfirmation::kRemovalCanceled);
}

const char* SearchResultView::GetClassName() const {
  return kViewClassName;
}

gfx::Size SearchResultView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidth, kPreferredHeight);
}

void SearchResultView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect icon_bounds(rect);

  const bool has_display_icon = !display_icon_->GetImage().isNull();
  views::ImageView* icon = has_display_icon ? display_icon_ : icon_;
  const int left_right_padding =
      (kPreferredIconViewWidth - icon->GetImage().width()) / 2;
  const int top_bottom_padding =
      (rect.height() - icon->GetImage().height()) / 2;
  icon_bounds.set_width(kPreferredIconViewWidth);
  icon_bounds.Inset(left_right_padding, top_bottom_padding);
  icon_bounds.Intersect(rect);
  icon->SetBoundsRect(icon_bounds);

  gfx::Rect badge_icon_bounds;

  const int badge_icon_dimension =
      AppListConfig::instance().search_list_badge_icon_dimension();
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
}

bool SearchResultView::OnKeyPressed(const ui::KeyEvent& event) {
  // result() could be null when result list is changing.
  if (!result())
    return false;

  switch (event.key_code()) {
    case ui::VKEY_RETURN:
      if (actions_view()->HasSelectedAction()) {
        OnSearchResultActionActivated(static_cast<OmniBoxZeroStateAction>(
                                          actions_view()->GetSelectedAction()),
                                      event.flags());
      } else {
        list_view_->SearchResultActivated(this, event.flags(),
                                          false /* by_button_press */);
      }
      return true;
    case ui::VKEY_DELETE:
    case ui::VKEY_BROWSER_BACK:
      // Allows alt+(back or delete) to trigger the 'remove result' dialog.
      OnSearchResultActionActivated(OmniBoxZeroStateAction::kRemoveSuggestion,
                                    event.flags());
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
  text_bounds.set_x(
      GetMirroredXWithWidthInView(text_bounds.x(), text_bounds.width()));

  // Set solid color background to avoid broken text. See crbug.com/746563.
  // This should be drawn before selected color which is semi-transparent.
  canvas->FillRect(
      text_bounds,
      AppListColorProvider::Get()->GetSearchBoxCardBackgroundColor());

  // Possibly call FillRect a second time (these colours are partially
  // transparent, so the previous FillRect is not redundant).
  if (selected() && !actions_view()->HasSelectedAction()) {
    canvas->FillRect(
        content_rect,
        AppListColorProvider::Get()->GetSearchResultViewHighlightColor());
  }

  gfx::Rect border_bottom = gfx::SubtractRects(rect, content_rect);
  canvas->FillRect(border_bottom, kResultBorderColor);

  if (title_text_ && details_text_) {
    gfx::Size title_size(text_bounds.width(), kTitleLineHeight);
    gfx::Size details_size(text_bounds.width(), kDetailsLineHeight);
    int total_height = title_size.height() + details_size.height();
    int y = text_bounds.y() + (text_bounds.height() - total_height) / 2;

    title_text_->SetDisplayRect(
        gfx::Rect(gfx::Point(text_bounds.x(), y), title_size));
    title_text_->Draw(canvas);

    y += title_size.height();
    details_text_->SetDisplayRect(
        gfx::Rect(gfx::Point(text_bounds.x(), y), details_size));
    details_text_->Draw(canvas);
  } else if (title_text_) {
    gfx::Size title_size(text_bounds.width(),
                         title_text_->GetStringSize().height());
    gfx::Rect centered_title_rect(text_bounds);
    centered_title_rect.ClampToCenteredSize(title_size);
    title_text_->SetDisplayRect(centered_title_rect);
    title_text_->Draw(canvas);
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
  node_data->AddState(ax::mojom::State::kFocusable);
  node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);
  node_data->SetName(GetAccessibleName());
}

void SearchResultView::VisibilityChanged(View* starting_from, bool is_visible) {
  NotifyAccessibilityEvent(ax::mojom::Event::kLayoutComplete, true);
}

void SearchResultView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_LONG_PRESS:
      if (actions_view()->IsValidActionIndex(
              OmniBoxZeroStateAction::kRemoveSuggestion)) {
        ScrollRectToVisible(GetLocalBounds());
        NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
        SetSelected(true, base::nullopt);
        confirm_remove_by_long_press_ = true;
        OnSearchResultActionActivated(OmniBoxZeroStateAction::kRemoveSuggestion,
                                      event->flags());
        event->SetHandled();
      }
      break;
    default:
      break;
  }
  if (!event->handled())
    Button::OnGestureEvent(event);
}

void SearchResultView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  DCHECK(sender == this);
  list_view_->SearchResultActivated(this, event.flags(),
                                    true /* by_button_press */);
}

void SearchResultView::OnMetadataChanged() {
  // Updates |icon_|.
  // Note: this might leave the view with an old icon. But it is needed to avoid
  // flash when a SearchResult's icon is loaded asynchronously. In this case, it
  // looks nicer to keep the stale icon for a little while on screen instead of
  // clearing it out. It should work correctly as long as the SearchResult does
  // not forget to SetIcon when it's ready.
  const gfx::ImageSkia icon(result() ? result()->icon() : gfx::ImageSkia());
  if (!icon.isNull())
    SetIconImage(icon, icon_,
                 AppListConfig::instance().search_list_icon_dimension());

  // Updates |badge_icon_|.
  const gfx::ImageSkia badge_icon(result() ? result()->badge_icon()
                                           : gfx::ImageSkia());
  if (badge_icon.isNull()) {
    badge_icon_->SetVisible(false);
  } else {
    SetIconImage(badge_icon, badge_icon_,
                 AppListConfig::instance().search_list_badge_icon_dimension());
    badge_icon_->SetVisible(true);
  }

  // Updates |actions_view()|.
  actions_view()->SetActions(result() ? result()->actions()
                                      : SearchResult::Actions());
}

void SearchResultView::SetIconImage(const gfx::ImageSkia& source,
                                    views::ImageView* const icon,
                                    const int icon_dimension) {
  gfx::ImageSkia image(source);
  image = gfx::ImageSkiaOperations::CreateResizedImage(
      source, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(icon_dimension, icon_dimension));
  icon->SetImage(image);
}

void SearchResultView::OnSearchResultActionActivated(size_t index,
                                                     int event_flags) {
  // |result()| could be nullptr when result list is changing.
  if (!result())
    return;

  DCHECK_LT(index, result()->actions().size());

  if (result()->is_omnibox_search()) {
    OmniBoxZeroStateAction button_action = GetOmniBoxZeroStateAction(index);

    if (button_action == OmniBoxZeroStateAction::kRemoveSuggestion) {
      RecordZeroStateSearchResultUserActionHistogram(
          ZeroStateSearchResultUserActionType::kRemoveResult);
      auto dialog = std::make_unique<RemoveQueryConfirmationDialog>(
          result()->title(),
          base::BindOnce(&SearchResultView::OnQueryRemovalAccepted,
                         weak_ptr_factory_.GetWeakPtr(), event_flags));
      list_view_->app_list_main_view()
          ->contents_view()
          ->search_results_page_view()
          ->ShowAnchoredDialog(std::move(dialog));
    } else if (button_action == OmniBoxZeroStateAction::kAppendSuggestion) {
      RecordZeroStateSearchResultUserActionHistogram(
          ZeroStateSearchResultUserActionType::kAppendResult);
      list_view_->SearchResultActionActivated(this, index, event_flags);
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
      view_delegate_->GetSearchModel()->tablet_mode());
  context_menu_->Run(gfx::Rect(point, gfx::Size()),
                     views::MenuAnchorPosition::kTopLeft,
                     views::MenuRunner::HAS_MNEMONICS);
  source->RequestFocus();
}

void SearchResultView::SetDisplayIcon(const gfx::ImageSkia& source) {
  display_icon_->SetImage(source);
  display_icon_->SetVisible(!source.isNull());
  icon_->SetVisible(source.isNull());
}

}  // namespace ash
