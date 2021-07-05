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
#include "build/chromeos_buildflags.h"
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
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/metrics/metrics_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/chromeos/net/delay_network_call.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager_util.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/profiles/profile_window.h"
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profile_picker.h"
#endif

namespace {

// List of sources for which sign out is always allowed.
signin_metrics::ProfileSignout kAlwaysAllowedSignoutSources[] = {
    // Allowed, because data has not been synced yet.
    signin_metrics::ProfileSignout::ABORT_SIGNIN,
    // Allowed, because only used on Android and the primary account must be
    // cleared when the account is removed from device
    signin_metrics::ProfileSignout::ACCOUNT_REMOVED_FROM_DEVICE,
    // Allowed to force finish the account id migration.
    signin_metrics::ACCOUNT_ID_MIGRATION,
    // Allowed, for tests.
    signin_metrics::ProfileSignout::FORCE_SIGNOUT_ALWAYS_ALLOWED_FOR_TEST};

SigninClient::SignoutDecision IsSignoutAllowed(
    Profile* profile,
    const signin_metrics::ProfileSignout signout_source) {
  if (signin_util::IsUserSignoutAllowedForProfile(profile))
    return SigninClient::SignoutDecision::ALLOW_SIGNOUT;

  for (const auto& always_allowed_source : kAlwaysAllowedSignoutSources) {
    if (signout_source == always_allowed_source)
      return SigninClient::SignoutDecision::ALLOW_SIGNOUT;
  }

  return SigninClient::SignoutDecision::DISALLOW_SIGNOUT;
}

}  // namespace

ChromeSigninClient::ChromeSigninClient(Profile* profile) : profile_(profile) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
#endif
}

ChromeSigninClient::~ChromeSigninClient() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
#endif
}

void ChromeSigninClient::DoFinalInit() {
  VerifySyncToken();
}

// static
bool ChromeSigninClient::ProfileAllowsSigninCookies(Profile* profile) {
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(profile).get();
  return signin::SettingsAllowSigninCookies(cookie_settings);
}

PrefService* ChromeSigninClient::GetPrefs() { return profile_->GetPrefs(); }

scoped_refptr<network::SharedURLLoaderFactory>
ChromeSigninClient::GetURLLoaderFactory() {
  if (url_loader_factory_for_testing_)
    return url_loader_factory_for_testing_;

  return profile_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

network::mojom::CookieManager* ChromeSigninClient::GetCookieManager() {
  return profile_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

bool ChromeSigninClient::AreSigninCookiesAllowed() {
  return ProfileAllowsSigninCookies(profile_);
}

bool ChromeSigninClient::AreSigninCookiesDeletedOnExit() {
  content_settings::CookieSettings* cookie_settings =
      CookieSettingsFactory::GetForProfile(profile_).get();
  return signin::SettingsDeleteSigninCookiesOnExit(cookie_settings);
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

void ChromeSigninClient::PreSignOut(
    base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
    signin_metrics::ProfileSignout signout_source_metric) {
  DCHECK(on_signout_decision_reached);
  DCHECK(!on_signout_decision_reached_) << "SignOut already in-progress!";
  on_signout_decision_reached_ = std::move(on_signout_decision_reached);

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

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
      // SIGNIN_PREF_CHANGED_DURING_SIGNIN will be triggered when
      // IdentityManager is initialized before window opening, there is no need
      // to close window. Call OnCloseBrowsersSuccess to continue sign out and
      // show UserManager afterwards.
      should_display_user_manager_ = false;  // Don't show UserManager twice.
      OnCloseBrowsersSuccess(signout_source_metric, profile_->GetPath());
    } else {
      BrowserList::CloseAllBrowsersWithProfile(
          profile_,
          base::BindRepeating(&ChromeSigninClient::OnCloseBrowsersSuccess,
                              base::Unretained(this), signout_source_metric),
          base::BindRepeating(&ChromeSigninClient::OnCloseBrowsersAborted,
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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeSigninClient::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type == network::mojom::ConnectionType::CONNECTION_NONE)
    return;

  for (base::OnceClosure& callback : delayed_callbacks_)
    std::move(callback).Run();

  delayed_callbacks_.clear();
}
#endif

void ChromeSigninClient::DelayNetworkCall(base::OnceClosure callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::DelayNetworkCall(
      base::TimeDelta::FromMilliseconds(chromeos::kDefaultNetworkRetryDelayMS),
      std::move(callback));
  return;
#else
  // Don't bother if we don't have any kind of network connection.
  network::mojom::ConnectionType type;
  bool sync = content::GetNetworkConnectionTracker()->GetConnectionType(
      &type, base::BindOnce(&ChromeSigninClient::OnConnectionChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  if (!sync || type == network::mojom::ConnectionType::CONNECTION_NONE) {
    // Connection type cannot be retrieved synchronously so delay the callback.
    delayed_callbacks_.push_back(std::move(callback));
  } else {
    std::move(callback).Run();
  }
#endif
}

std::unique_ptr<GaiaAuthFetcher> ChromeSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source) {
  return std::make_unique<GaiaAuthFetcher>(consumer, source,
                                           GetURLLoaderFactory());
}

void ChromeSigninClient::VerifySyncToken() {
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // We only verifiy the token once when Profile is just created.
  if (signin_util::IsForceSigninEnabled() && !force_signin_verifier_)
    force_signin_verifier_ = std::make_unique<ForceSigninVerifier>(
        profile_, IdentityManagerFactory::GetForProfile(profile_));
#endif
}

void ChromeSigninClient::SetDiceMigrationCompleted() {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  AccountConsistencyModeManager::GetForProfile(profile_)
      ->SetDiceMigrationCompleted();
#else
  NOTREACHED();
#endif
}

bool ChromeSigninClient::IsNonEnterpriseUser(const std::string& username) {
  return policy::BrowserPolicyConnector::IsNonEnterpriseUser(username);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Returns the account that must be auto-signed-in to the Main Profile in
// Lacros.
// This is, when available, the account used to sign into the Chrome OS
// session. This may be a Gaia account or a Microsoft Active Directory
// account. This field will be null for Guest sessions, Managed Guest
// sessions, Demo mode, and Kiosks. Note that this is different from the
// concept of a Primary Account in the browser. A user may not be signed into
// a Lacros browser Profile, or may be signed into a browser Profile with an
// account which is different from the account which they used to sign into
// the device - aka Device Account.
// Also note that this will be null for Secondary / non-Main Profiles in
// Lacros, because they do not start with the Chrome OS Device Account
// signed-in by default.
absl::optional<account_manager::Account>
ChromeSigninClient::GetInitialPrimaryAccount() {
  if (!IsAccountManagerAvailable(profile_)) {
    // Secondary Profiles in Lacros do not start with the Device Account signed
    // in.
    return absl::nullopt;
  }

  const crosapi::mojom::AccountPtr& device_account =
      chromeos::LacrosChromeServiceImpl::Get()->init_params()->device_account;
  if (!device_account)
    return absl::nullopt;

  return account_manager::FromMojoAccount(device_account);
}
#endif

void ChromeSigninClient::SetURLLoaderFactoryForTest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_for_testing_ = url_loader_factory;
}

void ChromeSigninClient::OnCloseBrowsersSuccess(
    const signin_metrics::ProfileSignout signout_source_metric,
    const base::FilePath& profile_path) {
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
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
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  if (!entry)
    return;
  entry->LockForceSigninProfile(true);
}

void ChromeSigninClient::ShowUserManager(const base::FilePath& profile_path) {
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  ProfilePicker::Show(ProfilePicker::EntryPoint::kProfileLocked);
#endif
}
