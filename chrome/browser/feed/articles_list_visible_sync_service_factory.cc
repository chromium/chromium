// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/articles_list_visible_sync_service_factory.h"

#include <memory>
#include <optional>

#include "base/android/device_info.h"
#include "base/feature_list.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feed/core/v2/public/articles_list_visible_sync_service.h"
#include "components/feed/feed_feature_list.h"
#include "ui/base/device_form_factor.h"

namespace feed {

// static
ArticlesListVisibleSyncServiceFactory*
ArticlesListVisibleSyncServiceFactory::GetInstance() {
  static base::NoDestructor<ArticlesListVisibleSyncServiceFactory> instance;
  return instance.get();
}

ArticlesListVisibleSyncServiceFactory::ArticlesListVisibleSyncServiceFactory()
    : ProfileKeyedServiceFactory("ArticlesListVisibleSyncService",
                                 ProfileSelections::BuildForRegularProfile()) {}

ArticlesListVisibleSyncServiceFactory::
    ~ArticlesListVisibleSyncServiceFactory() = default;

void ArticlesListVisibleSyncServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Registered even if the feature is disabled, so that synced values are kept
  // when the feature is toggled.
  ArticlesListVisibleSyncService::RegisterProfilePrefs(registry);
}

std::unique_ptr<KeyedService>
ArticlesListVisibleSyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kAndroidFeedsVisibilitySyncByFormFactor)) {
    return nullptr;
  }

  std::optional<ArticlesListVisibleSyncService::FormFactor> form_factor =
      ArticlesListVisibleSyncService::GetFormFactor(
          ui::GetDeviceFormFactor(), base::android::device_info::is_foldable());
  if (!form_factor) {
    return nullptr;
  }

  return std::make_unique<ArticlesListVisibleSyncService>(
      *PrefServiceSyncableFromProfile(Profile::FromBrowserContext(context)),
      *form_factor);
}

bool ArticlesListVisibleSyncServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ArticlesListVisibleSyncServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace feed
