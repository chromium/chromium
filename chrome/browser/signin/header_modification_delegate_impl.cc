// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/header_modification_delegate_impl.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/account_manager_core/pref_names.h"
#endif

namespace signin {

#if BUILDFLAG(IS_ANDROID)
HeaderModificationDelegateImpl::HeaderModificationDelegateImpl(
    Profile* profile,
    bool incognito_enabled)
    : profile_(profile),
      cookie_settings_(CookieSettingsFactory::GetForProfile(profile_)),
      incognito_enabled_(incognito_enabled) {}
#else
HeaderModificationDelegateImpl::HeaderModificationDelegateImpl(Profile* profile)
    : profile_(profile),
      cookie_settings_(CookieSettingsFactory::GetForProfile(profile_)) {}
#endif

HeaderModificationDelegateImpl::~HeaderModificationDelegateImpl() = default;

bool HeaderModificationDelegateImpl::ShouldInterceptNavigation(
    content::WebContents* contents) {
  if (profile_->IsOffTheRecord())
    return false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (ShouldIgnoreGuestWebViewRequest(contents))
    return false;
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
      SyncServiceFactory::GetForProfile(profile_);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_secondary_account_addition_allowed = true;
  if (!prefs->GetBoolean(
          ::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed)) {
    is_secondary_account_addition_allowed = false;
  }
#endif

  ConsentLevel consent_level = ConsentLevel::kSync;
#if BUILDFLAG(IS_ANDROID)
  consent_level = ConsentLevel::kSignin;
#endif

  IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(consent_level);
  signin::Tribool is_child_account =
      // Defaults to kUnknown if the account is not found.
      identity_manager->FindExtendedAccountInfo(account).is_child_account;

  int incognito_mode_availability =
      prefs->GetInteger(prefs::kIncognitoModeAvailability);
#if BUILDFLAG(IS_ANDROID)
  incognito_mode_availability =
      incognito_enabled_
          ? incognito_mode_availability
          : static_cast<int>(IncognitoModePrefs::Availability::kDisabled);
#endif

  FixAccountConsistencyRequestHeader(
      request_adapter, redirect_url, profile_->IsOffTheRecord(),
      incognito_mode_availability,
      AccountConsistencyModeManager::GetMethodForProfile(profile_),
      account.gaia, is_child_account,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      is_secondary_account_addition_allowed,
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
// static
bool HeaderModificationDelegateImpl::ShouldIgnoreGuestWebViewRequest(
    content::WebContents* contents) {
  if (!contents)
    return true;

  if (extensions::WebViewRendererState::GetInstance()->IsGuest(
          contents->GetPrimaryMainFrame()->GetProcess()->GetID())) {
    auto identity_api_config =
        extensions::WebAuthFlow::GetWebViewPartitionConfig(
            extensions::WebAuthFlow::GET_AUTH_TOKEN,
            contents->GetBrowserContext());
    if (contents->GetSiteInstance()->GetStoragePartitionConfig() !=
        identity_api_config)
      return true;

    // If the StoragePartitionConfig matches, but |contents| is not using a
    // guest SiteInstance, then there is likely a serious bug.
    CHECK(contents->GetSiteInstance()->IsGuest());
  }
  return false;
}
#endif

}  // namespace signin
