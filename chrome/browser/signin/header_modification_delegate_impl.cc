// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/header_modification_delegate_impl.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/schemeful_site.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "content/public/browser/site_instance.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "components/account_manager_core/pref_names.h"
#endif

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "base/containers/flat_set.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_impl.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "net/base/features.h"
#include "net/device_bound_sessions/registration_fetcher_param.h"
#endif

namespace signin {

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
namespace {
bool IsFirstPartyRequest(ResponseAdapter* response_adapter) {
  const url::Origin* top_frame_origin =
      response_adapter->GetRequestTopFrameOrigin();
  return top_frame_origin &&
         net::SchemefulSite::IsSameSite(
             *top_frame_origin,
             url::Origin::Create(response_adapter->GetUrl()));
}

void ProcessBoundSessionResponseHeaders(
    BoundSessionCookieRefreshService* bound_session_cookie_refresh_service,
    ResponseAdapter* response_adapter) {
  if (!bound_session_cookie_refresh_service) {
    return;
  }

  const net::HttpResponseHeaders* headers = response_adapter->GetHeaders();
  if (!headers) {
    return;
  }

  // Terminate the session if session termination header is set.
  bound_session_cookie_refresh_service->MaybeTerminateSession(
      response_adapter->GetUrl(), headers);

  // If an equivalent standard DBSC session is going to be triggered by the same
  // response, ignore the session registration.
  base::flat_set<GURL> ignored_registration_endpoints;
  if (base::FeatureList::IsEnabled(net::features::kDeviceBoundSessions)) {
    std::vector<net::device_bound_sessions::RegistrationFetcherParam>
        standard_registrations =
            net::device_bound_sessions::RegistrationFetcherParam::CreateIfValid(
                response_adapter->GetUrl(), headers);
    for (const auto& standard_registration : standard_registrations) {
      ignored_registration_endpoints.insert(
          standard_registration.registration_endpoint());
    }
  }

  for (auto& param : BoundSessionRegistrationFetcherParam::CreateFromHeaders(
           response_adapter->GetUrl(), headers)) {
    if (ignored_registration_endpoints.contains(
            param.registration_endpoint())) {
      continue;
    }
    bound_session_cookie_refresh_service->CreateRegistrationRequest(
        std::move(param));
  }
}

}  // namespace
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

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
  if (profile_->IsOffTheRecord()) {
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    if (!BoundSessionCookieRefreshServiceFactory::GetForProfile(profile_)) {
      return false;
    }
#else
    return false;
#endif
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (ShouldIgnoreGuestWebViewRequest(contents)) {
    return false;
  }
#endif

  return true;
}

void HeaderModificationDelegateImpl::ProcessRequest(
    ChromeRequestAdapter* request_adapter,
    const GURL& redirect_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (profile_->IsOffTheRecord()) {
    // We expect seeing traffic from OTR profiles only if
    // `BoundSessionCookieRefreshService` exists.
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    CHECK(BoundSessionCookieRefreshServiceFactory::GetForProfile(profile_));
    return;
#else
    NOTREACHED();
#endif
  }

  const PrefService* prefs = profile_->GetPrefs();
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  bool is_secondary_account_addition_allowed = true;
  if (!prefs->GetBoolean(
          ::account_manager::prefs::kSecondaryGoogleAccountSigninAllowed)) {
    is_secondary_account_addition_allowed = false;
  }
#endif

  ConsentLevel consent_level = ConsentLevel::kSignin;
#if !BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    consent_level = ConsentLevel::kSync;
  }
#endif

  IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(consent_level);
  signin::Tribool is_child_account =
      // Defaults to kUnknown if the account is not found.
      identity_manager->FindExtendedAccountInfo(account).is_child_account;

  int incognito_mode_availability =
      prefs->GetInteger(policy::policy_prefs::kIncognitoModeAvailability);
#if BUILDFLAG(IS_ANDROID)
  incognito_mode_availability =
      incognito_enabled_
          ? incognito_mode_availability
          : static_cast<int>(policy::IncognitoModeAvailability::kDisabled);
#endif

  FixAccountConsistencyRequestHeader(
      request_adapter, redirect_url, profile_->IsOffTheRecord(),
      incognito_mode_availability,
      AccountConsistencyModeManager::GetMethodForProfile(profile_),
      account.gaia, is_child_account,
#if BUILDFLAG(IS_CHROMEOS)
      is_secondary_account_addition_allowed,
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      // This usage of `IsSyncFeatureEnabled()` needs to be kept until the Sync
      // users are migrated to kSignin state. It tells the Gaia server whether
      // the sync feature is enabled, which in particular triggers a
      // confirmation web page on signout.
      sync_service && sync_service->IsSyncFeatureEnabled(),
      prefs->GetString(prefs::kGoogleServicesSigninScopedDeviceId),
#endif
      cookie_settings_.get());
}

void HeaderModificationDelegateImpl::ProcessResponse(
    ResponseAdapter* response_adapter,
    const GURL& redirect_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (gaia::HasGaiaSchemeHostPort(response_adapter->GetUrl()) &&
      IsFirstPartyRequest(response_adapter)) {
    ProcessBoundSessionResponseHeaders(
        BoundSessionCookieRefreshServiceFactory::GetForProfile(profile_),
        response_adapter);
  }
#endif

  if (profile_->IsOffTheRecord()) {
    // We expect seeing traffic from OTR profiles only if
    // `BoundSessionCookieRefreshService` exists.
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    CHECK(BoundSessionCookieRefreshServiceFactory::GetForProfile(profile_));
    return;
#else
    NOTREACHED();
#endif
  }

  ProcessAccountConsistencyResponseHeaders(response_adapter, redirect_url,
                                           profile_->IsOffTheRecord());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// static
bool HeaderModificationDelegateImpl::ShouldIgnoreGuestWebViewRequest(
    content::WebContents* contents) {
  if (!contents) {
    return true;
  }

  if (extensions::WebViewRendererState::GetInstance()->IsGuest(
          contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID())) {
    CHECK(contents->GetSiteInstance()->IsGuest());
    return true;
  }
  return false;
}
#endif

}  // namespace signin
