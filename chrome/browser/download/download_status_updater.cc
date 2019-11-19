// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include <stdint.h>

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"

namespace {

// DownloadStatusUpdater::UpdateAppIconDownloadProgress() expects to only be
// called once when a DownloadItem completes, then not again (except perhaps
// until it is resumed). The existence of WasInProgressData is effectively a
// boolean that indicates whether that final UpdateAppIconDownloadProgress()
// call has been made for a given DownloadItem. It is expected that there will
// be many more non-in-progress downloads than in-progress downloads, so
// WasInProgressData is set for in-progress downloads and cleared from
// non-in-progress downloads instead of the other way around in order to save
// memory.
class WasInProgressData : public base::SupportsUserData::Data {
 public:
  static bool Get(download::DownloadItem* item) {
    return item->GetUserData(kKey) != NULL;
  }

  static void Clear(download::DownloadItem* item) {
    item->RemoveUserData(kKey);
  }

  explicit WasInProgressData(download::DownloadItem* item) {
    item->SetUserData(kKey, base::WrapUnique(this));
  }

 private:
  static const char kKey[];
  DISALLOW_COPY_AND_ASSIGN(WasInProgressData);
};

const char WasInProgressData::kKey[] =
  "DownloadItem DownloadStatusUpdater WasInProgressData";

}  // anonymous namespace

DownloadStatusUpdater::DownloadStatusUpdater() {
}

DownloadStatusUpdater::~DownloadStatusUpdater() {
}

bool DownloadStatusUpdater::GetProgress(float* progress,
                                        int* download_count) const {
  *progress = 0;
  *download_count = 0;
  bool progress_certain = true;
  int64_t received_bytes = 0;
  int64_t total_bytes = 0;

  for (const auto& notifier : notifiers_) {
    if (notifier->GetManager()) {
      content::DownloadManager::DownloadVector items;
      notifier->GetManager()->GetAllDownloads(&items);
      for (auto* item : items) {
        if (item->GetState() == download::DownloadItem::IN_PROGRESS) {
          ++*download_count;
          if (item->GetTotalBytes() <= 0) {
            // There may or may not be more data coming down this pipe.
            progress_certain = false;
          } else {
            received_bytes += item->GetReceivedBytes();
            total_bytes += item->GetTotalBytes();
          }
        }
      }
    }
  }

  if (total_bytes > 0)
    *progress = static_cast<float>(received_bytes) / total_bytes;
  return progress_certain;
}

void DownloadStatusUpdater::AddManager(content::DownloadManager* manager) {
  notifiers_.push_back(
      std::make_unique<download::AllDownloadItemNotifier>(manager, this));
  content::DownloadManager::DownloadVector items;
  manager->GetAllDownloads(&items);
  for (auto* item : items)
    OnDownloadCreated(manager, item);
}

void DownloadStatusUpdater::OnDownloadCreated(content::DownloadManager* manager,
                                              download::DownloadItem* item) {
  // Ignore downloads loaded from history, which are in a terminal state.
  // TODO(benjhayden): Use the Observer interface to distinguish between
  // historical and started downloads.
  if (item->GetState() == download::DownloadItem::IN_PROGRESS) {
    UpdateAppIconDownloadProgress(item);
    new WasInProgressData(item);
  }
  // else, the lack of WasInProgressData indicates to OnDownloadUpdated that it
  // should not call UpdateAppIconDownloadProgress().
}

void DownloadStatusUpdater::OnDownloadUpdated(content::DownloadManager* manager,
                                              download::DownloadItem* item) {
  if (item->GetState() == download::DownloadItem::IN_PROGRESS) {
    // If the item was interrupted/cancelled and then resumed/restarted, then
    // set WasInProgress so that UpdateAppIconDownloadProgress() will be called
    // when it completes.
    if (!WasInProgressData::Get(item))
      new WasInProgressData(item);
  } else {
    // The item is now in a terminal state. If it was already in a terminal
    // state, then do not call UpdateAppIconDownloadProgress() again. If it is
    // now transitioning to a terminal state, then clear its WasInProgressData
    // so that UpdateAppIconDownloadProgress() won't be called after this final
    // call.
    if (!WasInProgressData::Get(item))
      return;
    WasInProgressData::Clear(item);
  }
  UpdateAppIconDownloadProgress(item);
}

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    download::DownloadItem* download) {
  // TODO(avi): Implement for Android?
}
#endif
