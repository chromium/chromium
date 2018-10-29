// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_container_view.h"

#include <algorithm>
#include <vector>

#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/folder_background_view.h"
#include "ash/app_list/views/horizontal_page_container.h"
#include "ash/app_list/views/page_switcher.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/suggestion_chip_container_view.h"
#include "ash/app_list/views/suggestions_container_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/command_line.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/textfield/textfield.h"

namespace app_list {

namespace {

// Minimum top padding of search box in fullscreen state.
constexpr int kSearchBoxMinimumTopPadding = 24;

// Height of suggestion chip container.
constexpr int kSuggestionChipContainerHeight = 32;

// The y position of suggestion chips in peeking and fullscreen state.
constexpr int kSuggestionChipPeekingY = 156;
constexpr int kSuggestionChipFullscreenY = 96;

// The ratio of allowed bounds for apps grid view to its maximum margin.
constexpr int kAppsGridMarginRatio = 16;

// The minimum margin of apps grid view.
constexpr int kAppsGridMinimumMargin = 8;

// The horizontal spacing between apps grid view and page switcher.
constexpr int kAppsGridPageSwitcherSpacing = 8;

// The range of app list transition progress in which the suggestion chips'
// opacity changes from 0 to 1.
constexpr float kSuggestionChipOpacityStartProgress = 0.66;
constexpr float kSuggestionChipOpacityEndProgress = 1;

}  // namespace

AppsContainerView::AppsContainerView(ContentsView* contents_view,
                                     AppListModel* model)
    : contents_view_(contents_view),
      is_new_style_launcher_enabled_(
          app_list_features::IsNewStyleLauncherEnabled()) {
  if (is_new_style_launcher_enabled_) {
    suggestion_chip_container_view_ =
        new SuggestionChipContainerView(contents_view);
    AddChildView(suggestion_chip_container_view_);
    UpdateSuggestionChips();
  }
  apps_grid_view_ = new AppsGridView(contents_view_, nullptr);
  apps_grid_view_->SetLayout(AppListConfig::instance().preferred_cols(),
                             AppListConfig::instance().preferred_rows());
  AddChildView(apps_grid_view_);

  // Page switcher should be initialized after AppsGridView.
  page_switcher_ = new PageSwitcher(apps_grid_view_->pagination_model(),
                                    true /* vertical */);
  AddChildView(page_switcher_);

  app_list_folder_view_ = new AppListFolderView(this, model, contents_view_);
  // The folder view is initially hidden.
  app_list_folder_view_->SetVisible(false);
  folder_background_view_ = new FolderBackgroundView(app_list_folder_view_);
  AddChildView(folder_background_view_);
  AddChildView(app_list_folder_view_);

  apps_grid_view_->SetModel(model);
  apps_grid_view_->SetItemList(model->top_level_item_list());
  SetShowState(SHOW_APPS, false);
}

AppsContainerView::~AppsContainerView() {
  // Make sure |page_switcher_| is deleted before |apps_grid_view_| because
  // |page_switcher_| uses the PaginationModel owned by |apps_grid_view_|.
  delete page_switcher_;
}

void AppsContainerView::ShowActiveFolder(AppListFolderItem* folder_item) {
  // Prevent new animations from starting if there are currently animations
  // pending. This fixes crbug.com/357099.
  if (app_list_folder_view_->IsAnimationRunning())
    return;

  app_list_folder_view_->SetAppListFolderItem(folder_item);

  SetShowState(SHOW_ACTIVE_FOLDER, false);

  // Disable all the items behind the folder so that they will not be reached
  // during focus traversal.
  contents_view_->GetSearchBoxView()->search_box()->RequestFocus();
  DisableFocusForShowingActiveFolder(true);
}

void AppsContainerView::ShowApps(AppListFolderItem* folder_item) {
  if (app_list_folder_view_->IsAnimationRunning())
    return;

  SetShowState(SHOW_APPS, folder_item ? true : false);
  DisableFocusForShowingActiveFolder(false);
}

void AppsContainerView::ResetForShowApps() {
  SetShowState(SHOW_APPS, false);
  DisableFocusForShowingActiveFolder(false);
}

void AppsContainerView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  apps_grid_view()->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
  app_list_folder_view()->items_grid_view()->SetDragAndDropHostOfCurrentAppList(
      drag_and_drop_host);
}

void AppsContainerView::ReparentFolderItemTransit(
    AppListFolderItem* folder_item) {
  if (app_list_folder_view_->IsAnimationRunning())
    return;
  SetShowState(SHOW_ITEM_REPARENT, false);
  DisableFocusForShowingActiveFolder(false);
}

bool AppsContainerView::IsInFolderView() const {
  return show_state_ == SHOW_ACTIVE_FOLDER;
}

void AppsContainerView::ReparentDragEnded() {
  DCHECK_EQ(SHOW_ITEM_REPARENT, show_state_);
  show_state_ = AppsContainerView::SHOW_APPS;
}

void AppsContainerView::UpdateControlVisibility(AppListViewState app_list_state,
                                                bool is_in_drag) {
  apps_grid_view_->UpdateControlVisibility(app_list_state, is_in_drag);
  page_switcher_->SetVisible(
      app_list_state == AppListViewState::FULLSCREEN_ALL_APPS || is_in_drag);

  // Ignore button press during dragging to avoid app list item views' opacity
  // being set to wrong value.
  page_switcher_->set_ignore_button_press(is_in_drag);

  if (suggestion_chip_container_view_) {
    suggestion_chip_container_view_->SetVisible(
        app_list_state == AppListViewState::FULLSCREEN_ALL_APPS ||
        app_list_state == AppListViewState::PEEKING || is_in_drag);
  }
}

void AppsContainerView::UpdateYPositionAndOpacity() {
  apps_grid_view_->UpdateOpacity();

  // Updates the opacity of page switcher buttons. The same rule as all apps in
  // AppsGridView.
  AppListView* app_list_view = contents_view_->app_list_view();
  bool should_restore_opacity =
      !app_list_view->is_in_drag() &&
      (app_list_view->app_list_state() != AppListViewState::CLOSED);
  int screen_bottom = app_list_view->GetScreenBottom();
  gfx::Rect switcher_bounds = page_switcher_->GetBoundsInScreen();
  float centerline_above_work_area =
      std::max<float>(screen_bottom - switcher_bounds.CenterPoint().y(), 0.f);
  float opacity =
      std::min(std::max((centerline_above_work_area - kAllAppsOpacityStartPx) /
                            (kAllAppsOpacityEndPx - kAllAppsOpacityStartPx),
                        0.f),
               1.0f);
  page_switcher_->layer()->SetOpacity(should_restore_opacity ? 1.0f : opacity);

  const float progress =
      contents_view_->app_list_view()->GetAppListTransitionProgress();
  if (suggestion_chip_container_view_) {
    // Changes the opacity of suggestion chips between 0 and 1 when app list
    // transition progress changes between |kSuggestionChipOpacityStartProgress|
    // and |kSuggestionChipOpacityEndProgress|.
    float chips_opacity =
        std::min(std::max((progress - kSuggestionChipOpacityStartProgress) /
                              (kSuggestionChipOpacityEndProgress -
                               kSuggestionChipOpacityStartProgress),
                          0.f),
                 1.0f);
    suggestion_chip_container_view_->layer()->SetOpacity(
        should_restore_opacity ? 1.0f : chips_opacity);

    suggestion_chip_container_view_->SetY(GetExpectedSuggestionChipY(
        contents_view_->app_list_view()->GetAppListTransitionProgress()));

    apps_grid_view_->SetY(suggestion_chip_container_view_->y() +
                          chip_grid_y_distance_);

    page_switcher_->SetY(suggestion_chip_container_view_->bounds().bottom());
  }
}

void AppsContainerView::OnTabletModeChanged(bool started) {
  if (suggestion_chip_container_view_)
    suggestion_chip_container_view_->OnTabletModeChanged(started);
  apps_grid_view_->OnTabletModeChanged(started);
}

gfx::Size AppsContainerView::CalculatePreferredSize() const {
  if (is_new_style_launcher_enabled_)
    return contents_view_->GetPreferredSize();

  gfx::Size size = apps_grid_view_->GetPreferredSize();
  // Add padding to both side of the apps grid to keep it horizontally
  // centered since we place page switcher on the right side.
  size.Enlarge(kAppsGridLeftRightPadding * 2, 0);
  return size;
}

void AppsContainerView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  switch (show_state_) {
    case SHOW_APPS: {
      if (is_new_style_launcher_enabled_) {
        // Layout suggestion chips.
        gfx::Rect chip_container_rect(rect);
        chip_container_rect.set_y(GetExpectedSuggestionChipY(
            contents_view_->app_list_view()->GetAppListTransitionProgress()));
        chip_container_rect.set_height(kSuggestionChipContainerHeight);
        suggestion_chip_container_view_->SetBoundsRect(chip_container_rect);

        // Leave the same available bounds for the apps grid view in both
        // fullscreen and peeking state to avoid resizing the view during
        // animation and dragging, which is an expensive operation.
        rect.set_y(chip_container_rect.bottom());
        rect.set_height(rect.height() - kSuggestionChipFullscreenY -
                        kSuggestionChipContainerHeight);
      }

      // Layout apps grid.
      gfx::Rect grid_rect = rect;
      if (is_new_style_launcher_enabled_) {
        // Switch the column and row size if apps grid's height is greater than
        // its width.
        const int cols = AppListConfig::instance().preferred_cols();
        const int rows = AppListConfig::instance().preferred_rows();
        const bool switch_cols_and_rows =
            grid_rect.height() > grid_rect.width();
        apps_grid_view_->SetLayout(switch_cols_and_rows ? rows : cols,
                                   switch_cols_and_rows ? cols : rows);

        // Calculate the maximum margin of apps grid.
        const int max_horizontal_margin =
            grid_rect.width() / kAppsGridMarginRatio;
        const int max_vertical_margin =
            grid_rect.height() / kAppsGridMarginRatio;

        // Calculate the minimum size of apps grid.
        const gfx::Size min_grid_size =
            apps_grid_view()->GetMinimumTileGridSize();

        // Calculate the actual margin of apps grid based on the rule: Always
        // keep maximum margin if apps grid can maintain at least
        // |min_grid_size|; Otherwise, always keep at least
        // |kAppsGridMinimumMargin|.
        const int horizontal_margin =
            max_horizontal_margin * 2 <=
                    grid_rect.width() - min_grid_size.width()
                ? max_horizontal_margin
                : std::max(kAppsGridMinimumMargin,
                           (grid_rect.width() - min_grid_size.width()) / 2);
        const int vertical_margin =
            max_vertical_margin * 2 <=
                    grid_rect.height() - min_grid_size.height()
                ? max_vertical_margin
                : std::max(kAppsGridMinimumMargin,
                           (grid_rect.height() - min_grid_size.height()) / 2);
        grid_rect.Inset(horizontal_margin, vertical_margin);
        grid_rect.ClampToCenteredSize(
            apps_grid_view_->GetMaximumTileGridSize());

        if ((grid_rect.width() > 0 && grid_rect.height() > 0) &&
            (grid_rect.width() < min_grid_size.width() ||
             grid_rect.height() < min_grid_size.height())) {
          // If the minimum size does not fit inside available bounds, scale
          // down the apps grid view via transform while keep the minimum size.
          const gfx::Insets insets = apps_grid_view_->GetInsets();
          const float scale =
              std::min((grid_rect.width()) /
                           static_cast<float>(min_grid_size.width() +
                                              insets.left() + insets.right()),
                       grid_rect.height() /
                           static_cast<float>(min_grid_size.height() +
                                              insets.top() + insets.bottom()));
          DCHECK_GT(scale, 0);
          const gfx::RectF scaled_grid_rect(grid_rect.x(), grid_rect.y(),
                                            grid_rect.width() / scale,
                                            grid_rect.height() / scale);

          gfx::Transform transform;
          transform.Scale(scale, scale);
          apps_grid_view_->SetTransform(transform);
          apps_grid_view_->SetBoundsRect(gfx::ToEnclosedRect(scaled_grid_rect));
        } else {
          grid_rect.Inset(-apps_grid_view_->GetInsets());
          apps_grid_view_->SetTransform(gfx::Transform());
          apps_grid_view_->SetBoundsRect(grid_rect);
        }

        // Record the distance of y position between suggestion chip container
        // and apps grid view to avoid duplicate calculation of apps grid view's
        // y position during dragging.
        chip_grid_y_distance_ =
            apps_grid_view_->y() - suggestion_chip_container_view_->y();
      } else {
        grid_rect.Inset(kAppsGridLeftRightPadding, 0);
        apps_grid_view_->SetBoundsRect(grid_rect);
      }

      // Layout page switcher.
      gfx::Rect page_switcher_rect = rect;
      page_switcher_rect.Inset(grid_rect.right() + kAppsGridPageSwitcherSpacing,
                               0, 0, 0);
      page_switcher_rect.set_width(page_switcher_->GetPreferredSize().width());
      page_switcher_->SetBoundsRect(page_switcher_rect);
      break;
    }
    case SHOW_ACTIVE_FOLDER: {
      folder_background_view_->SetBoundsRect(rect);
      app_list_folder_view_->SetBoundsRect(
          app_list_folder_view_->preferred_bounds());
      break;
    }
    case SHOW_ITEM_REPARENT:
      break;
    default:
      NOTREACHED();
  }
}

bool AppsContainerView::OnKeyPressed(const ui::KeyEvent& event) {
  if (show_state_ == SHOW_APPS)
    return apps_grid_view_->OnKeyPressed(event);
  else
    return app_list_folder_view_->OnKeyPressed(event);
}

const char* AppsContainerView::GetClassName() const {
  return "AppsContainerView";
}

void AppsContainerView::OnGestureEvent(ui::GestureEvent* event) {
  // Ignore tap/long-press, allow those to pass to the ancestor view.
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_LONG_PRESS)
    return;

  // Will forward events to |apps_grid_view_| if they occur in the same y-region
  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN &&
      event->location().y() <= apps_grid_view_->bounds().y()) {
    return;
  }

  // If a folder is currently opening or closing, we should ignore the event.
  // This is here until the animation for pagination while closing folders is
  // fixed: https://crbug.com/875133
  if (app_list_folder_view_->IsAnimationRunning()) {
    event->SetHandled();
    return;
  }

  // Temporary event for use by |apps_grid_view_|
  ui::GestureEvent grid_event(*event);
  ConvertEventToTarget(apps_grid_view_, &grid_event);
  apps_grid_view_->OnGestureEvent(&grid_event);

  // If the temporary event was handled, we don't want to handle it again.
  if (grid_event.handled())
    event->SetHandled();
}

void AppsContainerView::OnWillBeHidden() {
  if (show_state_ == SHOW_APPS || show_state_ == SHOW_ITEM_REPARENT)
    apps_grid_view_->EndDrag(true);
  else if (show_state_ == SHOW_ACTIVE_FOLDER)
    app_list_folder_view_->CloseFolderPage();
}

views::View* AppsContainerView::GetFirstFocusableView() {
  if (IsInFolderView()) {
    // The pagination inside a folder is set horizontally, so focus should be
    // set on the first item view in the selected page when it is moved down
    // from the search box.
    return app_list_folder_view_->items_grid_view()
        ->GetCurrentPageFirstItemViewInFolder();
  }
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), false /* reverse */, false /* dont_loop */);
}

gfx::Rect AppsContainerView::GetPageBoundsForState(
    ash::AppListState state) const {
  if (is_new_style_launcher_enabled_)
    return gfx::Rect(contents_view_->GetPreferredSize());

  if (contents_view_->app_list_view()->is_in_drag())
    return GetPageBoundsDuringDragging(state);

  gfx::Rect bounds = parent()->GetContentsBounds();
  bounds.ClampToCenteredSize(GetPreferredSize());

  // AppsContainerView page is shown in both STATE_START and STATE_APPS.
  if (state == ash::AppListState::kStateApps ||
      state == ash::AppListState::kStateStart) {
    // The bottom left point relative to |contents_view_|.
    gfx::Point bottom_left = GetSearchBoxExpectedBounds().bottom_left();
    if (state == ash::AppListState::kStateStart) {
      bottom_left.Offset(
          0, kSearchBoxPeekingBottomPadding - kSearchBoxBottomPadding);
    }
    ConvertPointToTarget(contents_view_, parent(), &bottom_left);
    bounds.set_y(bottom_left.y());
  }

  return bounds;
}

gfx::Rect AppsContainerView::GetSearchBoxExpectedBounds() const {
  gfx::Rect search_box_bounds(contents_view_->GetDefaultSearchBoxBounds());
  const float progress =
      contents_view_->app_list_view()->GetAppListTransitionProgress();
  if (progress <= 1) {
    search_box_bounds.set_y(gfx::Tween::IntValueBetween(
        progress, AppListConfig::instance().search_box_closed_top_padding(),
        AppListConfig::instance().search_box_peeking_top_padding()));
  } else {
    search_box_bounds.set_y(gfx::Tween::IntValueBetween(
        progress - 1,
        AppListConfig::instance().search_box_peeking_top_padding(),
        is_new_style_launcher_enabled_
            ? AppListConfig::instance().search_box_fullscreen_top_padding()
            : GetSearchBoxFinalTopPadding()));
  }
  return search_box_bounds;
}

int AppsContainerView::GetSearchBoxFinalTopPadding() const {
  gfx::Rect search_box_bounds(contents_view_->GetDefaultSearchBoxBounds());
  const int total_height =
      GetPreferredSize().height() + search_box_bounds.height();

  // Makes search box and content vertically centered in contents_view.
  int y = std::max(
      search_box_bounds.y(),
      (contents_view_->GetPreferredSize().height() - total_height) / 2);

  // Top padding of the searchbox should not be smaller than
  // |kSearchBoxMinimumTopPadding|
  return std::max(y, kSearchBoxMinimumTopPadding);
}

gfx::Rect AppsContainerView::GetPageBoundsDuringDragging(
    ash::AppListState state) const {
  const int shelf_height = AppListConfig::instance().shelf_height();
  const float drag_amount = std::max(
      0.f, static_cast<float>(
               contents_view_->app_list_view()->GetCurrentAppListHeight() -
               shelf_height));
  const int peeking_height =
      AppListConfig::instance().peeking_app_list_height();

  float y = 0;
  const float peeking_final_y =
      AppListConfig::instance().search_box_peeking_top_padding() +
      search_box::kSearchBoxPreferredHeight + kSearchBoxPeekingBottomPadding -
      kSearchBoxBottomPadding;
  if (drag_amount <= (peeking_height - shelf_height)) {
    // App list is dragged from collapsed to peeking, which moved up at most
    // |peeking_height - shelf_size| (272px). The top padding of apps
    // container view changes from |-kSearchBoxFullscreenBottomPadding| to
    // |kSearchBoxPeekingTopPadding + kSearchBoxPreferredHeight +
    // kSearchBoxPeekingBottomPadding - kSearchBoxFullscreenBottomPadding|.
    y = std::ceil(((peeking_final_y + kSearchBoxBottomPadding) * drag_amount) /
                      (peeking_height - shelf_height) -
                  kSearchBoxBottomPadding);
  } else {
    // App list is dragged from peeking to fullscreen, which moved up at most
    // |peeking_to_fullscreen_height|. The top padding of apps container view
    // changes from |peeking_final_y| to |final_y|.
    float final_y =
        GetSearchBoxFinalTopPadding() + search_box::kSearchBoxPreferredHeight;
    float peeking_to_fullscreen_height =
        contents_view_->GetPreferredSize().height() - peeking_height;
    y = std::ceil((final_y - peeking_final_y) *
                      (drag_amount - (peeking_height - shelf_height)) /
                      peeking_to_fullscreen_height +
                  peeking_final_y);
    y = std::max(std::min(final_y, y), peeking_final_y);
  }

  gfx::Rect bounds = parent()->GetContentsBounds();
  bounds.ClampToCenteredSize(GetPreferredSize());

  // AppsContainerView page is shown in both STATE_START and STATE_APPS.
  if (state == ash::AppListState::kStateApps ||
      state == ash::AppListState::kStateStart) {
    gfx::Point point(0, y);
    ConvertPointToTarget(contents_view_, parent(), &point);
    bounds.set_y(point.y());
  }

  return bounds;
}

void AppsContainerView::SetShowState(ShowState show_state,
                                     bool show_apps_with_animation) {
  if (show_state_ == show_state)
    return;

  show_state_ = show_state;

  // Layout before showing animation because the animation's target bounds are
  // calculated based on the layout.
  Layout();

  switch (show_state_) {
    case SHOW_APPS:
      folder_background_view_->SetVisible(false);
      apps_grid_view_->ResetForShowApps();
      if (is_new_style_launcher_enabled_)
        UpdateSuggestionChips();
      if (show_apps_with_animation)
        app_list_folder_view_->ScheduleShowHideAnimation(false, false);
      else
        app_list_folder_view_->HideViewImmediately();
      break;
    case SHOW_ACTIVE_FOLDER:
      folder_background_view_->SetVisible(true);
      app_list_folder_view_->ScheduleShowHideAnimation(true, false);
      break;
    case SHOW_ITEM_REPARENT:
      folder_background_view_->SetVisible(false);
      app_list_folder_view_->ScheduleShowHideAnimation(false, true);
      break;
    default:
      NOTREACHED();
  }
}

void AppsContainerView::UpdateSuggestionChips() {
  DCHECK(suggestion_chip_container_view_);
  suggestion_chip_container_view_->SetResults(
      contents_view_->GetAppListMainView()
          ->view_delegate()
          ->GetSearchModel()
          ->results());
}

void AppsContainerView::DisableFocusForShowingActiveFolder(bool disabled) {
  if (suggestion_chip_container_view_) {
    suggestion_chip_container_view_->DisableFocusForShowingActiveFolder(
        disabled);
  }
  apps_grid_view_->DisableFocusForShowingActiveFolder(disabled);
}

int AppsContainerView::GetExpectedSuggestionChipY(float progress) {
  if (progress <= 1) {
    // Currently transition progress is between closed and peeking state.
    return gfx::Tween::IntValueBetween(progress, 0, kSuggestionChipPeekingY);
  }

  // Currently transition progress is between peeking and fullscreen
  // state.
  return gfx::Tween::IntValueBetween(progress - 1, kSuggestionChipPeekingY,
                                     kSuggestionChipFullscreenY);
}

}  // namespace app_list
