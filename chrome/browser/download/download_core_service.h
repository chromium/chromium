// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/download/download_history.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/buildflags/buildflags.h"

class ChromeDownloadManagerDelegate;
class ExtensionDownloadsEventRouter;

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
  DownloadCoreService();
  ~DownloadCoreService() override;

  // Get the download manager delegate, creating it if it doesn't already exist.
  virtual ChromeDownloadManagerDelegate* GetDownloadManagerDelegate() = 0;

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

  // Number of non-malicious downloads associated with this instance of the
  // service.
  virtual int NonMaliciousDownloadCount() const = 0;

  // Cancels all in-progress downloads for this profile.
  virtual void CancelDownloads() = 0;

  // Number of non-malicious downloads associated with all profiles.
  static int NonMaliciousDownloadCountAllProfiles();

  // Cancels all in-progress downloads for all profiles.
  static void CancelAllDownloads();

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

  // Returns false if at least one extension has disabled the shelf, true
  // otherwise.
  virtual bool IsShelfEnabled() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadCoreService);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_H_
