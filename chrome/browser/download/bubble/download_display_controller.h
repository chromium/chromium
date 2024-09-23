// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_

#include "base/power_monitor/power_observer.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/offline_item_model.h"
#include "chrome/browser/ui/download/download_display.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"

struct DownloadBubbleDisplayInfo;
class DownloadBubbleUIController;
class DownloadDisplay;

namespace base {
class TimeDelta;
class OneShotTimer;
}  // namespace base

namespace offline_items_collection {
struct ContentId;
}

// Used to control the DownloadToolbar Button, through the DownloadDisplay
// interface. Supports both regular Download and Offline items. When in the
// future OfflineItems include regular Download on Desktop platforms,
// we can remove AllDownloadItemNotifier::Observer.
// TODO(chlily): Consolidate this with DownloadBubbleUIController.
class DownloadDisplayController : public FullscreenObserver,
                                  public base::PowerSuspendObserver {
 public:
  DownloadDisplayController(DownloadDisplay* display,
                            Browser* browser,
                            DownloadBubbleUIController* bubble_controller);
  DownloadDisplayController(const DownloadDisplayController&) = delete;
  DownloadDisplayController& operator=(const DownloadDisplayController&) =
      delete;
  ~DownloadDisplayController() override;

  // Notifies the controller that the button is pressed. Called by `display_`.
  void OnButtonPressed();

  // Handles the button pressed event. Called by the profile level controller.
  void HandleButtonPressed();

  // Common methods for new downloads or new offline items.
  // These methods are virtual so that they can be overridden for fake
  // controllers in testing.

  // Called from bubble controller when new item(s) are added.
  // |show_animation| specifies whether a small animated arrow should be shown.
  virtual void OnNewItem(bool show_animation);
  // Called from bubble controller when an item is updated, with |is_done|
  // indicating if it was marked done, and with |may_show_details| indicating
  // whether the partial view can be shown. (Whether the partial view is
  // actually shown may depend on the state of the other downloads.)
  virtual void OnUpdatedItem(bool is_done, bool may_show_details);
  // Called from bubble controller when an item is deleted.
  virtual void OnRemovedItem(const ContentId& id);

  // Asks `display_` to hide the toolbar button. Does nothing if the toolbar
  // button is already hidden.
  void HideToolbarButton();
  // Asks `display_` to hide the toolbar button details. Does nothing if the
  // details are already hidden.
  void HideBubble();

  // Opens the primary dialog to the item and scrolls to the item, and opens
  // the security dialog if the item has a security warning. Returns whether
  // bubble was opened to the requested item.
  // Note: This method is currently used only for Lacros download notifications.
  // It does not explicitly handle fullscreen conditions. See comment in
  // implementation. In the future if there are other entry points to this
  // method, non-immersive fullscreen (i.e. exclusive access bubble) will have
  // to be handled explicitly.
  bool OpenMostSpecificDialog(
      const offline_items_collection::ContentId& content_id);

  // Start listening to full screen changes. This is separate from the
  // constructor as the exclusive access manager is constructed after
  // BrowserWindow.
  void ListenToFullScreenChanges();

  // FullScreenObserver
  void OnFullscreenStateChanged() override;

  // PowerSuspendObserver
  void OnResume() override;

  // Returns the DownloadDisplay. Should always return a valid display.
  DownloadDisplay* download_display_for_testing() { return display_; }

  void OpenSecuritySubpage(const offline_items_collection::ContentId& id);

 private:
  friend class DownloadDisplayControllerTest;

  // Gets info about all models to display and progress ring info from the
  // update service, then updates the toolbar button state accordingly. Returns
  // the info about all models.
  const DownloadBubbleDisplayInfo& UpdateButtonStateFromUpdateService();

  // Obtains and announces accessible alerts from the update service.
  void HandleAccessibleAlerts();

  // Stops and restarts `icon_disappearance_timer_`. The toolbar button will
  // be hidden after the `interval`.
  void ScheduleToolbarDisappearance(base::TimeDelta interval);
  // Stops and restarts `icon_inactive_timer_`. The toolbar button will
  // be changed to inactive state after the `interval`.
  void ScheduleToolbarInactive(base::TimeDelta interval);

  // Asks `display_` to show the toolbar button. Does nothing if the toolbar
  // button is already showing.
  void ShowToolbarButton();

  // Updates the icon state of the `display_`, including the progress ring.
  void UpdateToolbarButtonState(
      const DownloadBubbleDisplayInfo& info,
      const DownloadDisplay::ProgressInfo& progress_info);

  // Asks `display_` to make the download icon inactive.
  void UpdateDownloadIconToInactive();

  // Decides whether the toolbar button should be shown when it is created.
  virtual void MaybeShowButtonWhenCreated();

  // The pointer is created in ToolbarView and owned by ToolbarView.
  raw_ptr<DownloadDisplay> const display_;
  raw_ptr<Browser> browser_;
  base::ScopedObservation<FullscreenController, FullscreenObserver>
      observation_{this};
  base::OneShotTimer icon_disappearance_timer_;
  base::OneShotTimer icon_inactive_timer_;
  bool fullscreen_notification_shown_ = false;
  bool should_show_details_on_exit_fullscreen_ = false;
  // DownloadDisplayController and DownloadBubbleUIController have the same
  // lifetime. Both are owned, constructed together, and destructed together by
  // DownloadToolbarButtonView. If one is valid, so is the other.
  raw_ptr<DownloadBubbleUIController, DanglingUntriaged> bubble_controller_;

  base::WeakPtrFactory<DownloadDisplayController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_
