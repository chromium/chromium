// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_presenter.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/app_list/app_list_bubble_event_filter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_event_targeter.h"
#include "ash/app_list/apps_collections_controller.h"
#include "ash/app_list/views/app_list_bubble_apps_collections_page.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/wm/container_finder.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

using assistant::AssistantExitPoint;

// Maximum amount of time to spend refreshing zero state search results before
// opening the launcher.
constexpr base::TimeDelta kZeroStateSearchTimeout = base::Milliseconds(16);

// Space between the edge of the bubble and the edge of the work area.
constexpr int kWorkAreaPadding = 8;

// Space between the AppListBubbleView and the top of the screen should be at
// least this value plus the shelf height.
constexpr int kExtraTopOfScreenSpacing = 16;

gfx::Rect GetWorkAreaForBubble(aura::Window* root_window) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  gfx::RectF work_area(display.work_area());

  // Subtract the shelf's bounds from the work area, since the shelf should
  // always be shown with the app list bubble. This is done because the work
  // area includes the area under the shelf when the shelf is set to auto-hide.
  gfx::RectF shelf_bounds(Shelf::ForWindow(root_window)->GetIdealBounds());
  wm::TranslateRectToScreen(root_window, &shelf_bounds);
  work_area.Subtract(shelf_bounds);

  return gfx::ToRoundedRect(work_area);
}

int GetBubbleWidth(gfx::Rect work_area, aura::Window* root_window) {
  // As of August 2021 the assistant cards require a minimum width of 640. If
  // the cards become narrower then this could be reduced.
  return work_area.width() < 1200 ? 544 : 640;
}

// Returns the preferred size of the bubble widget in DIPs.
gfx::Size ComputeBubbleSize(aura::Window* root_window,
                            AppListBubbleView* bubble_view) {
  const int default_height = 688;
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const gfx::Rect work_area = GetWorkAreaForBubble(root_window);
  int height = default_height;

  const int width = GetBubbleWidth(work_area, root_window);
  // If the work area height is too small to fit the default size bubble, then
  // calculate a smaller height to fit in the work area. Otherwise, if the work
  // area height is tall enough to fit at least two default sized bubbles, then
  // calculate a taller bubble with height taking no more than half the work
  // area.
  if (work_area.height() <
      default_height + shelf_size + kExtraTopOfScreenSpacing) {
    height = work_area.height() - shelf_size - kExtraTopOfScreenSpacing;
  } else if (work_area.height() >
             default_height * 2 + shelf_size + kExtraTopOfScreenSpacing) {
    // Calculate the height required to fit the contents of the AppListBubble
    // with no scrolling.
    int height_to_fit_all_apps = bubble_view->GetHeightToFitAllApps();
    int max_height =
        (work_area.height() - shelf_size - kExtraTopOfScreenSpacing) / 2;
    DCHECK_GE(max_height, default_height);
    height = std::clamp(height_to_fit_all_apps, default_height, max_height);
  }

  return gfx::Size(width, height);
}

// Returns the bounds in root window coordinates for the bubble widget.
gfx::Rect ComputeBubbleBounds(aura::Window* root_window,
                              AppListBubbleView* bubble_view) {
  const gfx::Rect work_area = GetWorkAreaForBubble(root_window);
  const gfx::Size bubble_size = ComputeBubbleSize(root_window, bubble_view);
  const int padding = kWorkAreaPadding;  // Shorten name for readability.
  int x = 0;
  int y = 0;
  switch (Shelf::ForWindow(root_window)->alignment()) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      if (base::i18n::IsRTL())
        x = work_area.right() - padding - bubble_size.width();
      else
        x = work_area.x() + padding;
      y = work_area.bottom() - padding - bubble_size.height();
      break;
    case ShelfAlignment::kLeft:
      x = work_area.x() + padding;
      y = work_area.y() + padding;
      break;
    case ShelfAlignment::kRight:
      x = work_area.right() - padding - bubble_size.width();
      y = work_area.y() + padding;
      break;
  }
  return gfx::Rect(x, y, bubble_size.width(), bubble_size.height());
}

// Creates a bubble widget for the display with `root_window`. The widget is
// owned by its native widget.
views::Widget* CreateBubbleWidget(aura::Window* root_window) {
  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "AppListBubble";
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_AppListContainer);
  // AppListBubbleView handles round corners and blur via layers.
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  widget->Init(std::move(params));
  return widget;
}

}  // namespace

AppListBubblePresenter::AppListBubblePresenter(
    AppListControllerImpl* controller)
    : controller_(controller) {
  DCHECK(controller_);
}

AppListBubblePresenter::~AppListBubblePresenter() {
  CHECK(!views::WidgetObserver::IsInObserverList());
}

void AppListBubblePresenter::Shutdown() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  // Aborting in-progress animations will run their cleanup callbacks, which
  // might close the widget.
  if (bubble_view_)
    bubble_view_->AbortAllAnimations();
  if (bubble_widget_)
    bubble_widget_->CloseNow();  // Calls OnWidgetDestroying().
  DCHECK(!bubble_widget_);
  DCHECK(!bubble_view_);
}

void AppListBubblePresenter::Show(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (is_target_visibility_show_)
    return;

  if (bubble_view_)
    bubble_view_->AbortAllAnimations();

  is_target_visibility_show_ = true;

  target_page_ = AppsCollectionsController::Get()->ShouldShowAppsCollection()
                     ? AppListBubblePage::kAppsCollections
                     : AppListBubblePage::kApps;

  controller_->OnVisibilityWillChange(/*visible=*/true, display_id);

  // Refresh the continue tasks before opening the launcher. If a file doesn't
  // exist on disk anymore then the launcher should not create or animate the
  // continue task view for that suggestion.
  controller_->GetClient()->StartZeroStateSearch(
      base::BindOnce(&AppListBubblePresenter::OnZeroStateSearchDone,
                     weak_factory_.GetWeakPtr(), display_id),
      kZeroStateSearchTimeout);
}

void AppListBubblePresenter::OnZeroStateSearchDone(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  // Dismiss() might have been called while waiting for zero-state results.
  if (!is_target_visibility_show_)
    return;

  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display_id);
  // Display might have disconnected during zero state refresh.
  if (!root_window)
    return;

  Shelf* shelf = Shelf::ForWindow(root_window);
  HomeButton* home_button = shelf->navigation_widget()->GetHomeButton();

  if (!bubble_widget_) {
    // If the bubble widget is null, this is the first show. Construct views.
    base::TimeTicks time_shown = base::TimeTicks::Now();

    bubble_widget_ = CreateBubbleWidget(root_window);
    bubble_widget_->GetNativeWindow()->SetTitle(l10n_util::GetStringUTF16(
        IDS_APP_LIST_LAUNCHER_ACCESSIBILITY_ANNOUNCEMENT));
    bubble_widget_->GetNativeWindow()->SetEventTargeter(
        std::make_unique<AppListEventTargeter>(controller_));
    bubble_view_ = bubble_widget_->SetContentsView(
        std::make_unique<AppListBubbleView>(controller_));
    // Some of Assistant UIs have to be initialized explicitly. See details in
    // the comment of AppListBubbleView::InitializeUIForBubbleView.
    bubble_view_->InitializeUIForBubbleView();
    // Arrow left/right and up/down triggers the same focus movement as
    // tab/shift+tab.
    bubble_widget_->widget_delegate()->SetEnableArrowKeyTraversal(true);

    bubble_widget_->AddObserver(this);
    Shell::Get()->activation_client()->AddObserver(this);
    // Set up event filter to close the bubble for clicks outside the bubble
    // that don't cause window activation changes (e.g. clicks on wallpaper or
    // blank areas of shelf).
    bubble_event_filter_ = std::make_unique<AppListBubbleEventFilter>(
        bubble_widget_, home_button,
        base::BindRepeating(&AppListBubblePresenter::OnPressOutsideBubble,
                            base::Unretained(this)));

    UmaHistogramTimes("Apps.AppListBubbleCreationTime",
                      base::TimeTicks::Now() - time_shown);
  } else {
    DCHECK(bubble_view_);
    // Refresh suggestions now that zero-state search data is updated.
    bubble_view_->UpdateSuggestions();
    bubble_event_filter_->SetButton(home_button);
  }

  // The widget bounds sometimes depend on the height of the apps grid, so set
  // the bounds after creating and setting the contents. This may cause the
  // bubble to change displays.
  bubble_widget_->SetBounds(ComputeBubbleBounds(root_window, bubble_view_));

  // Bubble launcher is always keyboard traversable. Update every show in case
  // we are coming out of tablet mode.
  controller_->SetKeyboardTraversalMode(true);

  bubble_widget_->Show();
  // The page must be set before triggering the show animation so the correct
  // animations are triggered.
  bubble_view_->ShowPage(target_page_);
  const bool is_side_shelf = !shelf->IsHorizontalAlignment();
  bubble_view_->StartShowAnimation(is_side_shelf);
  controller_->OnVisibilityChanged(/*visible=*/true, display_id);
}

ShelfAction AppListBubblePresenter::Toggle(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (is_target_visibility_show_) {
    Dismiss();
    return SHELF_ACTION_APP_LIST_DISMISSED;
  }
  Show(display_id);
  return SHELF_ACTION_APP_LIST_SHOWN;
}

void AppListBubblePresenter::Dismiss() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (!is_target_visibility_show_)
    return;

  // Check for view because the code could be waiting for zero-state search
  // results before first show.
  if (bubble_view_)
    bubble_view_->AbortAllAnimations();

  // Must call before setting `is_target_visibility_show_` to false.
  const int64_t display_id = GetDisplayId();

  is_target_visibility_show_ = false;

  // Reset keyboard traversal in case the user switches to tablet launcher.
  // Must happen before widget is destroyed.
  controller_->SetKeyboardTraversalMode(false);

  controller_->ViewClosing();
  controller_->OnVisibilityWillChange(/*visible=*/false, display_id);
  if (bubble_view_) {
    aura::Window* bubble_window = bubble_view_->GetWidget()->GetNativeWindow();
    DCHECK(bubble_window);
    Shelf* shelf = Shelf::ForWindow(bubble_window);
    const bool is_side_shelf = !shelf->IsHorizontalAlignment();
    bubble_view_->StartHideAnimation(
        is_side_shelf,
        base::BindOnce(&AppListBubblePresenter::OnHideAnimationEnded,
                       weak_factory_.GetWeakPtr()));
  }
  controller_->OnVisibilityChanged(/*visible=*/false, display_id);

  // Clean up assistant if it is showing.
  controller_->ScheduleCloseAssistant();
}

aura::Window* AppListBubblePresenter::GetWindow() const {
  return is_target_visibility_show_ && bubble_widget_
             ? bubble_widget_->GetNativeWindow()
             : nullptr;
}

bool AppListBubblePresenter::IsShowing() const {
  return is_target_visibility_show_;
}

bool AppListBubblePresenter::IsShowingEmbeddedAssistantUI() const {
  if (!is_target_visibility_show_)
    return false;

  // Bubble view is null while the bubble widget is being initialized for show.
  // In this case, return true iff the app list will show the assistant page
  // when initialized.
  if (!bubble_view_)
    return target_page_ == AppListBubblePage::kAssistant;

  return bubble_view_->IsShowingEmbeddedAssistantUI();
}

void AppListBubblePresenter::UpdateContinueSectionVisibility() {
  if (bubble_view_)
    bubble_view_->UpdateContinueSectionVisibility();
}

void AppListBubblePresenter::UpdateForNewSortingOrder(
    const std::optional<AppListSortOrder>& new_order,
    bool animate,
    base::OnceClosure update_position_closure) {
  DCHECK_EQ(animate, !update_position_closure.is_null());

  if (!bubble_view_) {
    // A rare case. Still handle it for safety.
    if (update_position_closure)
      std::move(update_position_closure).Run();
    return;
  }

  bubble_view_->UpdateForNewSortingOrder(new_order, animate,
                                         std::move(update_position_closure));
}

void AppListBubblePresenter::ShowEmbeddedAssistantUI() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  target_page_ = AppListBubblePage::kAssistant;
  // `bubble_view_` does not exist while waiting for zero-state results.
  // OnZeroStateSearchDone() sets the page in that case.
  if (bubble_view_) {
    DCHECK(bubble_widget_);
    bubble_view_->ShowEmbeddedAssistantUI();
  }
}

void AppListBubblePresenter::OnWidgetDestroying(views::Widget* widget) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  // NOTE: While the widget is usually cached after Show(), this method can be
  // called on monitor disconnect. Clean up state.
  // `bubble_event_filter_` holds a pointer to the widget.
  bubble_event_filter_.reset();
  Shell::Get()->activation_client()->RemoveObserver(this);
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
  bubble_view_ = nullptr;
}

void AppListBubblePresenter::OnWindowActivated(ActivationReason reason,
                                               aura::Window* gained_active,
                                               aura::Window* lost_active) {
  if (!is_target_visibility_show_)
    return;

  if (gained_active) {
    if (auto* container = GetContainerForWindow(gained_active)) {
      const int container_id = container->GetId();
      // The bubble can be shown without activation if:
      // 1. The bubble or one of its children (e.g. an uninstall dialog) gains
      //    activation; OR
      // 2. The shelf gains activation (e.g. by pressing Alt-Shift-L); OR
      // 3. A help bubble container's descendant gains activation.
      if (container_id == kShellWindowId_AppListContainer ||
          container_id == kShellWindowId_ShelfContainer ||
          container_id == kShellWindowId_HelpBubbleContainer) {
        return;
      }
    }
  }

  // Closing the bubble for "press" type events is handled by
  // `bubble_event_filter_`. Activation can change when a user merely moves the
  // cursor outside the app list bounds, so losing activation should not close
  // the bubble.
  if (reason == wm::ActivationChangeObserver::ActivationReason::INPUT_EVENT)
    return;

  aura::Window* app_list_container =
      bubble_widget_->GetNativeWindow()->parent();

  // Otherwise, if the bubble or one of its children lost activation or if
  // something other than the bubble gains activation, the bubble should close.
  if ((lost_active && app_list_container->Contains(lost_active)) ||
      (gained_active && !app_list_container->Contains(gained_active))) {
    Dismiss();
  }
}

void AppListBubblePresenter::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!IsShowing())
    return;
  // Ignore changes to displays that aren't showing the launcher.
  if (display.id() != GetDisplayId())
    return;
  aura::Window* root_window =
      bubble_widget_->GetNativeWindow()->GetRootWindow();
  bubble_widget_->SetBounds(ComputeBubbleBounds(root_window, bubble_view_));
}

void AppListBubblePresenter::OnPressOutsideBubble(
    const ui::LocatedEvent& event) {
  // Presses outside the bubble could be activating a shelf item. Record the
  // app list state prior to dismissal.
  controller_->RecordAppListState();

  // The press outside the bubble might spawn a menu. If the bubble is active at
  // the end of the hide animation, an activation change event will cause the
  // menu to close. Deactivate now so menus stay open. https://crbug.com/1299088
  if (bubble_widget_->IsActive()) {
    bubble_widget_->Deactivate();
  }
  Dismiss();
}

int64_t AppListBubblePresenter::GetDisplayId() const {
  if (!is_target_visibility_show_ || !bubble_widget_)
    return display::kInvalidDisplayId;
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(bubble_widget_->GetNativeView())
      .id();
}

void AppListBubblePresenter::OnHideAnimationEnded() {
  // Hiding the launcher causes a window activation change. If the launcher is
  // hiding because the user opened a system tray bubble, don't immediately
  // close the bubble in response.
  auto lock = TrayBackgroundView::DisableCloseBubbleOnWindowActivated();
  bubble_widget_->Hide();

  controller_->MaybeCloseAssistant();
}

int AppListBubblePresenter::GetPreferredBubbleWidth(
    aura::Window* root_window) const {
  return GetBubbleWidth(GetWorkAreaForBubble(root_window), root_window);
}

}  // namespace ash
