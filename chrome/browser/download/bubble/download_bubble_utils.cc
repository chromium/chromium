// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/bubble/download_bubble_utils.h"

#include "base/time/time.h"
#include "chrome/browser/download/download_ui_model.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "components/offline_items_collection/core/offline_item_state.h"

base::Time GetItemStartTime(const download::DownloadItem* item) {
  return item->GetStartTime();
}

base::Time GetItemStartTime(const offline_items_collection::OfflineItem& item) {
  return item.creation_time;
}

const std::string& GetItemId(const download::DownloadItem* item) {
  return item->GetGuid();
}

const offline_items_collection::ContentId& GetItemId(
    const offline_items_collection::OfflineItem& item) {
  return item.id;
}

bool ItemIsRecent(const download::DownloadItem* item, base::Time cutoff_time) {
  return ((item->GetStartTime().is_null() && !item->IsDone()) ||
          item->GetStartTime() > cutoff_time);
}

bool ItemIsRecent(const offline_items_collection::OfflineItem& item,
                  base::Time cutoff_time) {
  // TODO(chlily): Deduplicate this code from OfflineItemModel::IsDone().
  bool is_done = false;
  switch (item.state) {
    case offline_items_collection::OfflineItemState::IN_PROGRESS:
    case offline_items_collection::OfflineItemState::PAUSED:
    case offline_items_collection::OfflineItemState::PENDING:
      break;
    case offline_items_collection::OfflineItemState::INTERRUPTED:
      is_done = item.is_resumable;
      break;
    case offline_items_collection::OfflineItemState::FAILED:
    case offline_items_collection::OfflineItemState::COMPLETE:
    case offline_items_collection::OfflineItemState::CANCELLED:
      is_done = true;
      break;
    case offline_items_collection::OfflineItemState::NUM_ENTRIES:
      NOTREACHED();
  }
  return ((item.creation_time.is_null() && !is_done) ||
          item.creation_time > cutoff_time);
}

bool DownloadUIModelIsRecent(const DownloadUIModel* model,
                             base::Time cutoff_time) {
  return ((model->GetStartTime().is_null() && !model->IsDone()) ||
          model->GetStartTime() > cutoff_time);
}

bool IsPendingDeepScanning(const download::DownloadItem* item) {
  return item->GetState() == download::DownloadItem::IN_PROGRESS &&
         item->GetDangerType() ==
             download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING;
}

bool IsPendingDeepScanning(const DownloadUIModel* model) {
  return model->GetState() == download::DownloadItem::IN_PROGRESS &&
         model->GetDangerType() ==
             download::DOWNLOAD_DANGER_TYPE_PROMPT_FOR_SCANNING;
}

bool IsItemInProgress(const download::DownloadItem* item) {
  if (item->IsDangerous() && !IsPendingDeepScanning(item)) {
    return false;
  }
  return item->GetState() == download::DownloadItem::IN_PROGRESS;
}

bool IsItemInProgress(const offline_items_collection::OfflineItem& item) {
  // Offline items cannot be pending deep scanning.
  if (item.is_dangerous) {
    return false;
  }
  return item.state ==
             offline_items_collection::OfflineItemState::IN_PROGRESS ||
         item.state == offline_items_collection::OfflineItemState::PAUSED;
}

bool IsModelInProgress(const DownloadUIModel* model) {
  if (model->IsDangerous() && !IsPendingDeepScanning(model)) {
    return false;
  }
  return model->GetState() == download::DownloadItem::IN_PROGRESS;
}

bool IsItemPaused(const download::DownloadItem* item) {
  return item->IsPaused();
}

bool IsItemPaused(const offline_items_collection::OfflineItem& item) {
  return item.state == offline_items_collection::OfflineItemState::PAUSED;
}
