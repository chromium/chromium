// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_delegate_factory.h"

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

// Provider for Chrome-specific services and functionality.
class ChromeBackgroundFetchDelegateHelper
    : public BackgroundFetchDelegateImpl::Embedder {
 public:
  explicit ChromeBackgroundFetchDelegateHelper(Profile* profile)
      : profile_(profile) {}
  ChromeBackgroundFetchDelegateHelper(
      const ChromeBackgroundFetchDelegateHelper&) = delete;
  ChromeBackgroundFetchDelegateHelper& operator=(
      const ChromeBackgroundFetchDelegateHelper&) = delete;
  ~ChromeBackgroundFetchDelegateHelper() override = default;

  // BackgroundFetchDelegateImpl::Delegate:
  offline_items_collection::OfflineContentAggregator*
  GetOfflineContentAggregator() override {
    return OfflineContentAggregatorFactory::GetForKey(
        profile_->GetProfileKey());
  }

  download::DownloadService* GetDownloadService() override {
    return DownloadServiceFactory::GetInstance()->GetForKey(
        profile_->GetProfileKey());
  }

  HostContentSettingsMap* GetHostContentSettingsMap() override {
    return HostContentSettingsMapFactory::GetForProfile(profile_);
  }

  void UpdateOfflineItem(
      offline_items_collection::OfflineItem* offline_item) override {
#if defined(OS_ANDROID)
    if (profile_->IsOffTheRecord())
      offline_item->otr_profile_id = profile_->GetOTRProfileID().Serialize();
#endif
  }

  void OnJobCompleted(const url::Origin& origin,
                      bool user_initiated_abort) override {
    auto* ukm_background_service =
        ukm::UkmBackgroundRecorderFactory::GetForProfile(profile_);
    ukm_background_service->GetBackgroundSourceIdIfAllowed(
        origin,
        base::BindOnce(&DidGetBackgroundSourceId, user_initiated_abort));
  }

 private:
  static void DidGetBackgroundSourceId(
      bool user_initiated_abort,
      base::Optional<ukm::SourceId> source_id) {
    // This background event did not meet the requirements for the UKM service.
    if (!source_id)
      return;

    ukm::builders::BackgroundFetchDeletingRegistration(*source_id)
        .SetUserInitiatedAbort(user_initiated_abort)
        .Record(ukm::UkmRecorder::Get());
  }

  Profile* profile_;
};

}  // namespace

// static
BackgroundFetchDelegateImpl* BackgroundFetchDelegateFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BackgroundFetchDelegateImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BackgroundFetchDelegateFactory* BackgroundFetchDelegateFactory::GetInstance() {
  return base::Singleton<BackgroundFetchDelegateFactory>::get();
}

BackgroundFetchDelegateFactory::BackgroundFetchDelegateFactory()
    : BrowserContextKeyedServiceFactory(
          "BackgroundFetchService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(DownloadServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(OfflineContentAggregatorFactory::GetInstance());
  DependsOn(ukm::UkmBackgroundRecorderFactory::GetInstance());
}

BackgroundFetchDelegateFactory::~BackgroundFetchDelegateFactory() {}

KeyedService* BackgroundFetchDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BackgroundFetchDelegateImpl(
      context, std::make_unique<ChromeBackgroundFetchDelegateHelper>(
                   Profile::FromBrowserContext(context)));
}

content::BrowserContext* BackgroundFetchDelegateFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
