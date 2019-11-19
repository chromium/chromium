// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_core_service_impl.h"

#include "base/callback.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_offline_content_provider.h"
#include "chrome/browser/download/download_offline_content_provider_factory.h"
#include "chrome/browser/download/download_status_updater.h"
#include "chrome/browser/download/download_ui_controller.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/download_manager.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/download/android/download_utils.h"
#endif

using content::BrowserContext;
using content::DownloadManager;
using content::DownloadManagerDelegate;

DownloadCoreServiceImpl::DownloadCoreServiceImpl(Profile* profile)
    : download_manager_created_(false), profile_(profile) {}

DownloadCoreServiceImpl::~DownloadCoreServiceImpl() {}

ChromeDownloadManagerDelegate*
DownloadCoreServiceImpl::GetDownloadManagerDelegate() {
  DownloadManager* manager = BrowserContext::GetDownloadManager(profile_);
  // If we've already created the delegate, just return it.
  if (download_manager_created_)
    return manager_delegate_.get();
  download_manager_created_ = true;
  download::SimpleDownloadManagerCoordinator* coordinator =
      SimpleDownloadManagerCoordinatorFactory::GetForKey(
          profile_->GetProfileKey());
  coordinator->SetSimpleDownloadManager(manager, true);

  // In case the delegate has already been set by
  // SetDownloadManagerDelegateForTesting.
  if (!manager_delegate_.get())
    manager_delegate_.reset(new ChromeDownloadManagerDelegate(profile_));

  manager_delegate_->SetDownloadManager(manager);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_event_router_.reset(
      new extensions::ExtensionDownloadsEventRouter(profile_, manager));
#endif

  if (!profile_->IsOffTheRecord()) {
    history::HistoryService* history = HistoryServiceFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS);
    history->GetNextDownloadId(
        manager_delegate_->GetDownloadIdReceiverCallback());
    download_history_.reset(new DownloadHistory(
        manager, std::unique_ptr<DownloadHistory::HistoryAdapter>(
                     new DownloadHistory::HistoryAdapter(history))));
  }

  // Pass an empty delegate when constructing the DownloadUIController. The
  // default delegate does all the notifications we need.
  download_ui_.reset(new DownloadUIController(
      manager, std::unique_ptr<DownloadUIController::Delegate>()));

#if !defined(OS_ANDROID)
  download_shelf_controller_.reset(new DownloadShelfController(profile_));
#endif

  // Include this download manager in the set monitored by the
  // global status updater.
  DCHECK(g_browser_process->download_status_updater());
  g_browser_process->download_status_updater()->AddManager(manager);

  return manager_delegate_.get();
}

DownloadHistory* DownloadCoreServiceImpl::GetDownloadHistory() {
  if (!download_manager_created_) {
    GetDownloadManagerDelegate();
  }
  DCHECK(download_manager_created_);
  return download_history_.get();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
extensions::ExtensionDownloadsEventRouter*
DownloadCoreServiceImpl::GetExtensionEventRouter() {
  return extension_event_router_.get();
}
#endif

bool DownloadCoreServiceImpl::HasCreatedDownloadManager() {
  return download_manager_created_;
}

int DownloadCoreServiceImpl::NonMaliciousDownloadCount() const {
  if (!download_manager_created_)
    return 0;
  return BrowserContext::GetDownloadManager(profile_)
      ->NonMaliciousInProgressCount();
}

void DownloadCoreServiceImpl::CancelDownloads() {
  if (!download_manager_created_)
    return;

  DownloadManager* download_manager =
      BrowserContext::GetDownloadManager(profile_);
  DownloadManager::DownloadVector downloads;
  download_manager->GetAllDownloads(&downloads);
  for (auto it = downloads.begin(); it != downloads.end(); ++it) {
    if ((*it)->GetState() == download::DownloadItem::IN_PROGRESS)
      (*it)->Cancel(false);
  }
}

void DownloadCoreServiceImpl::SetDownloadManagerDelegateForTesting(
    std::unique_ptr<ChromeDownloadManagerDelegate> new_delegate) {
  manager_delegate_.swap(new_delegate);
  DownloadManager* dm = BrowserContext::GetDownloadManager(profile_);
  dm->SetDelegate(manager_delegate_.get());
  if (manager_delegate_)
    manager_delegate_->SetDownloadManager(dm);
  download_manager_created_ = !!manager_delegate_;
  if (new_delegate)
    new_delegate->Shutdown();
}

void DownloadCoreServiceImpl::SetDownloadHistoryForTesting(
    std::unique_ptr<DownloadHistory> download_history) {
  download_history_ = std::move(download_history);
}

bool DownloadCoreServiceImpl::IsShelfEnabled() {
#if defined(OS_ANDROID)
  return true;
#else
  return !extension_event_router_ || extension_event_router_->IsShelfEnabled();
#endif
}

void DownloadCoreServiceImpl::Shutdown() {
  if (download_manager_created_) {
    // Normally the DownloadManager would be shutdown later, after the Profile
    // goes away and BrowserContext's destructor runs. But that would be too
    // late for us since we need to use the profile (indirectly through history
    // code) when the DownloadManager is shutting down. So we shut it down
    // manually earlier. See http://crbug.com/131692
    BrowserContext::GetDownloadManager(profile_)->Shutdown();
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_event_router_.reset();
#endif
  manager_delegate_.reset();
  download_history_.reset();
}
