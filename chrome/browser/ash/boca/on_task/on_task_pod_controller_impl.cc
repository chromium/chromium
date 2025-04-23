// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_pod_controller_impl.h"

#include <memory>
#include <string>

#include "ash/boca/on_task/on_task_pod_utils.h"
#include "ash/boca/on_task/on_task_pod_view.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/wm/window_pin_util.h"
#include "ash/wm/window_state.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/frame/frame_header.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Internal name for the OnTask pod widget. Useful for debugging purposes.
constexpr char kOnTaskPodWidgetInternalName[] = "OnTaskPod";

// Creates a child widget for the specified parent window with some common
// characteristics.
std::unique_ptr<views::Widget> CreateChildWidget(
    aura::Window* parent_window,
    const std::string& widget_name,
    std::unique_ptr<views::View> view) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = parent_window;
  params.name = widget_name;
  params.activatable = views::Widget::InitParams::Activatable::kDefault;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->SetContentsView(std::move(view));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_SHOW);

  return widget;
}
}  // namespace

OnTaskPodControllerImpl::OnTaskPodControllerImpl(Browser* browser)
    : browser_(browser->AsWeakPtr()) {
  aura::Window* const browser_window = browser_->window()->GetNativeWindow();
  auto on_task_pod_view = std::make_unique<OnTaskPodView>(this);
  pod_widget_ = CreateChildWidget(browser_window->GetToplevelWindow(),
                                  kOnTaskPodWidgetInternalName,
                                  std::move(on_task_pod_view));
  // Fetch the header height in unlocked mode, since the value changes when the
  // window enters locked fullscreen.
  frame_header_height_ = boca::GetFrameHeaderHeight(pod_widget_->parent());
  pod_widget_->widget_delegate()->SetAccessibleTitle(
      l10n_util::GetStringUTF16(IDS_ON_TASK_POD_ACCESSIBLE_NAME));
  pod_widget_->SetBounds(CalculateWidgetBounds());
  OnPageNavigationContextChanged();
  pod_widget_->SetFocusTraversableParentView(
      BrowserView::GetBrowserViewForBrowser(browser_.get()));
  pod_widget_->Show();

  browser_window->AddObserver(this);
  WindowState::Get(browser_window)->AddObserver(this);
}

OnTaskPodControllerImpl::~OnTaskPodControllerImpl() {
  if (browser_) {
    aura::Window* const browser_window = browser_->window()->GetNativeWindow();
    WindowState::Get(browser_window)->RemoveObserver(this);
    browser_window->RemoveObserver(this);
  }
}

void OnTaskPodControllerImpl::MaybeNavigateToPreviousPage() {
  if (!browser_) {
    return;
  }
  boca::RecordOnTaskPodNavigateBackClicked();
  chrome::GoBack(browser_.get(), WindowOpenDisposition::CURRENT_TAB);
}

void OnTaskPodControllerImpl::MaybeNavigateToNextPage() {
  if (!browser_) {
    return;
  }
  boca::RecordOnTaskPodNavigateForwardClicked();
  chrome::GoForward(browser_.get(), WindowOpenDisposition::CURRENT_TAB);
}

void OnTaskPodControllerImpl::ReloadCurrentPage() {
  if (!browser_) {
    return;
  }
  boca::RecordOnTaskPodReloadPageClicked();
  chrome::Reload(browser_.get(), WindowOpenDisposition::CURRENT_TAB);
}

void OnTaskPodControllerImpl::ToggleTabStripVisibility(bool show,
                                                       bool user_action) {
  if (user_action) {
    boca::RecordOnTaskPodToggleTabStripVisibilityClicked();
  }

  // Hide tab strip.
  if (!show) {
    tab_strip_reveal_lock_.reset();
    return;
  }

  // Acquire lock to reveal the tab strip.
  auto* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_.get());
  tab_strip_reveal_lock_ =
      browser_view->immersive_mode_controller()->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_YES);
}

void OnTaskPodControllerImpl::SetSnapLocation(
    OnTaskPodSnapLocation snap_location) {
  boca::RecordOnTaskPodSetSnapLocationClicked(
      snap_location == ash::OnTaskPodSnapLocation::kTopLeft);
  pod_snap_location_ = snap_location;

  // Reposition the widget.
  pod_widget_->SetBounds(CalculateWidgetBounds());
}

void OnTaskPodControllerImpl::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (!pod_widget_) {
    return;
  }
  pod_widget_->SetBounds(CalculateWidgetBounds());
}

void OnTaskPodControllerImpl::OnPostWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  // Only update the pod when the old window and new window have different
  // pinned state.
  chromeos::WindowStateType new_type = window_state->GetStateType();
  if (chromeos::IsPinnedWindowStateType(old_type) ==
      chromeos::IsPinnedWindowStateType(new_type)) {
    return;
  }
  views::View* const pod_widget_contents_view = pod_widget_->GetContentsView();
  if (!pod_widget_contents_view) {
    return;
  }
  OnTaskPodView* const on_task_pod_view =
      static_cast<OnTaskPodView*>(pod_widget_contents_view);
  on_task_pod_view->OnLockedModeUpdate();

  // Resize and reset bounds of the widget to fit the contents view.
  pod_widget_->SetSize(on_task_pod_view->GetPreferredSize());
  pod_widget_->SetBounds(CalculateWidgetBounds());
}

void OnTaskPodControllerImpl::OnWindowVisibilityChanged(aura::Window* window,
                                                        bool visible) {
  DCHECK(pod_widget_);
  // We need to check browser window visibility directly; `visible` param is for
  // webcontents visibility, which changes when we switch tabs.
  if (browser_->window()->IsVisible()) {
    pod_widget_->Show();
  } else {
    pod_widget_->Hide();
  }
}

void OnTaskPodControllerImpl::OnPauseModeChanged(bool paused) {
  if (!pod_widget_) {
    return;
  }
  if (paused) {
    pod_widget_->Hide();
  } else {
    pod_widget_->Show();
  }
}

void OnTaskPodControllerImpl::OnPageNavigationContextChanged() {
  if (!pod_widget_) {
    return;
  }
  views::View* const pod_widget_contents_view = pod_widget_->GetContentsView();
  if (!pod_widget_contents_view) {
    return;
  }
  OnTaskPodView* const on_task_pod_view =
      static_cast<OnTaskPodView*>(pod_widget_contents_view);
  on_task_pod_view->OnPageNavigationContextUpdate();
}

bool OnTaskPodControllerImpl::CanNavigateToPreviousPage() {
  if (!browser_) {
    return false;
  }
  return chrome::CanGoBack(browser_.get());
}

bool OnTaskPodControllerImpl::CanNavigateToNextPage() {
  if (!browser_) {
    return false;
  }
  return chrome::CanGoForward(browser_.get());
}

bool OnTaskPodControllerImpl::CanToggleTabStripVisibility() {
  const auto* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_.get());
  return browser_ && platform_util::IsBrowserLockedFullscreen(browser_.get()) &&
         browser_view->immersive_mode_controller()->IsEnabled();
}

const gfx::Rect OnTaskPodControllerImpl::CalculateWidgetBounds() {
  const gfx::Rect parent_window_bounds =
      pod_widget_->parent()->GetWindowBoundsInScreen();
  const gfx::Size preferred_size =
      pod_widget_->GetContentsView()->GetPreferredSize();
  gfx::Point origin;
  switch (pod_snap_location_) {
    case ash::OnTaskPodSnapLocation::kTopLeft:
      origin = gfx::Point(parent_window_bounds.x() + kPodVerticalBorder,
                          parent_window_bounds.y() + frame_header_height_ +
                              kPodHorizontalBorder);
      break;
    case ash::OnTaskPodSnapLocation::kTopRight:
      origin = gfx::Point(parent_window_bounds.right() -
                              preferred_size.width() - kPodVerticalBorder,
                          parent_window_bounds.y() + frame_header_height_ +
                              kPodHorizontalBorder);
  }

  return gfx::Rect(origin, preferred_size);
}

views::Widget* OnTaskPodControllerImpl::GetPodWidgetForTesting() {
  if (!pod_widget_) {
    return nullptr;
  }
  return pod_widget_.get();
}

ImmersiveRevealedLock*
OnTaskPodControllerImpl::GetTabStripRevealLockForTesting() {
  if (!tab_strip_reveal_lock_) {
    return nullptr;
  }
  return tab_strip_reveal_lock_.get();
}

OnTaskPodSnapLocation OnTaskPodControllerImpl::GetSnapLocationForTesting()
    const {
  return pod_snap_location_;
}

}  // namespace ash
