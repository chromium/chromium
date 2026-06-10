// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/auto_picture_in_picture_window_occlusion_helper_widget.h"

#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr base::TimeDelta kUpdateDelay = base::Milliseconds(100);

}  // namespace

// static
std::unique_ptr<AutoPictureInPictureWindowOcclusionHelperBase>
AutoPictureInPictureWindowOcclusionHelperBase::CreatePlatformHelper(
    content::WebContents* web_contents,
    OcclusionStateChangedCallback callback) {
  return std::make_unique<AutoPictureInPictureWindowOcclusionHelperWidget>(
      web_contents, std::move(callback));
}

AutoPictureInPictureWindowOcclusionHelperWidget::
    AutoPictureInPictureWindowOcclusionHelperWidget(
        content::WebContents* web_contents,
        OcclusionStateChangedCallback callback)
    : AutoPictureInPictureWindowOcclusionHelperBase(web_contents,
                                                    std::move(callback)) {}

AutoPictureInPictureWindowOcclusionHelperWidget::
    ~AutoPictureInPictureWindowOcclusionHelperWidget() {
  StopObserving();
}

void AutoPictureInPictureWindowOcclusionHelperWidget::StartObserving() {
  if (is_observing_) {
    return;
  }
  is_observing_ = true;

  // Observe for any new browsers being added.
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());

  // Observe all existing browser widgets so we know when to check for
  // occlusion.
  GlobalBrowserCollection::GetInstance()->ForEach(
      [this](BrowserWindowInterface* browser) {
        BrowserView* browser_view =
            BrowserView::GetBrowserViewForBrowser(browser);
        if (!browser_view) {
          return true;
        }
        views::Widget* widget = browser_view->GetWidget();
        StartObservingWidget(widget);
        return true;
      });

  // Get initial occlusion state.
  current_occlusion_state_ = GetOcclusionState();
}

void AutoPictureInPictureWindowOcclusionHelperWidget::StopObserving() {
  if (!is_observing_) {
    return;
  }
  is_observing_ = false;

  browser_collection_observation_.Reset();
  widget_observations_.RemoveAllObservations();
}

AutoPictureInPictureWindowOcclusionHelperBase::OcclusionState
AutoPictureInPictureWindowOcclusionHelperWidget::GetOcclusionState() const {
  views::Widget* current_widget = GetCurrentWidget();
  if (!current_widget || !current_widget->IsVisibleOnScreen()) {
    return OcclusionState::kHidden;
  }

  const gfx::Rect current_bounds =
      current_widget->GetNonDecoratedClientAreaBoundsInScreen();
  bool is_occluded = false;

  // Iterate over all browser windows in order from most recently activated to
  // least recently activated to check if any of the windows more recently
  // active than ours (and therefore above ours) occludes ours.
  //
  // Note that we only check whether a single browser window fully occludes our
  // window. We do not check to see whether a combination of partial occlusions
  // from browser windows occludes us.
  GlobalBrowserCollection::GetInstance()->ForEach(
      [current_widget, current_bounds,
       &is_occluded](BrowserWindowInterface* browser) {
        // We don't consider picture-in-picture windows when determining
        // occlusion, since if only the picture-in-picture window is occluding
        // the observed window, then we'll want to trigger an exit
        // picture-in-picture action.
        if (browser->GetType() ==
            BrowserWindowInterface::TYPE_PICTURE_IN_PICTURE) {
          return true;  // Continue iterating
        }

        BrowserView* browser_view =
            BrowserView::GetBrowserViewForBrowser(browser);
        views::Widget* other_widget =
            browser_view ? browser_view->GetWidget() : nullptr;

        // Once we've reached our widget, we know we're above the rest of the
        // widgets we care about.
        if (other_widget == current_widget) {
          return false;  // Stop iterating
        }

        if (!other_widget || !other_widget->IsVisibleOnScreen()) {
          return true;  // Continue iterating
        }

        const gfx::Rect other_bounds =
            other_widget->GetNonDecoratedClientAreaBoundsInScreen();
        if (other_bounds.Contains(current_bounds)) {
          is_occluded = true;
          return false;  // Stop iterating
        }
        return true;  // Continue iterating
      },
      BrowserCollection::Order::kActivation);

  return is_occluded ? OcclusionState::kOccluded : OcclusionState::kVisible;
}

void AutoPictureInPictureWindowOcclusionHelperWidget::OnWidgetDestroying(
    views::Widget* widget) {
  StopObservingWidget(widget);
}

void AutoPictureInPictureWindowOcclusionHelperWidget::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  MaybeScheduleOcclusionStateUpdate();
}

void AutoPictureInPictureWindowOcclusionHelperWidget::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  MaybeScheduleOcclusionStateUpdate();
}

void AutoPictureInPictureWindowOcclusionHelperWidget::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }
  views::Widget* widget = browser_view->GetWidget();

  // A browser should not be created without a widget.
  CHECK(widget);

  StartObservingWidget(widget);
  MaybeScheduleOcclusionStateUpdate();
}

void AutoPictureInPictureWindowOcclusionHelperWidget::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }
  views::Widget* widget = browser_view->GetWidget();
  StopObservingWidget(widget);
  MaybeScheduleOcclusionStateUpdate();
}

void AutoPictureInPictureWindowOcclusionHelperWidget::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  MaybeScheduleOcclusionStateUpdate();
}

views::Widget*
AutoPictureInPictureWindowOcclusionHelperWidget::GetCurrentWidget() const {
  return views::Widget::GetWidgetForNativeWindow(
      GetObservedWebContents()->GetTopLevelNativeWindow());
}

void AutoPictureInPictureWindowOcclusionHelperWidget::StartObservingWidget(
    views::Widget* widget) {
  if (!widget || widget_observations_.IsObservingSource(widget)) {
    return;
  }
  widget_observations_.AddObservation(widget);
}

void AutoPictureInPictureWindowOcclusionHelperWidget::StopObservingWidget(
    views::Widget* widget) {
  if (!widget || !widget_observations_.IsObservingSource(widget)) {
    return;
  }
  widget_observations_.RemoveObservation(widget);
}

void AutoPictureInPictureWindowOcclusionHelperWidget::
    MaybeScheduleOcclusionStateUpdate() {
  if (!is_observing_ || occlusion_update_timer_.IsRunning()) {
    return;
  }

  // base::Unretained() is safe here since we own `occlusion_update_timer_`.
  occlusion_update_timer_.Start(
      FROM_HERE, kUpdateDelay,
      base::BindOnce(&AutoPictureInPictureWindowOcclusionHelperWidget::
                         UpdateOcclusionState,
                     base::Unretained(this)));
}

void AutoPictureInPictureWindowOcclusionHelperWidget::UpdateOcclusionState() {
  if (!is_observing_) {
    return;
  }
  OcclusionState new_state = GetOcclusionState();
  if (new_state != current_occlusion_state_) {
    current_occlusion_state_ = new_state;
    RunCallback(current_occlusion_state_);
  }
}
