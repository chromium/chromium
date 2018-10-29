// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_view.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_actions_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/progress_bar.h"

namespace app_list {

namespace {

constexpr int kPreferredWidth = 640;
constexpr int kPreferredHeight = 48;
constexpr int kIconLeftRightPadding = 19;
constexpr int kTextTrailPadding = 16;
// Extra margin at the right of the rightmost action icon.
constexpr int kActionButtonRightMargin = 8;

// Matched text color, #000 87%.
constexpr SkColor kMatchedTextColor = SkColorSetARGB(0xDE, 0x00, 0x00, 0x00);
// Default text color, #000 54%.
constexpr SkColor kDefaultTextColor = SkColorSetARGB(0x8A, 0x00, 0x00, 0x00);
// URL color.
constexpr SkColor kUrlColor = SkColorSetARGB(0xFF, 0x33, 0x67, 0xD6);
// Row selected color, Google Grey 8%.
constexpr SkColor kRowHighlightedColor = SkColorSetA(gfx::kGoogleGrey900, 0x14);
// Search result border color.
constexpr SkColor kResultBorderColor = SkColorSetARGB(0xFF, 0xE5, 0xE5, 0xE5);

int GetIconViewWidth() {
  return AppListConfig::instance().search_list_icon_dimension() +
         2 * kIconLeftRightPadding;
}

}  // namespace

// static
const char SearchResultView::kViewClassName[] = "ui/app_list/SearchResultView";

SearchResultView::SearchResultView(SearchResultListView* list_view,
                                   AppListViewDelegate* view_delegate)
    : list_view_(list_view),
      view_delegate_(view_delegate),
      icon_(new views::ImageView),
      badge_icon_(new views::ImageView),
      actions_view_(new SearchResultActionsView(this)),
      progress_bar_(new views::ProgressBar),
      weak_ptr_factory_(this) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  icon_->set_can_process_events_within_subtree(false);
  badge_icon_->set_can_process_events_within_subtree(false);

  AddChildView(icon_);
  AddChildView(badge_icon_);
  AddChildView(actions_view_);
  AddChildView(progress_bar_);
  set_context_menu_controller(this);
}

SearchResultView::~SearchResultView() {
  ClearResultNoRepaint();
}

void SearchResultView::SetResult(SearchResult* result) {
  ClearResultNoRepaint();

  result_ = result;
  if (result_)
    result_->AddObserver(this);

  OnMetadataChanged();
  UpdateTitleText();
  UpdateDetailsText();
  OnIsInstallingChanged();
  OnPercentDownloadedChanged();
  SchedulePaint();
}

void SearchResultView::ClearResultNoRepaint() {
  if (result_)
    result_->RemoveObserver(this);
  result_ = NULL;
}

void SearchResultView::ClearSelectedAction() {
  actions_view_->SetSelectedAction(-1);
}

void SearchResultView::UpdateTitleText() {
  if (!result_ || result_->title().empty())
    title_text_.reset();
  else
    CreateTitleRenderText();

  UpdateAccessibleName();
}

void SearchResultView::UpdateDetailsText() {
  if (!result_ || result_->details().empty())
    details_text_.reset();
  else

    CreateDetailsRenderText();

  UpdateAccessibleName();
}

base::string16 SearchResultView::ComputeAccessibleName() const {
  if (!result_)
    return base::string16();

  base::string16 accessible_name = result_->title();
  if (!result_->title().empty() && !result_->details().empty())
    accessible_name += base::ASCIIToUTF16(", ");
  accessible_name += result_->details();

  return accessible_name;
}

void SearchResultView::UpdateAccessibleName() {
  SetAccessibleName(ComputeAccessibleName());
}

void SearchResultView::CreateTitleRenderText() {
  auto render_text = gfx::RenderText::CreateHarfBuzzInstance();
  render_text->SetText(result_->title());
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  render_text->SetFontList(
      rb.GetFontList(kSearchResultTitleFontStyle)
          .DeriveWithSizeDelta(kSearchResultTitleTextSizeDelta));
  // When result is an omnibox non-url search, the matched tag indicates
  // proposed query. For all other cases, the matched tag indicates typed search
  // query.
  render_text->SetColor(result_->is_omnibox_search() ? kDefaultTextColor
                                                     : kMatchedTextColor);
  const SearchResult::Tags& tags = result_->title_tags();
  for (const auto& tag : tags) {
    if (tag.styles & SearchResult::Tag::URL) {
      render_text->ApplyColor(kUrlColor, tag.range);
    } else if (tag.styles & SearchResult::Tag::MATCH) {
      render_text->ApplyColor(
          result_->is_omnibox_search() ? kMatchedTextColor : kDefaultTextColor,
          tag.range);
    }
  }
  title_text_ = std::move(render_text);
}

void SearchResultView::CreateDetailsRenderText() {
  // Ensures single line row for omnibox non-url search result.
  if (result_->is_omnibox_search()) {
    details_text_.reset();
    return;
  }
  auto render_text = gfx::RenderText::CreateHarfBuzzInstance();
  render_text->SetText(result_->details());
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  render_text->SetFontList(rb.GetFontList(ui::ResourceBundle::BaseFont));
  render_text->SetColor(kDefaultTextColor);
  const SearchResult::Tags& tags = result_->details_tags();
  for (const auto& tag : tags) {
    if (tag.styles & SearchResult::Tag::URL)
      render_text->ApplyColor(kUrlColor, tag.range);
  }
  details_text_ = std::move(render_text);
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
  icon_bounds.set_width(GetIconViewWidth());
  const int top_bottom_padding =
      (rect.height() - AppListConfig::instance().search_list_icon_dimension()) /
      2;
  icon_bounds.Inset(kIconLeftRightPadding, top_bottom_padding,
                    kIconLeftRightPadding, top_bottom_padding);
  icon_bounds.Intersect(rect);
  icon_->SetBoundsRect(icon_bounds);

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
      std::min(max_actions_width, actions_view_->GetPreferredSize().width());

  gfx::Rect actions_bounds(rect);
  actions_bounds.set_x(rect.right() - kActionButtonRightMargin - actions_width);
  actions_bounds.set_width(actions_width);
  actions_view_->SetBoundsRect(actions_bounds);

  const int progress_width = rect.width() / 5;
  const int progress_height = progress_bar_->GetPreferredSize().height();
  const gfx::Rect progress_bounds(
      rect.right() - kActionButtonRightMargin - progress_width,
      rect.y() + (rect.height() - progress_height) / 2, progress_width,
      progress_height);
  progress_bar_->SetBoundsRect(progress_bounds);
}

bool SearchResultView::OnKeyPressed(const ui::KeyEvent& event) {
  // |result_| could be NULL when result list is changing.
  if (!result_)
    return false;

  switch (event.key_code()) {
    case ui::VKEY_RETURN: {
      int selected = actions_view_->selected_action();
      if (actions_view_->IsValidActionIndex(selected)) {
        OnSearchResultActionActivated(selected, event.flags());
      } else {
        list_view_->SearchResultActivated(this, event.flags());
      }
      return true;
    }
    default:
      break;
  }

  return false;
}

void SearchResultView::ChildPreferredSizeChanged(views::View* child) {
  Layout();
}

void SearchResultView::PaintButtonContents(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  gfx::Rect content_rect(rect);
  gfx::Rect text_bounds(rect);
  text_bounds.set_x(GetIconViewWidth());
  if (actions_view_->visible()) {
    text_bounds.set_width(
        rect.width() - GetIconViewWidth() - kTextTrailPadding -
        actions_view_->bounds().width() -
        (actions_view_->has_children() ? kActionButtonRightMargin : 0));
  } else {
    text_bounds.set_width(rect.width() - GetIconViewWidth() -
                          kTextTrailPadding - progress_bar_->bounds().width() -
                          kActionButtonRightMargin);
  }
  text_bounds.set_x(
      GetMirroredXWithWidthInView(text_bounds.x(), text_bounds.width()));

  // Set solid color background to avoid broken text. See crbug.com/746563.
  // This should be drawn before selected color which is semi-transparent.
  canvas->FillRect(text_bounds, kCardBackgroundColor);

  // Possibly call FillRect a second time (these colours are partially
  // transparent, so the previous FillRect is not redundant).
  if (background_highlighted())
    canvas->FillRect(content_rect, kRowHighlightedColor);

  gfx::Rect border_bottom = gfx::SubtractRects(rect, content_rect);
  canvas->FillRect(border_bottom, kResultBorderColor);

  if (title_text_ && details_text_) {
    gfx::Size title_size(text_bounds.width(),
                         title_text_->GetStringSize().height());
    gfx::Size details_size(text_bounds.width(),
                           details_text_->GetStringSize().height());
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

void SearchResultView::OnFocus() {
  ScrollRectToVisible(GetLocalBounds());
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
  SetBackgroundHighlighted(true);
}

void SearchResultView::OnBlur() {
  SetBackgroundHighlighted(false);
}

void SearchResultView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  DCHECK(sender == this);

  list_view_->SearchResultActivated(this, event.flags());
}

void SearchResultView::OnMetadataChanged() {
  // Updates |icon_|.
  // Note: this might leave the view with an old icon. But it is needed to avoid
  // flash when a SearchResult's icon is loaded asynchronously. In this case, it
  // looks nicer to keep the stale icon for a little while on screen instead of
  // clearing it out. It should work correctly as long as the SearchResult does
  // not forget to SetIcon when it's ready.
  const gfx::ImageSkia icon(result_ ? result_->icon() : gfx::ImageSkia());
  if (!icon.isNull())
    SetIconImage(icon, icon_,
                 AppListConfig::instance().search_list_icon_dimension());

  // Updates |badge_icon_|.
  const gfx::ImageSkia badge_icon(result_ ? result_->badge_icon()
                                          : gfx::ImageSkia());
  if (badge_icon.isNull()) {
    badge_icon_->SetVisible(false);
  } else {
    SetIconImage(badge_icon, badge_icon_,
                 AppListConfig::instance().search_list_badge_icon_dimension());
    badge_icon_->SetVisible(true);
  }

  // Updates |actions_view_|.
  actions_view_->SetActions(result_ ? result_->actions()
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

void SearchResultView::OnIsInstallingChanged() {
  const bool is_installing = result_ && result_->is_installing();
  actions_view_->SetVisible(!is_installing);
  progress_bar_->SetVisible(is_installing);
}

void SearchResultView::OnPercentDownloadedChanged() {
  progress_bar_->SetValue(result_ ? result_->percent_downloaded() / 100.0 : 0);
}

void SearchResultView::OnItemInstalled() {
  list_view_->OnSearchResultInstalled(this);
}

void SearchResultView::OnSearchResultActionActivated(size_t index,
                                                     int event_flags) {
  // |result_| could be NULL when result list is changing.
  if (!result_)
    return;

  DCHECK_LT(index, result_->actions().size());

  list_view_->SearchResultActionActivated(this, index, event_flags);
}

void SearchResultView::ShowContextMenuForView(views::View* source,
                                              const gfx::Point& point,
                                              ui::MenuSourceType source_type) {
  // |result_| could be NULL when result list is changing.
  if (!result_)
    return;

  view_delegate_->GetSearchResultContextMenuModel(
      result_->id(), base::BindOnce(&SearchResultView::OnGetContextMenu,
                                    weak_ptr_factory_.GetWeakPtr(), source,
                                    point, source_type));
}

void SearchResultView::OnGetContextMenu(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type,
    std::vector<ash::mojom::MenuItemPtr> menu) {
  if (menu.empty() || context_menu_->IsShowingMenu())
    return;

  context_menu_ = std::make_unique<AppListMenuModelAdapter>(
      std::string(), this, source_type, this,
      AppListMenuModelAdapter::SEARCH_RESULT, base::OnceClosure());
  context_menu_->Build(std::move(menu));
  context_menu_->Run(gfx::Rect(point, gfx::Size()), views::MENU_ANCHOR_TOPLEFT,
                     views::MenuRunner::HAS_MNEMONICS);
  source->RequestFocus();
}

void SearchResultView::ExecuteCommand(int command_id, int event_flags) {
  if (result_) {
    view_delegate_->SearchResultContextMenuItemSelected(
        result_->id(), command_id, event_flags);
  }
}

}  // namespace app_list
