// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_H_

#include <memory>

#include "chrome/browser/download/download_history.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/buildflags/buildflags.h"

class ChromeDownloadManagerDelegate;
class DownloadUIController;

namespace content {
class DownloadManager;
}

namespace extensions {
class ExtensionDownloadsEventRouter;
}

// Abstract base class for the download core service; see
// DownloadCoreServiceImpl for implementation.
class DownloadCoreService : public KeyedService {
 public:
  // This enum represents when `CancelDownloads` is called.
  enum class CancelDownloadsTrigger {
    kShutdown = 0,
    kProfileDeletion = 1,
  };

  DownloadCoreService();

  DownloadCoreService(const DownloadCoreService&) = delete;
  DownloadCoreService& operator=(const DownloadCoreService&) = delete;

  ~DownloadCoreService() override;

  // Get the download manager delegate, creating it if it doesn't already exist.
  virtual ChromeDownloadManagerDelegate* GetDownloadManagerDelegate() = 0;

  // Get the download UI controller, return nullptr if it doesn't already exist.
  virtual DownloadUIController* GetDownloadUIController() = 0;

  // Get the interface to the history system. Returns NULL if profile is
  // incognito or if the DownloadManager hasn't been created yet or if there is
  // no HistoryService for profile. Virtual for testing.
  virtual DownloadHistory* GetDownloadHistory() = 0;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  virtual extensions::ExtensionDownloadsEventRouter*
  GetExtensionEventRouter() = 0;
#endif

  // Has a download manager been created?
  virtual bool HasCreatedDownloadManager() = 0;

  // Number of downloads blocking shutdown associated with this instance of the
  // service.
  virtual int BlockingShutdownCount() const = 0;

  // Cancels all in-progress downloads for this profile.
  virtual void CancelDownloads(CancelDownloadsTrigger trigger) = 0;

  // Number of downloads blocking shutdown associated with all profiles.
  static int BlockingShutdownCountAllProfiles();

  // Cancels all in-progress downloads for all profiles.
  static void CancelAllDownloads(CancelDownloadsTrigger trigger);

  // Sets the DownloadManagerDelegate associated with this object and
  // its DownloadManager.  Takes ownership of |delegate|, and destroys
  // the previous delegate.  For testing.
  virtual void SetDownloadManagerDelegateForTesting(
      std::unique_ptr<ChromeDownloadManagerDelegate> delegate) = 0;

  // Sets the DownloadHistory associated with this object and
  // its DownloadManager. Takes ownership of |download_history|, and destroys
  // the previous delegate.  For testing.
  virtual void SetDownloadHistoryForTesting(
      std::unique_ptr<DownloadHistory> download_history) {}

  // Returns false if at least one extension has disabled the UI, true
  // otherwise.
  virtual bool IsDownloadUiEnabled() = 0;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_H_
