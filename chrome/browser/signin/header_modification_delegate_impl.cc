// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/header_modification_delegate_impl.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "extensions/browser/extension_navigation_ui_data.h"

namespace signin {

HeaderModificationDelegateImpl::HeaderModificationDelegateImpl(Profile* profile)
    : profile_(profile),
      cookie_settings_(CookieSettingsFactory::GetForProfile(profile_)) {}

HeaderModificationDelegateImpl::~HeaderModificationDelegateImpl() = default;

bool HeaderModificationDelegateImpl::ShouldInterceptNavigation(
    content::NavigationUIData* navigation_ui_data) {
  if (profile_->IsOffTheRecord())
    return false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Note: InlineLoginUI uses an isolated request context and thus should
  // bypass the account consistency flow. See http://crbug.com/428396
  ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<ChromeNavigationUIData*>(navigation_ui_data);
  if (chrome_navigation_ui_data) {
    extensions::ExtensionNavigationUIData* extension_navigation_ui_data =
        chrome_navigation_ui_data->GetExtensionNavigationUIData();
    if (extension_navigation_ui_data &&
        extension_navigation_ui_data->is_web_view()) {
      return false;
    }
  }
#endif

  return true;
}

void HeaderModificationDelegateImpl::ProcessRequest(
    ChromeRequestAdapter* request_adapter,
    const GURL& redirect_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const PrefService* prefs = profile_->GetPrefs();
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
#endif
  FixAccountConsistencyRequestHeader(
      request_adapter, redirect_url, profile_->IsOffTheRecord(),
      prefs->GetInteger(prefs::kIncognitoModeAvailability),
      AccountConsistencyModeManager::GetMethodForProfile(profile_),
      IdentityManagerFactory::GetForProfile(profile_)
          ->GetPrimaryAccountInfo()
          .gaia,
#if defined(OS_CHROMEOS)
      prefs->GetBoolean(prefs::kAccountConsistencyMirrorRequired),
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      sync_service && sync_service->IsSyncFeatureEnabled(),
      prefs->GetString(prefs::kGoogleServicesSigninScopedDeviceId),
#endif
      cookie_settings_.get());
}

void HeaderModificationDelegateImpl::ProcessResponse(
    ResponseAdapter* response_adapter,
    const GURL& redirect_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ProcessAccountConsistencyResponseHeaders(response_adapter, redirect_url,
                                           profile_->IsOffTheRecord());
}

}  // namespace signin
