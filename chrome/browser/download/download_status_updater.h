// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATUS_UPDATER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATUS_UPDATER_H_

#include <memory>
#include <set>

#include "build/chromeos_buildflags.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/download_manager.h"

class Profile;
class ScopedProfileKeepAlive;

// Keeps track of download progress for the entire browser.
class DownloadStatusUpdater
    : public download::AllDownloadItemNotifier::Observer {
 public:
  DownloadStatusUpdater();

  DownloadStatusUpdater(const DownloadStatusUpdater&) = delete;
  DownloadStatusUpdater& operator=(const DownloadStatusUpdater&) = delete;

  ~DownloadStatusUpdater() override;

  // Fills in |*download_count| with the number of currently active downloads.
  // If we know the final size of all downloads, this routine returns true
  // with |*progress| set to the percentage complete of all in-progress
  // downloads.  Otherwise, it returns false.
  bool GetProgress(float* progress, int* download_count) const;

  // Add the specified DownloadManager to the list of managers for which
  // this object reports status.
  // The manager must not have previously been added to this updater.
  // The updater will automatically disassociate itself from the
  // manager when the manager is shutdown.
  void AddManager(content::DownloadManager* manager);

  // AllDownloadItemNotifier::Observer
  void OnManagerGoingDown(content::DownloadManager* manager) override;
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

 protected:
  // Platform-specific function to update the platform UI for download progress.
  // |download| is the download item that changed. Implementations should not
  // hold the value of |download| as it is not guaranteed to remain valid.
  // Virtual to be overridable for testing.
  virtual void UpdateAppIconDownloadProgress(download::DownloadItem* download);

  // Updates the ScopedProfileKeepAlive for the profile tied to |manager|. If
  // there are in-progress downloads, it will acquire a keepalive. Otherwise, it
  // will release it.
  //
  // This prevents deleting the Profile* too early when there are still
  // in-progress downloads, and the browser is not tearing down yet.
  void UpdateProfileKeepAlive(content::DownloadManager* manager);

 private:
  std::vector<std::unique_ptr<download::AllDownloadItemNotifier>> notifiers_;
  std::map<Profile*, std::unique_ptr<ScopedProfileKeepAlive>>
      profile_keep_alives_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Looks up the DownloadItem* for a given guid, or returns nullptr if none is
  // found.
  download::DownloadItem* GetDownloadItemFromGuid(const std::string& guid);

  class Delegate;
  std::unique_ptr<Delegate> delegate_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_STATUS_UPDATER_H_
