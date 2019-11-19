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
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/command_line.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

namespace {

// Suggestion chip container top margin (from the search box view).
constexpr int kSuggestionChipContainerTopMarginForSmallScreens = 8;

// The ratio of allowed bounds for apps grid view to its maximum margin.
constexpr int kAppsGridMarginRatio = 16;
constexpr int kAppsGridMarginRatioForSmallWidth = 12;

// The width threshold under which kAppsGridMarginRatioForSmallWidth should be
// used to calculate apps grid horizontal margins.
constexpr int kAppsGridMarginSmallWidthThreshold = 600;

// The minimum margin of apps grid view.
constexpr int kAppsGridMinimumMargin = 8;

// The horizontal spacing between apps grid view and page switcher.
constexpr int kAppsGridPageSwitcherSpacing = 8;

// The range of app list transition progress in which the suggestion chips'
// opacity changes from 0 to 1.
constexpr float kSuggestionChipOpacityStartProgress = 0.66;
constexpr float kSuggestionChipOpacityEndProgress = 1;

// The app list transition progress value for fullscreen state.
constexpr float kAppListFullscreenProgressValue = 2.0;

// Returns ideal horizontal padding for apps container with provided contents
// bounds.
int GetContainerHorizontalPaddingForBounds(const gfx::Rect& bounds) {
  const int horizontal_margin_ratio =
      (app_list_features::IsScalableAppListEnabled() &&
       bounds.width() <= kAppsGridMarginSmallWidthThreshold)
          ? kAppsGridMarginRatioForSmallWidth
          : kAppsGridMarginRatio;
  return bounds.width() / horizontal_margin_ratio;
}

}  // namespace

// static
int AppsContainerView::GetMinimumGridHorizontalMargin() {
  // If ScalableAppList feature is enabled, there is no extra horizontal margin
  // between grid view and the page switcher.
  return kAppsGridPageSwitcherSpacing +
         PageSwitcher::kMaxButtonRadiusForRootGrid * 2 +
         (app_list_features::IsScalableAppListEnabled()
              ? 0
              : kAppsGridMinimumMargin);
}

AppsContainerView::AppsContainerView(ContentsView* contents_view,
                                     AppListModel* model)
    : contents_view_(contents_view) {
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  suggestion_chip_container_view_ =
      new SuggestionChipContainerView(contents_view);
  AddChildView(suggestion_chip_container_view_);

  apps_grid_view_ = new AppsGridView(contents_view_, nullptr);
  AddChildView(apps_grid_view_);

  // Page switcher should be initialized after AppsGridView.
  page_switcher_ =
      new PageSwitcher(apps_grid_view_->pagination_model(), true /* vertical */,
                       contents_view_->app_list_view()->is_tablet_mode());
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

  // If there is no selected view in the root grid when a folder is opened,
  // silently focus the first item in the folder to avoid showing the selection
  // highlight or announcing to A11y, but still ensuring the arrow keys navigate
  // from the first item.
  AppListItemView* first_item_view_in_folder_grid =
      app_list_folder_view_->items_grid_view()->view_model()->view_at(0);
  if (!apps_grid_view()->has_selected_view()) {
    first_item_view_in_folder_grid->SilentlyRequestFocus();
  } else {
    first_item_view_in_folder_grid->RequestFocus();
  }
  // Disable all the items behind the folder so that they will not be reached
  // during focus traversal.

  DisableFocusForShowingActiveFolder(true);
}

void AppsContainerView::ShowApps(AppListFolderItem* folder_item) {
  if (app_list_folder_view_->IsAnimationRunning())
    return;

  SetShowState(SHOW_APPS, folder_item ? true : false);
  DisableFocusForShowingActiveFolder(false);
}

void AppsContainerView::ResetForShowApps() {
  UpdateSuggestionChips();
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

void AppsContainerView::UpdateControlVisibility(
    ash::AppListViewState app_list_state,
    bool is_in_drag) {
  if (app_list_state == ash::AppListViewState::kClosed)
    return;

  set_can_process_events_within_subtree(
      app_list_state == ash::AppListViewState::kFullscreenAllApps ||
      app_list_state == ash::AppListViewState::kPeeking);

  apps_grid_view_->UpdateControlVisibility(app_list_state, is_in_drag);
  page_switcher_->SetVisible(
      is_in_drag ||
      app_list_state == ash::AppListViewState::kFullscreenAllApps ||
      (app_list_features::IsScalableAppListEnabled() &&
       app_list_state == ash::AppListViewState::kFullscreenSearch));

  // Ignore button press during dragging to avoid app list item views' opacity
  // being set to wrong value.
  page_switcher_->set_ignore_button_press(is_in_drag);

  suggestion_chip_container_view_->SetVisible(
      app_list_state == ash::AppListViewState::kFullscreenAllApps ||
      app_list_state == ash::AppListViewState::kPeeking || is_in_drag);
}

void AppsContainerView::AnimateOpacity(float current_progress,
                                       ash::AppListViewState target_view_state,
                                       const OpacityAnimator& animator) {
  const bool target_suggestion_chip_visibility =
      target_view_state == ash::AppListViewState::kFullscreenAllApps ||
      target_view_state == ash::AppListViewState::kPeeking;
  animator.Run(suggestion_chip_container_view_,
               target_suggestion_chip_visibility);

  if (!apps_grid_view_->layer()->GetAnimator()->IsAnimatingProperty(
          ui::LayerAnimationElement::OPACITY)) {
    apps_grid_view_->UpdateOpacity(true /*restore_opacity*/);
    apps_grid_view_->layer()->SetOpacity(current_progress > 1.0f ? 1.0f : 0.0f);
  }

  const bool target_grid_visibility =
      target_view_state == ash::AppListViewState::kFullscreenAllApps ||
      target_view_state == ash::AppListViewState::kFullscreenSearch;
  animator.Run(apps_grid_view_, target_grid_visibility);

  animator.Run(page_switcher_, target_grid_visibility);
}

void AppsContainerView::AnimateYPosition(
    ash::AppListViewState target_view_state,
    const TransformAnimator& animator) {
  const int target_suggestion_chip_y = GetExpectedSuggestionChipY(
      AppListView::GetTransitionProgressForState(target_view_state));

  suggestion_chip_container_view_->SetY(target_suggestion_chip_y);
  animator.Run(suggestion_chip_container_view_);

  apps_grid_view_->SetY(suggestion_chip_container_view_->y() +
                        chip_grid_y_distance_);
  animator.Run(apps_grid_view_);

  page_switcher_->SetY(suggestion_chip_container_view_->y() +
                       chip_grid_y_distance_);
  animator.Run(page_switcher_);
}

void AppsContainerView::UpdateYPositionAndOpacity(float progress,
                                                  bool restore_opacity) {
  apps_grid_view_->UpdateOpacity(restore_opacity);

  // Updates the opacity of page switcher buttons. The same rule as all apps in
  // AppsGridView.
  AppListView* app_list_view = contents_view_->app_list_view();
  int screen_bottom = app_list_view->GetScreenBottom();
  gfx::Rect switcher_bounds = page_switcher_->GetBoundsInScreen();
  float centerline_above_work_area =
      std::max<float>(screen_bottom - switcher_bounds.CenterPoint().y(), 0.f);
  const float start_px = AppListConfig::instance().all_apps_opacity_start_px();
  float opacity = std::min(
      std::max(
          (centerline_above_work_area - start_px) /
              (AppListConfig::instance().all_apps_opacity_end_px() - start_px),
          0.f),
      1.0f);
  page_switcher_->layer()->SetOpacity(restore_opacity ? 1.0f : opacity);

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
      restore_opacity ? 1.0 : chips_opacity);

  suggestion_chip_container_view_->SetY(GetExpectedSuggestionChipY(progress));

  apps_grid_view_->SetY(suggestion_chip_container_view_->y() +
                        chip_grid_y_distance_);
  page_switcher_->SetY(suggestion_chip_container_view_->y() +
                       chip_grid_y_distance_);
}

void AppsContainerView::OnTabletModeChanged(bool started) {
  suggestion_chip_container_view_->OnTabletModeChanged(started);
  apps_grid_view_->OnTabletModeChanged(started);
  app_list_folder_view_->OnTabletModeChanged(started);
  page_switcher_->set_is_tablet_mode(started);
}

void AppsContainerView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  switch (show_state_) {
    case SHOW_APPS: {
      // Layout suggestion chips.
      gfx::Rect chip_container_rect = rect;
      chip_container_rect.set_y(GetExpectedSuggestionChipY(
          contents_view_->app_list_view()->GetAppListTransitionProgress(
              AppListView::kProgressFlagNone)));
      chip_container_rect.set_height(
          GetAppListConfig().suggestion_chip_container_height());
      if (app_list_features::IsScalableAppListEnabled()) {
        chip_container_rect.Inset(GetContainerHorizontalPaddingForBounds(rect),
                                  0);
      }
      suggestion_chip_container_view_->SetBoundsRect(chip_container_rect);

      // Leave the same available bounds for the apps grid view in both
      // fullscreen and peeking state to avoid resizing the view during
      // animation and dragging, which is an expensive operation.
      rect.set_y(chip_container_rect.bottom());
      rect.set_height(
          rect.height() -
          GetExpectedSuggestionChipY(kAppListFullscreenProgressValue) -
          chip_container_rect.height());

      const int page_switcher_width =
          page_switcher_->GetPreferredSize().width();
      // With scalable app list feature enabled, the margins are calculated from
      // the edge of the apps container, instead of container bounds inset by
      // page switcher area.
      if (!app_list_features::IsScalableAppListEnabled())
        rect.Inset(kAppsGridPageSwitcherSpacing + page_switcher_width, 0);

      const GridLayout grid_layout = CalculateGridLayout();
      apps_grid_view_->SetLayout(grid_layout.columns, grid_layout.rows);

      // Layout apps grid.
      gfx::Rect grid_rect = rect;

      if (app_list_features::IsScalableAppListEnabled()) {
        const gfx::Insets grid_insets = apps_grid_view_->GetInsets();
        const gfx::Insets margins = CalculateMarginsForAvailableBounds(
            GetContentsBounds(),
            contents_view_->GetSearchBoxSize(ash::AppListState::kStateApps),
            true /*for_full_container_bounds*/);
        grid_rect.Inset(
            margins.left(),
            GetAppListConfig().grid_fadeout_zone_height() - grid_insets.top(),
            margins.right(), margins.bottom());
        // The grid rect insets are added to calculated margins. Given that the
        // grid bounds rect should include insets, they have to be removed from
        // added margins.
        grid_rect.Inset(-grid_insets.left(), 0, -grid_insets.right(),
                        -grid_insets.bottom());
      } else {
        grid_rect.Inset(CalculateMarginsForAvailableBounds(
            rect, gfx::Size(), false /*for_full_container_bounds*/));
        // The grid rect insets are added to calculated margins. Given that the
        // grid bounds rect should include insets, they have to be removed from
        // the added margins.
        grid_rect.Inset(-apps_grid_view_->GetInsets());
      }

      apps_grid_view_->SetBoundsRect(grid_rect);

      // Record the distance of y position between suggestion chip container
      // and apps grid view to avoid duplicate calculation of apps grid view's
      // y position during dragging.
      chip_grid_y_distance_ =
          apps_grid_view_->y() - suggestion_chip_container_view_->y();

      // Layout page switcher.
      page_switcher_->SetBoundsRect(
          gfx::Rect(grid_rect.right() + kAppsGridPageSwitcherSpacing,
                    grid_rect.y(), page_switcher_width, grid_rect.height()));
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
      event->type() == ui::ET_GESTURE_LONG_PRESS) {
    return;
  }

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
  return contents_view_->GetContentsBounds();
}

const gfx::Insets& AppsContainerView::CalculateMarginsForAvailableBounds(
    const gfx::Rect& available_bounds,
    const gfx::Size& search_box_size,
    bool for_full_container_bounds) {
  DCHECK_EQ(for_full_container_bounds,
            app_list_features::IsScalableAppListEnabled());

  if (cached_container_margins_.bounds_size == available_bounds.size() &&
      cached_container_margins_.search_box_size == search_box_size) {
    return cached_container_margins_.margins;
  }

  const GridLayout grid_layout = CalculateGridLayout();
  const gfx::Size min_grid_size = apps_grid_view()->GetMinimumTileGridSize(
      grid_layout.columns, grid_layout.rows);
  const gfx::Size max_grid_size = apps_grid_view()->GetMaximumTileGridSize(
      grid_layout.columns, grid_layout.rows);

  int available_height = available_bounds.height();
  // If calculating the bounds for the full apps container (rather than apps
  // grid only), add search box, and suggestion chips container height (with
  // its margins to search box and apps grid) to non apps grid size.
  // NOTE: Not removing bottom apps grid inset (or top inset when
  // |for_full_container_bounds| is false) because they are included into the
  // total margin values.
  if (for_full_container_bounds) {
    available_height -=
        search_box_size.height() +
        GetAppListConfig().grid_fadeout_zone_height() +
        GetAppListConfig().suggestion_chip_container_height() +
        GetAppListConfig().suggestion_chip_container_top_margin();
  }

  // Calculates margin value to ensure the apps grid size is within required
  // bounds.
  // |ideal_margin|: The value the margin would have with no restrictions on
  //                 grid size.
  // |available_size|: The available size for apps grid in the dimension where
  //                   margin is applied.
  // |min_size|: The min allowed size for apps grid in the dimension where
  //             margin is applied.
  // |max_size|: The max allowed size for apps grid in the dimension where
  //             margin is applied.
  const auto calculate_margin = [](int ideal_margin, int available_size,
                                   int min_size, int max_size) -> int {
    const int ideal_size = available_size - 2 * ideal_margin;
    if (ideal_size < min_size)
      return ideal_margin - (min_size - ideal_size + 1) / 2;
    if (ideal_size > max_size)
      return ideal_margin + (ideal_size - max_size) / 2;
    return ideal_margin;
  };

  const int ideal_vertical_margin =
      available_bounds.height() / kAppsGridMarginRatio;
  const int vertical_margin =
      calculate_margin(ideal_vertical_margin, available_height,
                       min_grid_size.height(), max_grid_size.height());

  const int ideal_horizontal_margin =
      GetContainerHorizontalPaddingForBounds(available_bounds);
  const int horizontal_margin =
      calculate_margin(ideal_horizontal_margin, available_bounds.width(),
                       min_grid_size.width(), max_grid_size.width());

  const int min_horizontal_margin =
      app_list_features::IsScalableAppListEnabled()
          ? kAppsGridPageSwitcherSpacing +
                page_switcher_->GetPreferredSize().width()
          : kAppsGridMinimumMargin;

  cached_container_margins_.margins = gfx::Insets(
      std::max(vertical_margin, GetAppListConfig().grid_fadeout_zone_height()),
      std::max(horizontal_margin, min_horizontal_margin),
      std::max(vertical_margin, GetAppListConfig().grid_fadeout_zone_height()),
      std::max(horizontal_margin, min_horizontal_margin));
  cached_container_margins_.bounds_size = available_bounds.size();
  cached_container_margins_.search_box_size = search_box_size;

  return cached_container_margins_.margins;
}

void AppsContainerView::UpdateSuggestionChips() {
  suggestion_chip_container_view_->SetResults(
      contents_view_->GetAppListMainView()
          ->view_delegate()
          ->GetSearchModel()
          ->results());
}

const AppListConfig& AppsContainerView::GetAppListConfig() const {
  return contents_view_->app_list_view()->GetAppListConfig();
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
      page_switcher_->set_can_process_events_within_subtree(true);
      folder_background_view_->SetVisible(false);
      apps_grid_view_->ResetForShowApps();
      if (show_apps_with_animation)
        app_list_folder_view_->ScheduleShowHideAnimation(false, false);
      else
        app_list_folder_view_->HideViewImmediately();
      break;
    case SHOW_ACTIVE_FOLDER:
      page_switcher_->set_can_process_events_within_subtree(false);
      folder_background_view_->SetVisible(true);
      app_list_folder_view_->ScheduleShowHideAnimation(true, false);
      break;
    case SHOW_ITEM_REPARENT:
      page_switcher_->set_can_process_events_within_subtree(true);
      folder_background_view_->SetVisible(false);
      app_list_folder_view_->ScheduleShowHideAnimation(false, true);
      break;
    default:
      NOTREACHED();
  }
}

void AppsContainerView::DisableFocusForShowingActiveFolder(bool disabled) {
  suggestion_chip_container_view_->DisableFocusForShowingActiveFolder(disabled);
  apps_grid_view_->DisableFocusForShowingActiveFolder(disabled);

  // Ignore the page switcher in accessibility tree so that buttons inside it
  // will not be accessed by ChromeVox.
  page_switcher_->GetViewAccessibility().OverrideIsIgnored(disabled);
  page_switcher_->GetViewAccessibility().NotifyAccessibilityEvent(
      ax::mojom::Event::kTreeChanged);
}

int AppsContainerView::GetSuggestionChipContainerTopMargin(
    float progress) const {
  // For small screen sizes in fullscreen state, reduce the margin between the
  // search box and suggestion chips to reclaim as much of the vertical space as
  // possible.
  if (GetContentsBounds().height() < kAppsGridMarginSmallWidthThreshold &&
      !app_list_features::IsScalableAppListEnabled() && progress > 1.0) {
    return gfx::Tween::IntValueBetween(
        progress - 1, GetAppListConfig().suggestion_chip_container_top_margin(),
        kSuggestionChipContainerTopMarginForSmallScreens);
  }
  return GetAppListConfig().suggestion_chip_container_top_margin();
}

int AppsContainerView::GetExpectedSuggestionChipY(float progress) {
  const gfx::Rect search_box_bounds =
      contents_view_->GetSearchBoxExpectedBoundsForProgress(
          ash::AppListState::kStateApps, progress);
  return search_box_bounds.bottom() +
         GetSuggestionChipContainerTopMargin(progress);
}

AppsContainerView::GridLayout AppsContainerView::CalculateGridLayout() const {
  // Adapt columns and rows based on the work area size.
  const gfx::Size size =
      display::Screen::GetScreen()
          ->GetDisplayNearestView(GetWidget()->GetNativeView())
          .work_area()
          .size();

  GridLayout result;
  const AppListConfig& config = GetAppListConfig();
  // Switch columns and rows for portrait mode.
  if (size.width() < size.height()) {
    result.columns = config.preferred_rows();
    result.rows = config.preferred_cols();
  } else {
    result.columns = config.preferred_cols();
    result.rows = config.preferred_rows();
  }
  return result;
}

}  // namespace ash
