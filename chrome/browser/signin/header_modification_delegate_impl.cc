// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#endif

namespace signin {

HeaderModificationDelegateImpl::HeaderModificationDelegateImpl(Profile* profile)
    : profile_(profile),
      cookie_settings_(CookieSettingsFactory::GetForProfile(profile_)) {}

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
      ProfileSyncServiceFactory::GetForProfile(profile_);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool is_secondary_account_addition_allowed = true;
  if (!prefs->GetBoolean(
          chromeos::prefs::kSecondaryGoogleAccountSigninAllowed)) {
    is_secondary_account_addition_allowed = false;
  }
#endif

  ConsentLevel consent_level = ConsentLevel::kSync;
#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(kMobileIdentityConsistency))
    consent_level = ConsentLevel::kSignin;
#endif

  IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(consent_level);
  base::Optional<bool> is_child_account = base::nullopt;
  if (!account.IsEmpty()) {
    base::Optional<AccountInfo> extended_account_info =
        identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
            account);
    if (extended_account_info.has_value()) {
      is_child_account = base::make_optional<bool>(
          extended_account_info.value().is_child_account);
    }
  }

  FixAccountConsistencyRequestHeader(
      request_adapter, redirect_url, profile_->IsOffTheRecord(),
      prefs->GetInteger(prefs::kIncognitoModeAvailability),
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
          contents->GetMainFrame()->GetProcess()->GetID())) {
    GURL identity_api_site =
        extensions::WebViewGuest::GetSiteForGuestPartitionConfig(
            extensions::WebAuthFlow::GetWebViewPartitionConfig(
                extensions::WebAuthFlow::GET_AUTH_TOKEN,
                contents->GetBrowserContext()));
    if (contents->GetSiteInstance()->GetSiteURL() != identity_api_site)
      return true;

    // If the site URL matches, but |contents| is not using a guest
    // SiteInstance, then there is likely a serious bug.
    CHECK(contents->GetSiteInstance()->IsGuest());
  }
  return false;
}
#endif

}  // namespace signin
