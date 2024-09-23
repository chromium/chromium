// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UTILS_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UTILS_H_

#include "base/time/time.h"
#include "chrome/browser/download/bubble/download_bubble_accessible_alerts_map.h"
#include "chrome/browser/download/download_ui_model.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/webapps/common/web_app_id.h"

base::Time GetItemStartTime(const download::DownloadItem* item);
base::Time GetItemStartTime(const offline_items_collection::OfflineItem& item);

const std::string& GetItemId(const download::DownloadItem* item);
const offline_items_collection::ContentId& GetItemId(
    const offline_items_collection::OfflineItem& item);

// Whether the download is more recent than |cutoff_time|.
bool ItemIsRecent(const download::DownloadItem* item, base::Time cutoff_time);
bool ItemIsRecent(const offline_items_collection::OfflineItem& item,
                  base::Time cutoff_time);
bool DownloadUIModelIsRecent(const DownloadUIModel* model,
                             base::Time cutoff_time);

// Whether the download is in progress and pending deep scanning.
bool IsPendingDeepScanning(const download::DownloadItem* item);
bool IsPendingDeepScanning(const DownloadUIModel* model);

// Whether the download is considered in-progress from the UI's point of view.
// Consider dangerous downloads as completed, because we don't want to encourage
// users to interact with them. Items that are paused count as in-progress.
bool IsItemInProgress(const download::DownloadItem* item);
bool IsItemInProgress(const offline_items_collection::OfflineItem& item);
bool IsModelInProgress(const DownloadUIModel* model);

// Whether the item is paused.
bool IsItemPaused(const download::DownloadItem* item);
bool IsItemPaused(const offline_items_collection::OfflineItem& item);

// Gets the appropriate accessible alert based on the status of the download.
// This should be called upon receiving an update for a download.
DownloadBubbleAccessibleAlertsMap::Alert GetAccessibleAlertForModel(
    const DownloadUIModel& model);

// Finds the browser most appropriate to show the "download started" animation
// in.
Browser* FindBrowserToShowAnimation(download::DownloadItem* item,
                                    Profile* profile);

// Gets a pointer to the web app id, if the browser is for a web app, otherwise
// nullptr.
const webapps::AppId* GetWebAppIdForBrowser(const Browser* browser);

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UTILS_H_
