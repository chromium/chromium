// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_

#include "base/power_monitor/power_observer.h"
#include "base/timer/timer.h"
#include "chrome/browser/download/bubble/download_icon_state.h"
#include "chrome/browser/download/offline_item_model.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_content_provider.h"

class DownloadBubbleUIController;

namespace base {
class TimeDelta;
class OneShotTimer;
}  // namespace base

class DownloadDisplay;

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

  // Information extracted from iterating over all models, to avoid having to do
  // so multiple times.
  struct AllDownloadUIModelsInfo {
    // Number of models that would be returned to display.
    size_t all_models_size = 0;
    // The last time that a download was completed. Will be null if no downloads
    // were completed.
    base::Time last_completed_time;
    // Whether there are any downloads actively doing deep scanning.
    bool has_deep_scanning = false;
    // Whether any downloads are unactioned.
    bool has_unactioned = false;
    // From the button UI's perspective, whether the download is considered in
    // progress. Consider dangerous downloads as completed, because we don't
    // want to encourage users to interact with them.
    int in_progress_count = 0;
    // Count of in-progress downloads (by the above definition) that are paused.
    int paused_count = 0;

    // Returns a reference to a singleton empty struct. This is for callers who
    // return references but don't have anything to return in some cases.
    static const AllDownloadUIModelsInfo& EmptyInfo();
  };

  struct ProgressInfo {
    bool progress_certain = true;
    int progress_percentage = 0;
    int download_count = 0;
  };

  struct IconInfo {
    download::DownloadIconState icon_state =
        download::DownloadIconState::kComplete;
    bool is_active = false;
  };

  // Returns a ProgressInfo where |download_count| is the number of currently
  // active downloads. If we know the final size of all downloads,
  // |progress_certain| is true. |progress_percentage| is the percentage
  // complete of all in-progress downloads. Forwards to the
  // DownloadBubbleUpdateService.
  ProgressInfo GetProgress();

  // Returns an IconInfo that contains current state of the icon.
  IconInfo GetIconInfo();

  // Returns whether the display is showing details.
  bool IsDisplayShowingDetails();

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

 private:
  friend class DownloadDisplayControllerTest;

  // Gets info about all models to display, then updates the toolbar button
  // state accordingly. Returns the info about all models.
  const AllDownloadUIModelsInfo& UpdateButtonStateFromAllModelsInfo();

  // Stops and restarts `icon_disappearance_timer_`. The toolbar button will
  // be hidden after the `interval`.
  void ScheduleToolbarDisappearance(base::TimeDelta interval);
  // Stops and restarts `icon_inactive_timer_`. The toolbar button will
  // be changed to inactive state after the `interval`.
  void ScheduleToolbarInactive(base::TimeDelta interval);

  // Asks `display_` to show the toolbar button. Does nothing if the toolbar
  // button is already showing.
  void ShowToolbarButton();

  // Updates the icon state of the `display_`.
  void UpdateToolbarButtonState(const AllDownloadUIModelsInfo& info);
  // Asks `display_` to make the download icon inactive.
  void UpdateDownloadIconToInactive();

  // Decides whether the toolbar button should be shown when it is created.
  virtual void MaybeShowButtonWhenCreated();
  // Whether the last download complete time is less than `interval` ago.
  bool HasRecentCompleteDownload(base::TimeDelta interval,
                                 base::Time last_complete_time);

  base::Time GetLastCompleteTime(
      base::Time last_completed_time_from_current_models) const;

  // The pointer is created in ToolbarView and owned by ToolbarView.
  raw_ptr<DownloadDisplay> const display_;
  raw_ptr<Browser> browser_;
  base::ScopedObservation<FullscreenController, FullscreenObserver>
      observation_{this};
  base::OneShotTimer icon_disappearance_timer_;
  base::OneShotTimer icon_inactive_timer_;
  IconInfo icon_info_;
  bool fullscreen_notification_shown_ = false;
  bool should_show_details_on_exit_fullscreen_ = false;
  // DownloadDisplayController and DownloadBubbleUIController have the same
  // lifetime. Both are owned, constructed together, and destructed together by
  // DownloadToolbarButtonView. If one is valid, so is the other.
  raw_ptr<DownloadBubbleUIController, DanglingUntriaged> bubble_controller_;

  base::WeakPtrFactory<DownloadDisplayController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_
