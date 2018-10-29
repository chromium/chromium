// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/force_signin_verifier.h"
#include "chrome/browser/signin/local_auth.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/net/delay_network_call.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_window.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/user_manager.h"
#endif

namespace {
SigninClient::SignoutDecision IsSignoutAllowed(
    Profile* profile,
    const signin_metrics::ProfileSignout signout_source_metric) {
  // TODO(msarda): This logic should be reworked to only prohibit user-
  // initiated sign-out. For now signin_util::IsUserSignoutAllowedForProfile()
  // prohibits ALL sign-outs with the exception of ACCOUNT_REMOVED_FROM_DEVICE
  // because this preserves the original behavior. A follow-up CL will make the
  // slightly riskier change described above.
  if (signin_util::IsUserSignoutAllowedForProfile(profile))
    return SigninClient::SignoutDecision::ALLOW_SIGNOUT;

  switch (signout_source_metric) {
    case signin_metrics::ProfileSignout::ACCOUNT_REMOVED_FROM_DEVICE:
      return SigninClient::SignoutDecision::ALLOW_SIGNOUT;

    case signin_metrics::ProfileSignout::ABORT_SIGNIN:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      // Allowed, because data has not been synced yet.
      return SigninClient::SignoutDecision::ALLOW_SIGNOUT;
#else
      // ABORT_SIGNIN is only used on Dice platforms.
      NOTREACHED();
      return SigninClient::SignoutDecision::DISALLOW_SIGNOUT;
#endif

    case signin_metrics::ProfileSignout::SIGNOUT_PREF_CHANGED:
    case signin_metrics::ProfileSignout::GOOGLE_SERVICE_NAME_PATTERN_CHANGED:
    case signin_metrics::ProfileSignout::SIGNIN_PREF_CHANGED_DURING_SIGNIN:
    case signin_metrics::ProfileSignout::USER_CLICKED_SIGNOUT_SETTINGS:
    case signin_metrics::ProfileSignout::SERVER_FORCED_DISABLE:
    case signin_metrics::ProfileSignout::TRANSFER_CREDENTIALS:
    case signin_metrics::ProfileSignout::
        AUTHENTICATION_FAILED_WITH_FORCE_SIGNIN:
    case signin_metrics::ProfileSignout::USER_TUNED_OFF_SYNC_FROM_DICE_UI:
      return SigninClient::SignoutDecision::DISALLOW_SIGNOUT;

    case signin_metrics::ProfileSignout::NUM_PROFILE_SIGNOUT_METRICS:
      NOTREACHED();
      return SigninClient::SignoutDecision::DISALLOW_SIGNOUT;
  }
}
}  // namespace

ChromeSigninClient::ChromeSigninClient(
    Profile* profile,
    SigninErrorController* signin_error_controller)
    : OAuth2TokenService::Consumer("chrome_signin_client"),
      profile_(profile),
      signin_error_controller_(signin_error_controller),
      weak_ptr_factory_(this) {
  signin_error_controller_->AddObserver(this);
#if !defined(OS_CHROMEOS)
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
#endif
}

ChromeSigninClient::~ChromeSigninClient() {
  signin_error_controller_->RemoveObserver(this);
#if !defined(OS_CHROMEOS)
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
#endif
}

void ChromeSigninClient::DoFinalInit() {
  MaybeFetchSigninTokenHandle();
  VerifySyncToken();
}

// static
bool ChromeSigninClient::ProfileAllowsSigninCookies(Profile* profile) {
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(profile).get();
  return signin::SettingsAllowSigninCookies(cookie_settings);
}

PrefService* ChromeSigninClient::GetPrefs() { return profile_->GetPrefs(); }

void ChromeSigninClient::OnSignedOut() {
  ProfileAttributesEntry* entry;
  bool has_entry = g_browser_process->profile_manager()->
      GetProfileAttributesStorage().
      GetProfileAttributesWithPath(profile_->GetPath(), &entry);

  // If sign out occurs because Sync setup was in progress and the Profile got
  // deleted, then the profile's no longer in the ProfileAttributesStorage.
  if (!has_entry)
    return;

  entry->SetLocalAuthCredentials(std::string());
  entry->SetAuthInfo(std::string(), base::string16());
  entry->SetIsSigninRequired(false);
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeSigninClient::GetURLLoaderFactory() {
  if (url_loader_factory_for_testing_)
    return url_loader_factory_for_testing_;

  return content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetURLLoaderFactoryForBrowserProcess();
}

network::mojom::CookieManager* ChromeSigninClient::GetCookieManager() {
  return content::BrowserContext::GetDefaultStoragePartition(profile_)
      ->GetCookieManagerForBrowserProcess();
}

std::string ChromeSigninClient::GetProductVersion() {
  return chrome::GetVersionString();
}

bool ChromeSigninClient::IsFirstRun() const {
#if defined(OS_ANDROID)
  return false;
#else
  return first_run::IsChromeFirstRun();
#endif
}

base::Time ChromeSigninClient::GetInstallDate() {
  return base::Time::FromTimeT(
      g_browser_process->metrics_service()->GetInstallDate());
}

bool ChromeSigninClient::AreSigninCookiesAllowed() {
  return ProfileAllowsSigninCookies(profile_);
}

void ChromeSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
  HostContentSettingsMapFactory::GetForProfile(profile_)
      ->AddObserver(observer);
}

void ChromeSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
  HostContentSettingsMapFactory::GetForProfile(profile_)
      ->RemoveObserver(observer);
}

void ChromeSigninClient::OnSignedIn(const std::string& account_id,
                                    const std::string& gaia_id,
                                    const std::string& username,
                                    const std::string& password) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesEntry* entry;
  if (profile_manager->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_->GetPath(), &entry)) {
    entry->SetAuthInfo(gaia_id, base::UTF8ToUTF16(username));
    ProfileMetrics::UpdateReportedProfilesStatistics(profile_manager);
  }
}

void ChromeSigninClient::PostSignedIn(const std::string& account_id,
                                      const std::string& username,
                                      const std::string& password) {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // Don't store password hash except when lock is available for the user.
  if (!password.empty() && profiles::IsLockAvailable(profile_))
    LocalAuth::SetLocalAuthCredentials(profile_, password);
#endif
}

void ChromeSigninClient::PreSignOut(
    base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
    signin_metrics::ProfileSignout signout_source_metric) {
  DCHECK(on_signout_decision_reached);
  DCHECK(!on_signout_decision_reached_) << "SignOut already in-progress!";
  on_signout_decision_reached_ = std::move(on_signout_decision_reached);

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

  // These sign out won't remove the policy cache, keep the window opened.
  bool keep_window_opened =
      signout_source_metric ==
          signin_metrics::GOOGLE_SERVICE_NAME_PATTERN_CHANGED ||
      signout_source_metric == signin_metrics::SERVER_FORCED_DISABLE ||
      signout_source_metric == signin_metrics::SIGNOUT_PREF_CHANGED;
  if (signin_util::IsForceSigninEnabled() && !profile_->IsSystemProfile() &&
      !profile_->IsGuestSession() && !profile_->IsSupervised() &&
      !keep_window_opened) {
    if (signout_source_metric ==
        signin_metrics::SIGNIN_PREF_CHANGED_DURING_SIGNIN) {
      // SIGNIN_PREF_CHANGED_DURING_SIGNIN will be triggered when SigninManager
      // is initialized before window opening, there is no need to close window.
      // Call OnCloseBrowsersSuccess to continue sign out and show UserManager
      // afterwards.
      should_display_user_manager_ = false;  // Don't show UserManager twice.
      OnCloseBrowsersSuccess(signout_source_metric, profile_->GetPath());
    } else {
      BrowserList::CloseAllBrowsersWithProfile(
          profile_,
          base::Bind(&ChromeSigninClient::OnCloseBrowsersSuccess,
                     base::Unretained(this), signout_source_metric),
          base::Bind(&ChromeSigninClient::OnCloseBrowsersAborted,
                     base::Unretained(this)),
          signout_source_metric == signin_metrics::ABORT_SIGNIN ||
              signout_source_metric ==
                  signin_metrics::AUTHENTICATION_FAILED_WITH_FORCE_SIGNIN ||
              signout_source_metric == signin_metrics::TRANSFER_CREDENTIALS);
    }
  } else {
#else
  {
#endif
    std::move(on_signout_decision_reached_)
        .Run(IsSignoutAllowed(profile_, signout_source_metric));
  }
}

void ChromeSigninClient::OnErrorChanged() {
  // Some tests don't have a ProfileManager.
  if (g_browser_process->profile_manager() == nullptr)
    return;

  ProfileAttributesEntry* entry;

  if (!g_browser_process->profile_manager()->GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_->GetPath(), &entry)) {
    return;
  }

  entry->SetIsAuthError(signin_error_controller_->HasError());
}

void ChromeSigninClient::OnGetTokenInfoResponse(
    std::unique_ptr<base::DictionaryValue> token_info) {
  if (!token_info->HasKey("error")) {
    std::string handle;
    if (token_info->GetString("token_handle", &handle)) {
      ProfileAttributesEntry* entry = nullptr;
      bool has_entry = g_browser_process->profile_manager()->
          GetProfileAttributesStorage().
          GetProfileAttributesWithPath(profile_->GetPath(), &entry);
      DCHECK(has_entry);
      entry->SetPasswordChangeDetectionToken(handle);
    }
  }
  oauth_request_.reset();
}

void ChromeSigninClient::OnOAuthError() {
  // Ignore the failure.  It's not essential and we'll try again next time.
    oauth_request_.reset();
}

void ChromeSigninClient::OnNetworkError(int response_code) {
  // Ignore the failure.  It's not essential and we'll try again next time.
    oauth_request_.reset();
}

void ChromeSigninClient::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  // Exchange the access token for a handle that can be used for later
  // verification that the token is still valid (i.e. the password has not
  // been changed).
    if (!oauth_client_) {
      oauth_client_.reset(new gaia::GaiaOAuthClient(GetURLLoaderFactory()));
    }
    oauth_client_->GetTokenInfo(token_response.access_token, 3 /* retries */,
                                this);
}

void ChromeSigninClient::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  // Ignore the failure.  It's not essential and we'll try again next time.
  oauth_request_.reset();
}

#if !defined(OS_CHROMEOS)
void ChromeSigninClient::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type == network::mojom::ConnectionType::CONNECTION_NONE)
    return;

  for (const base::Closure& callback : delayed_callbacks_)
    callback.Run();

  delayed_callbacks_.clear();
}
#endif

void ChromeSigninClient::DelayNetworkCall(const base::Closure& callback) {
#if defined(OS_CHROMEOS)
  chromeos::DelayNetworkCall(
      base::TimeDelta::FromMilliseconds(chromeos::kDefaultNetworkRetryDelayMS),
      callback);
  return;
#else
  // Don't bother if we don't have any kind of network connection.
  network::mojom::ConnectionType type;
  bool sync = content::GetNetworkConnectionTracker()->GetConnectionType(
      &type, base::BindOnce(&ChromeSigninClient::OnConnectionChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  if (!sync || type == network::mojom::ConnectionType::CONNECTION_NONE) {
    // Connection type cannot be retrieved synchronously so delay the callback.
    delayed_callbacks_.push_back(callback);
  } else {
    callback.Run();
  }
#endif
}

std::unique_ptr<GaiaAuthFetcher> ChromeSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    const std::string& source,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<GaiaAuthFetcher>(consumer, source,
                                           url_loader_factory);
}

void ChromeSigninClient::VerifySyncToken() {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  if (signin_util::IsForceSigninEnabled())
    force_signin_verifier_ = std::make_unique<ForceSigninVerifier>(profile_);
#endif
}

void ChromeSigninClient::MaybeFetchSigninTokenHandle() {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // We get a "handle" that can be used to reference the signin token on the
  // server.  We fetch this if we don't have one so that later we can check
  // it to know if the signin token to which it is attached has been revoked
  // and thus distinguish between a password mismatch due to the password
  // being changed and the user simply mis-typing it.
  if (profiles::IsLockAvailable(profile_)) {
    ProfileAttributesStorage& storage =
        g_browser_process->profile_manager()->GetProfileAttributesStorage();
    ProfileAttributesEntry* entry;
    // If we don't have a token for detecting a password change, create one.
    if (storage.GetProfileAttributesWithPath(profile_->GetPath(), &entry) &&
        entry->GetPasswordChangeDetectionToken().empty() && !oauth_request_) {
      std::string account_id = SigninManagerFactory::GetForProfile(profile_)
          ->GetAuthenticatedAccountId();
      if (!account_id.empty()) {
        ProfileOAuth2TokenService* token_service =
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
        OAuth2TokenService::ScopeSet scopes;
        scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
        oauth_request_ = token_service->StartRequest(account_id, scopes, this);
      }
    }
  }
#endif
}

void ChromeSigninClient::AfterCredentialsCopied() {
  if (signin_util::IsForceSigninEnabled()) {
    // The signout after credential copy won't open UserManager after all
    // browser window are closed. Because the browser window will be opened for
    // the new profile soon.
    should_display_user_manager_ = false;
  }
}

void ChromeSigninClient::SetReadyForDiceMigration(bool is_ready) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  AccountConsistencyModeManager::GetForProfile(profile_)
      ->SetReadyForDiceMigration(is_ready);
#else
  NOTREACHED();
#endif
}

void ChromeSigninClient::SetURLLoaderFactoryForTest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_for_testing_ = url_loader_factory;
}

void ChromeSigninClient::OnCloseBrowsersSuccess(
    const signin_metrics::ProfileSignout signout_source_metric,
    const base::FilePath& profile_path) {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  if (signin_util::IsForceSigninEnabled() && force_signin_verifier_.get()) {
    force_signin_verifier_->Cancel();
  }
#endif

  std::move(on_signout_decision_reached_)
      .Run(IsSignoutAllowed(profile_, signout_source_metric));

  LockForceSigninProfile(profile_path);
  // After sign out, lock the profile and show UserManager if necessary.
  if (should_display_user_manager_) {
    ShowUserManager(profile_path);
  } else {
    should_display_user_manager_ = true;
  }
}

void ChromeSigninClient::OnCloseBrowsersAborted(
    const base::FilePath& profile_path) {
  should_display_user_manager_ = true;

  // Disallow sign-out (aborted).
  std::move(on_signout_decision_reached_)
      .Run(SignoutDecision::DISALLOW_SIGNOUT);
}

void ChromeSigninClient::LockForceSigninProfile(
    const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry;
  bool has_entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath(), &entry);
  if (!has_entry)
    return;
  entry->LockForceSigninProfile(true);
}

void ChromeSigninClient::ShowUserManager(const base::FilePath& profile_path) {
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  UserManager::Show(profile_path,
                    profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
#endif
}
