// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/session_sync_service_factory.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegates_getter.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/session_sync_service_impl.h"
#include "components/sync_sessions/sync_sessions_client.h"

#if defined(OS_ANDROID)
#include "chrome/browser/sync/glue/synced_window_delegates_getter_android.h"
#endif  // defined(OS_ANDROID)

namespace {

bool ShouldSyncURLImpl(const GURL& url) {
  if (url == chrome::kChromeUIHistoryURL) {
    // Whitelist the chrome history page, home for "Tabs from other devices", so
    // it can trigger starting up the sync engine.
    return true;
  }
  return url.is_valid() && !url.SchemeIs(content::kChromeUIScheme) &&
         !url.SchemeIs(chrome::kChromeNativeScheme) && !url.SchemeIsFile();
}

// Chrome implementation of SyncSessionsClient.
class SyncSessionsClientImpl : public sync_sessions::SyncSessionsClient {
 public:
  explicit SyncSessionsClientImpl(Profile* profile)
      : profile_(profile), session_sync_prefs_(profile->GetPrefs()) {
    window_delegates_getter_ =
#if defined(OS_ANDROID)
        // Android doesn't have multi-profile support, so no need to pass the
        // profile in.
        std::make_unique<browser_sync::SyncedWindowDelegatesGetterAndroid>();
#else   // defined(OS_ANDROID)
        std::make_unique<browser_sync::BrowserSyncedWindowDelegatesGetter>(
            profile);
#endif  // defined(OS_ANDROID)
  }

  ~SyncSessionsClientImpl() override {}

  // SyncSessionsClient implementation.
  favicon::FaviconService* GetFaviconService() override {
    return FaviconServiceFactory::GetForProfile(
        profile_, ServiceAccessType::IMPLICIT_ACCESS);
  }

  history::HistoryService* GetHistoryService() override {
    return HistoryServiceFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS);
  }

  sync_sessions::SessionSyncPrefs* GetSessionSyncPrefs() override {
    return &session_sync_prefs_;
  }

  syncer::RepeatingModelTypeStoreFactory GetStoreFactory() override {
    return ModelTypeStoreServiceFactory::GetForProfile(profile_)
        ->GetStoreFactory();
  }

  bool ShouldSyncURL(const GURL& url) const override {
    return ShouldSyncURLImpl(url);
  }

  sync_sessions::SyncedWindowDelegatesGetter* GetSyncedWindowDelegatesGetter()
      override {
    return window_delegates_getter_.get();
  }

  sync_sessions::LocalSessionEventRouter* GetLocalSessionEventRouter()
      override {
    syncer::SyncableService::StartSyncFlare flare(
        sync_start_util::GetFlareForSyncableService(profile_->GetPath()));
    sync_sessions::SyncSessionsWebContentsRouter* router =
        sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
            profile_);
    // TODO(mastiz): Injecting a start flare as a side effect of what seems to
    // be a getter is error-prone. In fact, we seem to call this function very
    // early for the USS implementation.
    router->InjectStartSyncFlare(flare);
    return router;
  }

 private:
  Profile* const profile_;
  std::unique_ptr<sync_sessions::SyncedWindowDelegatesGetter>
      window_delegates_getter_;
  sync_sessions::SessionSyncPrefs session_sync_prefs_;

  DISALLOW_COPY_AND_ASSIGN(SyncSessionsClientImpl);
};

}  // namespace

// static
sync_sessions::SessionSyncService* SessionSyncServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<sync_sessions::SessionSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SessionSyncServiceFactory* SessionSyncServiceFactory::GetInstance() {
  return base::Singleton<SessionSyncServiceFactory>::get();
}

// static
bool SessionSyncServiceFactory::ShouldSyncURLForTesting(const GURL& url) {
  return ShouldSyncURLImpl(url);
}

SessionSyncServiceFactory::SessionSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SessionSyncService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(FaviconServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(sync_sessions::SyncSessionsWebContentsRouterFactory::GetInstance());
}

SessionSyncServiceFactory::~SessionSyncServiceFactory() {}

KeyedService* SessionSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new sync_sessions::SessionSyncServiceImpl(
      chrome::GetChannel(), std::make_unique<SyncSessionsClientImpl>(profile));
}
