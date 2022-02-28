// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_

#include "base/timer/timer.h"
#include "components/download/content/public/all_download_item_notifier.h"

namespace content {
class DownloadManager;
}  // namespace content

namespace base {
class TimeDelta;
class OneShotTimer;
}  // namespace base

class DownloadDisplay;

class DownloadDisplayController
    : public download::AllDownloadItemNotifier::Observer {
 public:
  DownloadDisplayController(DownloadDisplay* display,
                            content::DownloadManager* download_manager);
  DownloadDisplayController(const DownloadDisplayController&) = delete;
  DownloadDisplayController& operator=(const DownloadDisplayController&) =
      delete;
  ~DownloadDisplayController() override;

  struct ProgressInfo {
    bool progress_certain = true;
    int progress_percentage = 0;
    int download_count = 0;
  };

  // Returns a ProgressInfo where |download_count| is the number of currently
  // active downloads. If we know the final size of all downloads,
  // |progress_certain| is true. |progress_percentage| is the percentage
  // complete of all in-progress  downloads.
  //
  // This implementation will match the one in download_status_updater.cc
  ProgressInfo GetProgress();

  // Asks `display_` to show the toolbar button. Does nothing if the toolbar
  // button is already showing.
  void ShowToolbarButton();
  // Asks `display_` to hide the toolbar button. Does nothing if the toolbar
  // button is already hidden.
  void HideToolbarButton();

  download::AllDownloadItemNotifier& get_download_notifier_for_testing() {
    return download_notifier_;
  }

 private:
  friend class DownloadDisplayControllerTest;

  // Stops and restarts `icon_disappearance_timer_`. The toolbar button will
  // be hidden after the `interval`.
  void ScheduleToolbarDisappearance(base::TimeDelta interval);

  // Based on the information from `download_manager_`, updates the icon state
  // of the `display_`.
  void UpdateToolbarButtonState();

  // Decides whether the toolbar button should be shown when it is created.
  void MaybeShowButtonWhenCreated();

  // AllDownloadItemNotifier::Observer
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnManagerGoingDown(content::DownloadManager* manager) override;

  // The pointer is created in ToolbarView and owned by ToolbarView.
  DownloadDisplay* const display_;
  content::DownloadManager* download_manager_;
  download::AllDownloadItemNotifier download_notifier_;
  base::OneShotTimer icon_disappearance_timer_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_DISPLAY_CONTROLLER_H_
