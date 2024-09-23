// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/session_sync_service_factory.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegates_getter.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/session_sync_service_impl.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "content/public/common/url_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sync/glue/synced_window_delegates_getter_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

bool ShouldSyncURLImpl(const GURL& url) {
  return url.is_valid() && !content::HasWebUIScheme(url) &&
         !url.SchemeIs(chrome::kChromeNativeScheme) && !url.SchemeIsFile() &&
         !url.SchemeIs(dom_distiller::kDomDistillerScheme);
}

// Chrome implementation of SyncSessionsClient.
class SyncSessionsClientImpl final : public sync_sessions::SyncSessionsClient {
 public:
  explicit SyncSessionsClientImpl(Profile* profile)
      : profile_(profile), session_sync_prefs_(profile->GetPrefs()) {
    window_delegates_getter_ =
#if BUILDFLAG(IS_ANDROID)
        // Android doesn't have multi-profile support, so no need to pass the
        // profile in.
        std::make_unique<browser_sync::SyncedWindowDelegatesGetterAndroid>();
#else   // BUILDFLAG(IS_ANDROID)
        std::make_unique<browser_sync::BrowserSyncedWindowDelegatesGetter>(
            profile);
#endif  // BUILDFLAG(IS_ANDROID)
  }

  SyncSessionsClientImpl(const SyncSessionsClientImpl&) = delete;
  SyncSessionsClientImpl& operator=(const SyncSessionsClientImpl&) = delete;

  ~SyncSessionsClientImpl() override = default;

  // SyncSessionsClient implementation.
  sync_sessions::SessionSyncPrefs* GetSessionSyncPrefs() override {
    return &session_sync_prefs_;
  }

  syncer::RepeatingDataTypeStoreFactory GetStoreFactory() override {
    return DataTypeStoreServiceFactory::GetForProfile(profile_)
        ->GetStoreFactory();
  }

  void ClearAllOnDemandFavicons() override {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (!history_service) {
      return;
    }
    history_service->ClearAllOnDemandFavicons();
  }

  bool ShouldSyncURL(const GURL& url) const override {
    return ShouldSyncURLImpl(url);
  }

  bool IsRecentLocalCacheGuid(const std::string& cache_guid) const override {
    return DeviceInfoSyncServiceFactory::GetForProfile(profile_)
        ->GetDeviceInfoTracker()
        ->IsRecentLocalCacheGuid(cache_guid);
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

  base::WeakPtr<SyncSessionsClient> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const raw_ptr<Profile> profile_;
  std::unique_ptr<sync_sessions::SyncedWindowDelegatesGetter>
      window_delegates_getter_;
  sync_sessions::SessionSyncPrefs session_sync_prefs_;
  base::WeakPtrFactory<SyncSessionsClientImpl> weak_ptr_factory_{this};
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
  static base::NoDestructor<SessionSyncServiceFactory> instance;
  return instance.get();
}

// static - exposed for testing and metrics.
bool SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
    const GURL& url) {
  return ShouldSyncURLImpl(url);
}

SessionSyncServiceFactory::SessionSyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "SessionSyncService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(FaviconServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(sync_sessions::SyncSessionsWebContentsRouterFactory::GetInstance());
}

SessionSyncServiceFactory::~SessionSyncServiceFactory() = default;

KeyedService* SessionSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new sync_sessions::SessionSyncServiceImpl(
      chrome::GetChannel(), std::make_unique<SyncSessionsClientImpl>(profile));
}
