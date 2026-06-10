// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_WINDOW_OCCLUSION_HELPER_WIDGET_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_WINDOW_OCCLUSION_HELPER_WIDGET_H_

#include "base/containers/flat_set.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_window_occlusion_helper_base.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Widget;
}  // namespace views

// This class tracks the window occlusion state of a given WebContents by
// monitoring other Browser widgets to determine if the WebContents' widget is
// occluded. It does not detect if another application's window occludes the
// WebContent's widget. It also only checks whether a single browser window
// fully occludes the given WebContents' widget. It does not check to see
// whether a combination of partial occlusions from those widgets occludes the
// WebContent's widget.
class AutoPictureInPictureWindowOcclusionHelperWidget
    : public AutoPictureInPictureWindowOcclusionHelperBase,
      public views::WidgetObserver,
      public BrowserCollectionObserver {
 public:
  AutoPictureInPictureWindowOcclusionHelperWidget(
      content::WebContents* web_contents,
      OcclusionStateChangedCallback callback);
  AutoPictureInPictureWindowOcclusionHelperWidget(
      const AutoPictureInPictureWindowOcclusionHelperWidget&) = delete;
  AutoPictureInPictureWindowOcclusionHelperWidget& operator=(
      const AutoPictureInPictureWindowOcclusionHelperWidget&) = delete;
  ~AutoPictureInPictureWindowOcclusionHelperWidget() override;

  // AutoPictureInPictureWindowOcclusionHelperBase:
  void StartObserving() override;
  void StopObserving() override;
  OcclusionState GetOcclusionState() const override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;

 private:
  views::Widget* GetCurrentWidget() const;
  void StartObservingWidget(views::Widget* widget);
  void StopObservingWidget(views::Widget* widget);
  void MaybeScheduleOcclusionStateUpdate();
  void UpdateOcclusionState();

  bool is_observing_ = false;
  OcclusionState current_occlusion_state_ = OcclusionState::kHidden;
  base::OneShotTimer occlusion_update_timer_;
  base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_AUTO_PICTURE_IN_PICTURE_WINDOW_OCCLUSION_HELPER_WIDGET_H_
