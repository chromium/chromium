// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sync/sessions/browser_list_router_helper.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"
#else
#include "chrome/browser/android/tab_android.h"
#endif  // !BUILDFLAG(IS_ANDROID)
#include "components/history/core/browser/history_service.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"

namespace sync_sessions {

namespace {

SyncedTabDelegate* GetSyncedTabDelegateFromWebContents(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  return tab ? tab->GetSyncedTabDelegate() : nullptr;
#else
  SyncedTabDelegate* delegate =
      BrowserSyncedTabDelegate::FromWebContents(web_contents);
  return delegate;
#endif
}

}  // namespace

SyncSessionsWebContentsRouter::SyncSessionsWebContentsRouter(Profile* profile) {
#if !BUILDFLAG(IS_ANDROID)
  browser_list_helper_ =
      std::make_unique<BrowserListRouterHelper>(this, profile);
#endif  // !BUILDFLAG(IS_ANDROID)
}

SyncSessionsWebContentsRouter::~SyncSessionsWebContentsRouter() = default;

void SyncSessionsWebContentsRouter::NotifyTabModified(
    content::WebContents* web_contents,
    bool page_load_completed) {
  SyncedTabDelegate* delegate = nullptr;
  if (web_contents) {
    delegate = GetSyncedTabDelegateFromWebContents(web_contents);
  }

  if (handler_ && delegate) {
    handler_->OnLocalTabModified(delegate);
  }

  if (!flare_.is_null() && delegate && page_load_completed) {
    flare_.Run(syncer::SESSIONS);
    flare_.Reset();
  }
}

void SyncSessionsWebContentsRouter::NotifySessionRestoreComplete() {
  if (handler_) {
    handler_->OnSessionRestoreComplete();
  }
}

void SyncSessionsWebContentsRouter::InjectStartSyncFlare(
    syncer::SyncableService::StartSyncFlare flare) {
  flare_ = flare;
}

void SyncSessionsWebContentsRouter::StartRoutingTo(
    LocalSessionEventHandler* handler) {
  handler_ = handler;
}

void SyncSessionsWebContentsRouter::Stop() {
  handler_ = nullptr;
}

void SyncSessionsWebContentsRouter::Shutdown() {
#if !BUILDFLAG(IS_ANDROID)
  browser_list_helper_.reset();
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace sync_sessions
