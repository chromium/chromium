// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_permissions_util.h"

#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"

namespace {

bool IsUserDataSaverEnabledAndAllowedToFetchHints(Profile* profile) {
  // Check if they are a data saver user.
  if (!data_reduction_proxy::DataReductionProxySettings::
          IsDataSaverEnabledByUser(profile->IsOffTheRecord(),
                                   profile->GetPrefs())) {
    return false;
  }

  // Now ensure that they have seen the HTTPS infobar notification.
  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(profile);
  if (!previews_service)
    return false;

  PreviewsHTTPSNotificationInfoBarDecider* info_bar_decider =
      previews_service->previews_https_notification_infobar_decider();
  return !info_bar_decider->NeedsToNotifyUser();
}

bool IsUserConsentedToAnonymousDataCollectionAndAllowedToFetchHints(
    Profile* profile) {
  if (!optimization_guide::features::
          IsHintsFetchingForAnonymousDataConsentEnabled()) {
    return false;
  }

  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (!sync_service)
    return false;

  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(profile->GetPrefs(),
                                                   sync_service);
  return helper->IsEnabled();
}

}  // namespace

bool IsUserPermittedToFetchHints(Profile* profile) {
  if (profile->IsIncognitoProfile())
    return false;

  if (!optimization_guide::features::IsHintsFetchingEnabled())
    return false;

  if (IsUserDataSaverEnabledAndAllowedToFetchHints(profile))
    return true;

  return IsUserConsentedToAnonymousDataCollectionAndAllowedToFetchHints(
      profile);
}
