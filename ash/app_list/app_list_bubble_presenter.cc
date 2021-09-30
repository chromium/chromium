// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_bubble_presenter.h"

#include <memory>
#include <utility>

#include "ash/app_list/app_list_bubble_event_filter.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_drag_and_drop_host.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/services/assistant/public/cpp/assistant_enums.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using chromeos::assistant::AssistantExitPoint;

// Space between the edge of the bubble and the edge of the work area.
constexpr int kWorkAreaPadding = 8;

// Space between the AppListBubbleView and the top of the screen should be at
// least this value plus the shelf height.
constexpr int kExtraTopOfScreenSpacing = 16;

gfx::Rect GetWorkAreaForBubble(aura::Window* root_window) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_window);
  gfx::Rect work_area = display.work_area();

  // Subtract the shelf's bounds from the work area, since the shelf should
  // always be shown with the app list bubble. This is done because the work
  // area includes the area under the shelf when the shelf is set to auto-hide.
  work_area.Subtract(Shelf::ForWindow(root_window)->GetIdealBounds());

  return work_area;
}

// Returns the preferred size of the bubble widget in DIPs.
gfx::Size ComputeBubbleSize(aura::Window* root_window,
                            AppListBubbleView* bubble_view) {
  const int default_height = 688;
  // As of August 2021 the assistant cards require a minimum width of 640. If
  // the cards become narrower then this could be reduced.
  const int default_width = 640;
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const gfx::Rect work_area = GetWorkAreaForBubble(root_window);
  int height = default_height;

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
    height = base::clamp(height_to_fit_all_apps, default_height, max_height);
  }

  return gfx::Size(default_width, height);
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
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
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
  if (bubble_widget_)
    bubble_widget_->CloseNow();
  CHECK(!IsInObserverList());
}

void AppListBubblePresenter::Show(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (bubble_widget_)
    return;

  base::Time time_shown = base::Time::Now();

  aura::Window* root_window = Shell::GetRootWindowForDisplayId(display_id);
  bubble_widget_ = CreateBubbleWidget(root_window);
  Shelf* shelf = Shelf::ForWindow(root_window);
  ApplicationDragAndDropHost* drag_and_drop_host =
      shelf->shelf_widget()->GetDragAndDropHostForAppList();
  bubble_view_ = bubble_widget_->SetContentsView(
      std::make_unique<AppListBubbleView>(controller_, drag_and_drop_host));
  // The widget bounds sometimes depend on the height of the apps grid, so set
  // the bounds after creating and setting the contents.
  // TODO(jamescook): Update bounds on display configuration change.
  bubble_widget_->SetBounds(ComputeBubbleBounds(root_window, bubble_view_));

  // Arrow left/right and up/down triggers the same focus movement as
  // tab/shift+tab.
  bubble_widget_->widget_delegate()->SetEnableArrowKeyTraversal(true);

  bubble_widget_->AddObserver(this);
  controller_->OnVisibilityWillChange(/*visible=*/true, display_id);
  bubble_widget_->Show();
  controller_->OnVisibilityChanged(/*visible=*/true, display_id);
  bubble_view_->FocusSearchBox();  // Must happen after widget creation.

  // Bubble launcher is always keyboard traversable.
  controller_->SetKeyboardTraversalMode(true);

  // Set up event filter to close the bubble for clicks outside the bubble that
  // don't cause window activation changes (e.g. clicks on wallpaper or blank
  // areas of shelf).
  HomeButton* home_button = shelf->navigation_widget()->GetHomeButton();
  bubble_event_filter_ = std::make_unique<AppListBubbleEventFilter>(
      bubble_widget_, home_button,
      base::BindRepeating(&AppListBubblePresenter::OnPressOutsideBubble,
                          base::Unretained(this)));

  UmaHistogramTimes("Apps.AppListBubbleCreationTime",
                    base::Time::Now() - time_shown);
}

ShelfAction AppListBubblePresenter::Toggle(int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (bubble_widget_) {
    Dismiss();
    return SHELF_ACTION_APP_LIST_DISMISSED;
  }
  Show(display_id);
  return SHELF_ACTION_APP_LIST_SHOWN;
}

void AppListBubblePresenter::Dismiss() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (!bubble_widget_)
    return;

  // Reset keyboard traversal in case the user switches to tablet launcher.
  // Must happen before widget is destroyed.
  controller_->SetKeyboardTraversalMode(false);

  controller_->ViewClosing();
  DCHECK(bubble_widget_);  // ViewClosing() did not destroy the widget.

  const int64_t display_id = GetDisplayId();
  controller_->OnVisibilityWillChange(/*visible=*/false, display_id);
  bubble_widget_->CloseNow();
  controller_->OnVisibilityChanged(/*visible=*/false, display_id);

  // Clean up assistant. Must occur after CloseNow(), otherwise it will try to
  // Dismiss() the app list and call this function re-entrantly.
  AssistantUiController::Get()->CloseUi(AssistantExitPoint::kLauncherClose);
}

bool AppListBubblePresenter::IsShowing() const {
  return !!bubble_widget_;
}

bool AppListBubblePresenter::IsShowingEmbeddedAssistantUI() const {
  if (!bubble_view_)
    return false;
  return bubble_view_->IsShowingEmbeddedAssistantUI();
}

void AppListBubblePresenter::ShowEmbeddedAssistantUI() {
  bubble_view_->ShowEmbeddedAssistantUI();
}

void AppListBubblePresenter::OnWidgetDestroying(views::Widget* widget) {
  // `bubble_event_filter_` holds a pointer to the widget.
  bubble_event_filter_.reset();
  bubble_widget_->RemoveObserver(this);
  bubble_widget_ = nullptr;
  bubble_view_ = nullptr;
}

void AppListBubblePresenter::OnPressOutsideBubble() {
  // Presses outside the bubble could be activating a shelf item. Record the
  // app list state prior to dismissal.
  controller_->RecordAppListState();
  Dismiss();
}

int64_t AppListBubblePresenter::GetDisplayId() const {
  if (!bubble_widget_)
    return display::kInvalidDisplayId;
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(bubble_widget_->GetNativeView())
      .id();
}

}  // namespace ash
