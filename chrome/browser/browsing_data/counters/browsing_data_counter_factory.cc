// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/browsing_data_counter_factory.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"
#include "chrome/browser/browsing_data/counters/cache_counter.h"
#include "chrome/browser/browsing_data/counters/downloads_counter.h"
#include "chrome/browser/browsing_data/counters/media_licenses_counter.h"
#include "chrome/browser/browsing_data/counters/signin_data_counter.h"
#include "chrome/browser/browsing_data/counters/site_data_counter.h"
#include "chrome/browser/browsing_data/counters/site_settings_counter.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/history_counter.h"
#include "components/browsing_data/core/counters/passwords_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/password_manager/core/browser/password_store.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/browsing_data/counters/hosted_apps_counter.h"
#endif

#if !defined(OS_ANDROID)
#include "content/public/browser/host_zoom_map.h"
#endif

#if defined(OS_MACOSX)
#include "device/fido/mac/credential_store.h"
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
        base::Bind(&GetUpdatedWebHistoryService, base::Unretained(profile)),
        ProfileSyncServiceFactory::GetForProfile(profile));
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
// However, on Android it does include Media Licenses.
#if defined(OS_ANDROID)
    return MediaLicensesCounter::Create(profile);
#else
    return nullptr;
#endif
  }

  if (pref_name == browsing_data::prefs::kDeletePasswords) {
    std::unique_ptr<::device::fido::PlatformCredentialStore> credential_store =
#if defined(OS_MACOSX)
        std::make_unique<::device::fido::mac::TouchIdCredentialStore>(
            ChromeAuthenticatorRequestDelegate::
                TouchIdAuthenticatorConfigForProfile(profile));
#else
        nullptr;
#endif
    return std::make_unique<browsing_data::SigninDataCounter>(
        PasswordStoreFactory::GetForProfile(profile,
                                            ServiceAccessType::EXPLICIT_ACCESS),
        ProfileSyncServiceFactory::GetForProfile(profile),
        std::move(credential_store));
  }

  if (pref_name == browsing_data::prefs::kDeleteFormData) {
    return std::make_unique<browsing_data::AutofillCounter>(
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS),
        ProfileSyncServiceFactory::GetForProfile(profile));
  }

  if (pref_name == browsing_data::prefs::kDeleteDownloadHistory) {
    return std::make_unique<DownloadsCounter>(profile);
  }

  if (pref_name == browsing_data::prefs::kDeleteMediaLicenses) {
    return MediaLicensesCounter::Create(profile);
  }

  if (pref_name == browsing_data::prefs::kDeleteSiteSettings) {
    return std::make_unique<SiteSettingsCounter>(
        HostContentSettingsMapFactory::GetForProfile(profile),
#if !defined(OS_ANDROID)
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

  return nullptr;
}
