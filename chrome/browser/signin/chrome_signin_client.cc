// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_client.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
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
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/url_and_title.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/metrics/metrics_service.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/base/models/tree_node_iterator.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/signin/wait_for_network_callback_helper_ash.h"
#include "chromeos/ash/components/network/network_handler.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include <optional>

#include "chrome/browser/lacros/account_manager/account_manager_util.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_util.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/profiles/profile_window.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/signin/wait_for_network_callback_helper_chrome.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/common/manifest.h"
#endif

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_oauth_multilogin_delegate_impl.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_request_throttled_handler_browser_impl.h"
#include "chrome/browser/signin/bound_session_credentials/throttled_gaia_auth_fetcher.h"
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

namespace {

// List of sources for which sign out is always allowed.
// TODO(crbug.com/40162614): core product logic should not rely on metric
// sources/callsites.  Consider removing such logic, potentially as part of
// introducing a cross-platform SigninManager.
signin_metrics::ProfileSignout kAlwaysAllowedSignoutSources[] = {
    // Allowed, because data has not been synced yet.
    signin_metrics::ProfileSignout::kAbortSignin,
    // Allowed, because the primary account must be cleared when the account is
    // removed from device. Only used on Android and Lacros.
    signin_metrics::ProfileSignout::kAccountRemovedFromDevice,
    // Allowed, for tests.
    signin_metrics::ProfileSignout::kForceSignoutAlwaysAllowedForTest,
    // Allowed, because access to this entry point is controlled to only be
    // enabled if the user may turn off sync.
    signin_metrics::ProfileSignout::kUserClickedRevokeSyncConsentSettings,
    // Allowed, because the dialog offers the option to the user to sign out.
    // Note that the dialog is only shown on iOS and isn't planned to be shown
    // on the other platforms since they already support user policies (no need
    // for a notification in that case). Still, the metric is added to the
    // kAlwaysAllowedSignoutSources for coherence.
    signin_metrics::ProfileSignout::
        kUserClickedSignoutFromUserPolicyNotificationDialog,
    // Allowed, because the profile was signed out and the account was signed in
    // to the web only before showing the sync confirmation dialog. The account
    // was signed in to the profile in order to show the sync confirmation.
    signin_metrics::ProfileSignout::kCancelSyncConfirmationOnWebOnlySignedIn,
    // Allowed as the user wasn't signed in initially and data has not been
    // synced yet.
    signin_metrics::ProfileSignout::kCancelSyncConfirmationRemoveAccount,
    // Data not synced yet.
    // Used when moving the primary account (e.g. profile switch).
    signin_metrics::ProfileSignout::kMovePrimaryAccount,
    // Allowed as the profile is being deleted anyway.
    signin_metrics::ProfileSignout::kSignoutDuringProfileDeletion,
};

// Returns the histogram suffix name per group of `signin_metrics::AccessPoint`.
std::string_view NameOfGroupedAccessPointHistogram(
    signin_metrics::AccessPoint access_point) {
  switch (access_point) {
    case signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN:
      return ".PreUnoWebSignin";
    case signin_metrics::AccessPoint::
        ACCESS_POINT_CHROME_SIGNIN_INTERCEPT_BUBBLE:
      return ".UnoSigninBubble";
    case signin_metrics::AccessPoint::ACCESS_POINT_USER_MANAGER:
    case signin_metrics::AccessPoint::ACCESS_POINT_FOR_YOU_FRE:
    case signin_metrics::AccessPoint::
        ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE:
    case signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE:
      return ".ProfileCreation";
    case signin_metrics::AccessPoint::ACCESS_POINT_AVATAR_BUBBLE_SIGN_IN:
      return ".ProfileMenu";
    default:
      return ".Other";
  }
}

void RecordBookmarksCounts(signin_metrics::AccessPoint access_point,
                           signin::ConsentLevel consent_level,
                           size_t all_bookmarks_count,
                           size_t bar_bookmarks_count) {
  static constexpr std::string_view kBaseHistogramName = "Signin.Bookmarks";

  std::string_view consent_level_token =
      consent_level == signin::ConsentLevel::kSignin ? ".OnSignin" : ".OnSync";

  std::string all_bookmarks_histogram_name =
      base::StrCat({kBaseHistogramName, consent_level_token, ".AllBookmarks"});
  base::UmaHistogramCounts1000(all_bookmarks_histogram_name,
                               all_bookmarks_count);
  base::UmaHistogramCounts1000(
      base::StrCat({all_bookmarks_histogram_name,
                    NameOfGroupedAccessPointHistogram(access_point)}),
      all_bookmarks_count);

  std::string bar_bookmarks_histogram_name =
      base::StrCat({kBaseHistogramName, consent_level_token, ".BookmarksBar"});
  base::UmaHistogramCounts1000(bar_bookmarks_histogram_name,
                               bar_bookmarks_count);
  base::UmaHistogramCounts1000(
      base::StrCat({bar_bookmarks_histogram_name,
                    NameOfGroupedAccessPointHistogram(access_point)}),
      bar_bookmarks_count);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void RecordExtensionsCounts(signin_metrics::AccessPoint access_point,
                            signin::ConsentLevel consent_level,
                            int extensions_count) {
  static constexpr std::string_view kBaseHistogramName = "Signin.Extensions";

  std::string_view consent_level_token =
      consent_level == signin::ConsentLevel::kSignin ? ".OnSignin" : ".OnSync";

  base::UmaHistogramCounts1000(
      base::StrCat({kBaseHistogramName, consent_level_token}),
      extensions_count);
  base::UmaHistogramCounts1000(
      base::StrCat({kBaseHistogramName, consent_level_token,
                    NameOfGroupedAccessPointHistogram(access_point)}),
      extensions_count);
}
#endif

}  // namespace

ChromeSigninClient::ChromeSigninClient(Profile* profile)
    : wait_for_network_callback_helper_(
#if BUILDFLAG(IS_CHROMEOS_ASH)
          std::make_unique<WaitForNetworkCallbackHelperAsh>()
#else
          std::make_unique<WaitForNetworkCallbackHelperChrome>()
#endif
              ),
      profile_(profile) {
}

ChromeSigninClient::~ChromeSigninClient() = default;

void ChromeSigninClient::DoFinalInit() {
  VerifySyncToken();
}

// static
bool ChromeSigninClient::ProfileAllowsSigninCookies(Profile* profile) {
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(profile);
  return signin::SettingsAllowSigninCookies(cookie_settings.get());
}

PrefService* ChromeSigninClient::GetPrefs() {
  return profile_->GetPrefs();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeSigninClient::GetURLLoaderFactory() {
  if (url_loader_factory_for_testing_) {
    return url_loader_factory_for_testing_;
  }

  return profile_->GetDefaultStoragePartition()
      ->GetURLLoaderFactoryForBrowserProcess();
}

network::mojom::CookieManager* ChromeSigninClient::GetCookieManager() {
  return profile_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

network::mojom::NetworkContext* ChromeSigninClient::GetNetworkContext() {
  return profile_->GetDefaultStoragePartition()->GetNetworkContext();
}

bool ChromeSigninClient::AreSigninCookiesAllowed() {
  return ProfileAllowsSigninCookies(profile_);
}

bool ChromeSigninClient::AreSigninCookiesDeletedOnExit() {
  scoped_refptr<content_settings::CookieSettings> cookie_settings =
      CookieSettingsFactory::GetForProfile(profile_);
  return signin::SettingsDeleteSigninCookiesOnExit(cookie_settings.get());
}

void ChromeSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
  HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(observer);
}

void ChromeSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
  HostContentSettingsMapFactory::GetForProfile(profile_)->RemoveObserver(
      observer);
}

bool ChromeSigninClient::IsClearPrimaryAccountAllowed(
    bool has_sync_account) const {
  return GetSignoutDecision(has_sync_account,
                            /*signout_source=*/std::nullopt) ==
         SigninClient::SignoutDecision::ALLOW;
}

bool ChromeSigninClient::IsRevokeSyncConsentAllowed() const {
  return GetSignoutDecision(/*has_sync_account=*/true,
                            /*signout_source=*/std::nullopt) !=
         SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED;
}

void ChromeSigninClient::PreSignOut(
    base::OnceCallback<void(SignoutDecision)> on_signout_decision_reached,
    signin_metrics::ProfileSignout signout_source_metric,
    bool has_sync_account) {
  DCHECK(on_signout_decision_reached);
  DCHECK(!on_signout_decision_reached_) << "SignOut already in-progress!";
  on_signout_decision_reached_ = std::move(on_signout_decision_reached);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // `signout_source_metric` is `signin_metrics::ProfileSignout::kAbortSignin`
  // if the user declines sync in the signin process. In case the user accepts
  // the managed account but declines sync, we should keep the window open.
  // `signout_source_metric` is
  // `signin_metrics::ProfileSignout::kRevokeSyncFromSettings` when the user
  // turns off sync from the settings, we should also keep the window open at
  // this point.
  // TODO(crbug.com/40280466): Check for managed accounts to be modified
  // when aligning Managed vs Consumer accounts.
  bool user_declines_sync_after_consenting_to_management =
      (signout_source_metric == signin_metrics::ProfileSignout::kAbortSignin ||
       signout_source_metric ==
           signin_metrics::ProfileSignout::kRevokeSyncFromSettings ||
       signout_source_metric == signin_metrics::ProfileSignout::
                                    kCancelSyncConfirmationOnWebOnlySignedIn) &&
      enterprise_util::UserAcceptedAccountManagement(profile_);
  // These sign out won't remove the policy cache, keep the window opened.
  bool keep_window_opened =
      signout_source_metric ==
          signin_metrics::ProfileSignout::kGoogleServiceNamePatternChanged ||
      signout_source_metric ==
          signin_metrics::ProfileSignout::kServerForcedDisable ||
      signout_source_metric == signin_metrics::ProfileSignout::kPrefChanged ||
      user_declines_sync_after_consenting_to_management;
  if (signin_util::IsForceSigninEnabled() && !profile_->IsSystemProfile() &&
      !profile_->IsGuestSession() && !profile_->IsChild() &&
      !keep_window_opened) {
    BrowserList::CloseAllBrowsersWithProfile(
        profile_,
        base::BindRepeating(&ChromeSigninClient::OnCloseBrowsersSuccess,
                            base::Unretained(this), signout_source_metric,
                            /*should_sign_out=*/true, has_sync_account),
        base::BindRepeating(&ChromeSigninClient::OnCloseBrowsersAborted,
                            base::Unretained(this)),
        signout_source_metric == signin_metrics::ProfileSignout::kAbortSignin ||
            signout_source_metric == signin_metrics::ProfileSignout::
                                         kAuthenticationFailedWithForceSignin ||
            signout_source_metric ==
                signin_metrics::ProfileSignout::
                    kCancelSyncConfirmationOnWebOnlySignedIn);
  } else {
#else
  {
#endif
    std::move(on_signout_decision_reached_)
        .Run(GetSignoutDecision(has_sync_account, signout_source_metric));
  }
}

bool ChromeSigninClient::AreNetworkCallsDelayed() {
  return wait_for_network_callback_helper_->AreNetworkCallsDelayed();
}

void ChromeSigninClient::DelayNetworkCall(base::OnceClosure callback) {
  wait_for_network_callback_helper_->DelayNetworkCall(std::move(callback));
}

std::unique_ptr<GaiaAuthFetcher> ChromeSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source) {
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  if (BoundSessionCookieRefreshService* bound_session_cookie_refresh_service =
          BoundSessionCookieRefreshServiceFactory::GetForProfile(profile_);
      bound_session_cookie_refresh_service) {
    CHECK(switches::IsBoundSessionCredentialsEnabled(profile_->GetPrefs()));
    return std::make_unique<ThrottledGaiaAuthFetcher>(
        consumer, source, GetURLLoaderFactory(),
        bound_session_cookie_refresh_service->GetBoundSessionThrottlerParams(),
        std::make_unique<BoundSessionRequestThrottledHandlerBrowserImpl>(
            *bound_session_cookie_refresh_service));
  }
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  return std::make_unique<GaiaAuthFetcher>(consumer, source,
                                           GetURLLoaderFactory());
}

version_info::Channel ChromeSigninClient::GetClientChannel() {
  return chrome::GetChannel();
}

void ChromeSigninClient::OnPrimaryAccountChanged(
    signin::PrimaryAccountChangeEvent event_details) {
  for (signin::ConsentLevel consent_level :
       {signin::ConsentLevel::kSignin, signin::ConsentLevel::kSync}) {
    // Only record metrics when setting the primary account.
    switch (event_details.GetEventTypeFor(consent_level)) {
      case signin::PrimaryAccountChangeEvent::Type::kNone:
      case signin::PrimaryAccountChangeEvent::Type::kCleared:
        break;
      case signin::PrimaryAccountChangeEvent::Type::kSet:
        CHECK(event_details.GetSetPrimaryAccountAccessPoint().has_value());
        signin_metrics::AccessPoint access_point =
            event_details.GetSetPrimaryAccountAccessPoint().value();

        std::optional<size_t> all_bookmarks_count = GetAllBookmarksCount();
        std::optional<size_t> bar_bookmarks_count =
            GetBookmarkBarBookmarksCount();
        if (all_bookmarks_count.has_value() &&
            bar_bookmarks_count.has_value()) {
          RecordBookmarksCounts(access_point, consent_level,
                                all_bookmarks_count.value(),
                                bar_bookmarks_count.value());
        }

#if BUILDFLAG(ENABLE_EXTENSIONS)
        std::optional<size_t> extensions_count = GetExtensionsCount();
        if (extensions_count.has_value()) {
          RecordExtensionsCounts(access_point, consent_level,
                                 extensions_count.value());
        }
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
        RecordOpenTabCount(access_point, consent_level);
#endif
    }
  }
}

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
std::unique_ptr<signin::BoundSessionOAuthMultiLoginDelegate>
ChromeSigninClient::CreateBoundSessionOAuthMultiloginDelegate() const {
  if (BoundSessionCookieRefreshService* bound_session_cookie_refresh_service =
          BoundSessionCookieRefreshServiceFactory::GetForProfile(profile_);
      bound_session_cookie_refresh_service) {
    return std::make_unique<BoundSessionOAuthMultiLoginDelegateImpl>(
        bound_session_cookie_refresh_service->GetWeakPtr());
  }
  return nullptr;
}
#endif

SigninClient::SignoutDecision ChromeSigninClient::GetSignoutDecision(
    bool has_sync_account,
    const std::optional<signin_metrics::ProfileSignout> signout_source) const {
  // TODO(crbug.com/40239707): Revisit |kAlwaysAllowedSignoutSources| in general
  // and for Lacros main profile.
  for (const auto& always_allowed_source : kAlwaysAllowedSignoutSources) {
    if (!signout_source.has_value()) {
      break;
    }
    if (signout_source.value() == always_allowed_source) {
      return SigninClient::SignoutDecision::ALLOW;
    }
  }

  if (is_clear_primary_account_allowed_for_testing_.has_value()) {
    return is_clear_primary_account_allowed_for_testing_.value();
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The primary account in Lacros main profile must be the device account and
  // can't be changed/cleared.
  if (profile_->IsMainProfile()) {
    return SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED;
  }
#endif
#if BUILDFLAG(IS_ANDROID)
  // On Android we do not allow supervised users to sign out.
  // We also don't allow sign out on ChromeOS, though this is enforced outside
  // the scope of this method.
  // Other platforms do not restrict signout of supervised users.
  if (profile_->IsChild()) {
    return SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED;
  }
#endif

// Android allows signing out of Managed accounts.
#if !BUILDFLAG(IS_ANDROID)
  // Check if managed user.
  if (enterprise_util::UserAcceptedAccountManagement(profile_)) {
    if (base::FeatureList::IsEnabled(kDisallowManagedProfileSignout)) {
      // Allow revoke sync but disallow signout regardless of consent level of
      // the primary account.
      return SigninClient::SignoutDecision::CLEAR_PRIMARY_ACCOUNT_DISALLOWED;
    }
    // Syncing users are not allowed to revoke sync or signout. Signed in non-
    // syncing users don't have any signout restrictions related to management.
    if (has_sync_account) {
      return SigninClient::SignoutDecision::REVOKE_SYNC_DISALLOWED;
    }
  }
#endif
  return SigninClient::SignoutDecision::ALLOW;
}

void ChromeSigninClient::VerifySyncToken() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // We only verify the token once when Profile is just created.
  if (signin_util::IsForceSigninEnabled() && !force_signin_verifier_) {
    force_signin_verifier_ = std::make_unique<ForceSigninVerifier>(
        profile_, IdentityManagerFactory::GetForProfile(profile_),
        base::BindOnce(&ChromeSigninClient::OnTokenFetchComplete,
                       base::Unretained(this)));
  }
#endif
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeSigninClient::OnTokenFetchComplete(bool token_is_valid) {
  // If the token is valid we do need to do anything special and let the user
  // proceed.
  if (token_is_valid) {
    return;
  }

  // Token is not valid, we close all the browsers and open the Profile
  // Picker.
  should_display_user_manager_ = true;
  BrowserList::CloseAllBrowsersWithProfile(
      profile_,
      base::BindRepeating(
          &ChromeSigninClient::OnCloseBrowsersSuccess, base::Unretained(this),
          signin_metrics::ProfileSignout::kAuthenticationFailedWithForceSignin,
          // Do not sign the user out to allow them to reauthenticate from the
          // profile picker.
          /*should_sign_out=*/false,
          // Sync value is not used since we are not signing out.
          /*has_sync_account=*/false),
      /*on_close_aborted=*/base::DoNothing(),
      /*skip_beforeunload=*/true);
}
#endif

std::optional<size_t> ChromeSigninClient::GetAllBookmarksCount() {
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  if (!bookmarks || !bookmarks->root_node()) {
    return std::nullopt;
  }

  // Recursive traversal of the root node, counting URLs only.
  size_t count = 0;
  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(
      bookmarks->root_node());
  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* const node = iterator.Next();
    // Skip folders.
    if (node->is_url()) {
      ++count;
    }
  }
  return count;
}

std::optional<size_t> ChromeSigninClient::GetBookmarkBarBookmarksCount() {
  bookmarks::BookmarkModel* bookmarks =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  if (!bookmarks || !bookmarks->bookmark_bar_node()) {
    return std::nullopt;
  }

  // It is intended that we only count the visible bookmarks on the bar, meaning
  // we are not interested in the bookmarks within a folder or subfolder of the
  // bar. Counting the children only gets us the first layer that appears on the
  // bar which is the count we need (Note: a folder on that layer counts as 1).
  return bookmarks->bookmark_bar_node()->children().size();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
std::optional<size_t> ChromeSigninClient::GetExtensionsCount() {
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistryFactory::GetForBrowserContext(profile_);
  if (!registry) {
    return std::nullopt;
  }

  size_t user_installed_extension_count = 0;
  for (auto& extension : registry->enabled_extensions()) {
    // Mimics the count done for the Histograms `Extensions.LoadExtensionUser2`
    // that counts the user installed extensions.
    if (extension->is_extension() &&
        !extensions::Manifest::IsExternalLocation(extension->location()) &&
        !extensions::Manifest::IsUnpackedLocation(extension->location()) &&
        !extensions::Manifest::IsComponentLocation(extension->location())) {
      ++user_installed_extension_count;
    }
  }

  return user_installed_extension_count;
}
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeSigninClient::RecordOpenTabCount(
    signin_metrics::AccessPoint access_point,
    signin::ConsentLevel consent_level) {
  size_t tabs_count = 0;

#if BUILDFLAG(IS_ANDROID)
  for (const TabModel* model : TabModelList::models()) {
    // Note: Even though on Android only a single regular profile is supported,
    // there can also be an incognito profile which should be excluded here.
    if (model->GetProfile() != profile_) {
      continue;
    }

    tabs_count += model->GetTabCount();
  }
#else   // !BUILDFLAG(IS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile_) {
      continue;
    }
    if (TabStripModel* tab_strip_model = browser->tab_strip_model()) {
      tabs_count += tab_strip_model->count();
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  signin_metrics::RecordOpenTabCountOnSignin(access_point, consent_level,
                                             tabs_count);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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
std::optional<account_manager::Account>
ChromeSigninClient::GetInitialPrimaryAccount() {
  if (!profile_->IsMainProfile()) {
    return std::nullopt;
  }

  const crosapi::mojom::AccountPtr& device_account =
      chromeos::BrowserParamsProxy::Get()->DeviceAccount();
  if (!device_account) {
    return std::nullopt;
  }

  return account_manager::FromMojoAccount(device_account);
}

// Returns whether the account that must be auto-signed-in to the main profile
// in Lacros is a child account.
// Returns false for guest session, public session, kiosk, demo mode and Active
// Directory account.
// Returns null for secondary / non-main profiles in LaCrOS.
std::optional<bool> ChromeSigninClient::IsInitialPrimaryAccountChild() const {
  if (!profile_->IsMainProfile()) {
    return std::nullopt;
  }

  const bool is_child_session =
      chromeos::BrowserParamsProxy::Get()->SessionType() ==
      crosapi::mojom::SessionType::kChildSession;
  return is_child_session;
}

void ChromeSigninClient::RemoveAccount(
    const account_manager::AccountKey& account_key) {
  std::optional<account_manager::Account> device_account =
      GetInitialPrimaryAccount();
  if (device_account.has_value() && device_account->key == account_key) {
    DLOG(ERROR)
        << "The primary account should not be removed from the main profile";
    return;
  }

  g_browser_process->profile_manager()
      ->GetAccountProfileMapper()
      ->RemoveAccount(profile_->GetPath(), account_key);
}

void ChromeSigninClient::RemoveAllAccounts() {
  if (GetInitialPrimaryAccount().has_value()) {
    DLOG(ERROR) << "It is not allowed to remove the initial primary account.";
    return;
  }

  DCHECK(!profile_->IsMainProfile());
  g_browser_process->profile_manager()
      ->GetAccountProfileMapper()
      ->RemoveAllAccounts(profile_->GetPath());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void ChromeSigninClient::SetURLLoaderFactoryForTest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_for_testing_ = url_loader_factory;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Do not make network requests in unit tests. ash::NetworkHandler should
  // not be used and is not expected to have been initialized in unit tests.
  wait_for_network_callback_helper_
      ->DisableNetworkCallsDelayedForTesting(  // IN-TEST
          url_loader_factory_for_testing_ &&
          !ash::NetworkHandler::IsInitialized());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ChromeSigninClient::OnCloseBrowsersSuccess(
    const signin_metrics::ProfileSignout signout_source_metric,
    bool should_sign_out,
    bool has_sync_account,
    const base::FilePath& profile_path) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (signin_util::IsForceSigninEnabled() && force_signin_verifier_.get()) {
    force_signin_verifier_->Cancel();
  }
#endif

  if (should_sign_out) {
    std::move(on_signout_decision_reached_)
        .Run(GetSignoutDecision(has_sync_account, signout_source_metric));
  }

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
      .Run(SignoutDecision::REVOKE_SYNC_DISALLOWED);
}

void ChromeSigninClient::LockForceSigninProfile(
    const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_->GetPath());
  if (!entry) {
    return;
  }
  entry->LockForceSigninProfile(true);
}

void ChromeSigninClient::ShowUserManager(const base::FilePath& profile_path) {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileLocked));
#endif
}
