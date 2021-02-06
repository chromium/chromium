// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_IMPL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_IMPL_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/download/download_core_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/buildflags/buildflags.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/download/download_shelf_controller.h"
#endif

class ChromeDownloadManagerDelegate;
class DownloadHistory;
class DownloadUIController;
class ExtensionDownloadsEventRouter;
class Profile;

namespace content {
class DownloadManager;
}

namespace extensions {
class ExtensionDownloadsEventRouter;
}

// Owning class for ChromeDownloadManagerDelegate.
class DownloadCoreServiceImpl : public DownloadCoreService {
 public:
  explicit DownloadCoreServiceImpl(Profile* profile);
  ~DownloadCoreServiceImpl() override;

  // DownloadCoreService
  ChromeDownloadManagerDelegate* GetDownloadManagerDelegate() override;
  DownloadHistory* GetDownloadHistory() override;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::ExtensionDownloadsEventRouter* GetExtensionEventRouter() override;
#endif
  bool HasCreatedDownloadManager() override;
  int NonMaliciousDownloadCount() const override;
  void CancelDownloads() override;
  void SetDownloadManagerDelegateForTesting(
      std::unique_ptr<ChromeDownloadManagerDelegate> delegate) override;
  bool IsShelfEnabled() override;
  void SetDownloadHistoryForTesting(
      std::unique_ptr<DownloadHistory> download_history) override;

  // KeyedService
  void Shutdown() override;

 private:
  bool download_manager_created_;
  Profile* profile_;

  // ChromeDownloadManagerDelegate may be the target of callbacks from
  // the history service/DB thread and must be kept alive for those
  // callbacks.
  std::unique_ptr<ChromeDownloadManagerDelegate> manager_delegate_;

  std::unique_ptr<DownloadHistory> download_history_;

  // The UI controller is responsible for observing the download manager and
  // notifying the UI of any new downloads. Its lifetime matches that of the
  // associated download manager.
  // Note on destruction order: download_ui_ depends on download_history_ and
  // should be destroyed before the latter.
  std::unique_ptr<DownloadUIController> download_ui_;

#if !defined(OS_ANDROID)
  std::unique_ptr<DownloadShelfController> download_shelf_controller_;
#endif

// On Android, GET downloads are not handled by the DownloadManager.
// Once we have extensions on android, we probably need the EventRouter
// in ContentViewDownloadDelegate which knows about both GET and POST
// downloads.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The ExtensionDownloadsEventRouter dispatches download creation, change, and
  // erase events to extensions. Like ChromeDownloadManagerDelegate, it's a
  // chrome-level concept and its lifetime should match DownloadManager. There
  // should be a separate EDER for on-record and off-record managers.
  // There does not appear to be a separate ExtensionSystem for on-record and
  // off-record profiles, so ExtensionSystem cannot own the EDER.
  std::unique_ptr<extensions::ExtensionDownloadsEventRouter>
      extension_event_router_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DownloadCoreServiceImpl);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_CORE_SERVICE_IMPL_H_
