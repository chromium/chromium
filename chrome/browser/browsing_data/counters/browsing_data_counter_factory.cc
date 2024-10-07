// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/browsing_data_counter_factory.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"
#include "chrome/browser/browsing_data/counters/cache_counter.h"
#include "chrome/browser/browsing_data/counters/downloads_counter.h"
#include "chrome/browser/browsing_data/counters/signin_data_counter.h"
#include "chrome/browser/browsing_data/counters/site_data_counter.h"
#include "chrome/browser/browsing_data/counters/site_settings_counter.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/sync/service/sync_service.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/browsing_data/counters/hosted_apps_counter.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "content/public/browser/host_zoom_map.h"
#else
#include "chrome/browser/browsing_data/counters/tabs_counter.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "device/fido/cros/credential_store.h"
#endif

namespace {

history::WebHistoryService* GetUpdatedWebHistoryService(Profile* profile) {
  return WebHistoryServiceFactory::GetForProfile(profile);
}

}  // namespace

// static
std::unique_ptr<browsing_data::BrowsingDataCounter>
BrowsingDataCounterFactory::GetForProfileAndPref(Profile* profile,
                                                 const std::string& pref_name) {
  if (pref_name == browsing_data::prefs::kDeleteBrowsingHistory) {
    return std::make_unique<browsing_data::HistoryCounter>(
        HistoryServiceFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        base::BindRepeating(&GetUpdatedWebHistoryService,
                            base::Unretained(profile)),
        SyncServiceFactory::GetForProfile(profile));
  }
  if (pref_name == browsing_data::prefs::kDeleteBrowsingHistoryBasic) {
    // The history option on the basic tab doesn't use a counter.
    return nullptr;
  }

  if (pref_name == browsing_data::prefs::kDeleteCache ||
      pref_name == browsing_data::prefs::kDeleteCacheBasic) {
    return std::make_unique<CacheCounter>(profile);
  }

  if (pref_name == browsing_data::prefs::kDeleteCookies) {
    return std::make_unique<SiteDataCounter>(profile);
  }
  if (pref_name == browsing_data::prefs::kDeleteCookiesBasic) {
    // The cookies option on the basic tab doesn't use a counter.
    return nullptr;
  }

  if (pref_name == browsing_data::prefs::kDeletePasswords) {
    std::unique_ptr<::device::fido::PlatformCredentialStore> credential_store =
#if BUILDFLAG(IS_CHROMEOS_ASH)
        std::make_unique<
            ::device::fido::cros::PlatformAuthenticatorCredentialStore>();
#else
        nullptr;
#endif
    return std::make_unique<browsing_data::SigninDataCounter>(
        ProfilePasswordStoreFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        AccountPasswordStoreFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile),
        std::move(credential_store));
  }

  if (pref_name == browsing_data::prefs::kDeleteFormData) {
    return std::make_unique<browsing_data::AutofillCounter>(
        autofill::PersonalDataManagerFactory::GetForBrowserContext(profile),
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
#if !BUILDFLAG(IS_ANDROID)
        UserAnnotationsServiceFactory::GetForProfile(profile),
#else
        /*user_annotations_service=*/nullptr,
#endif
        SyncServiceFactory::GetForProfile(profile));
  }

  if (pref_name == browsing_data::prefs::kDeleteDownloadHistory) {
    return std::make_unique<DownloadsCounter>(profile);
  }

  if (pref_name == browsing_data::prefs::kDeleteSiteSettings) {
    return std::make_unique<SiteSettingsCounter>(
        HostContentSettingsMapFactory::GetForProfile(profile),
#if !BUILDFLAG(IS_ANDROID)
        content::HostZoomMap::GetDefaultForBrowserContext(profile),
#else
        nullptr,
#endif
        ProtocolHandlerRegistryFactory::GetForBrowserContext(profile),
        profile->GetPrefs());
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (pref_name == browsing_data::prefs::kDeleteHostedAppsData) {
    return std::make_unique<HostedAppsCounter>(profile);
  }
#endif

#if BUILDFLAG(IS_ANDROID)
  if (pref_name == browsing_data::prefs::kCloseTabs) {
    return std::make_unique<TabsCounter>(profile);
  }
#endif

  return nullptr;
}
