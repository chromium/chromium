// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/safe_browsing/user_population.h"
#include "chrome/browser/safe_browsing/verdict_cache_manager_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/google/core/common/google_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/ui/password_check_referrer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/content/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/content/password_protection/password_protection_request_content.h"
#include "components/safe_browsing/content/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/csd.pb.h"
#include "components/safe_browsing/core/triggers/trigger_throttler.h"
#include "components/safe_browsing/core/verdict_cache_manager.h"
#include "components/security_interstitials/content/unsafe_resource_util.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync_user_events/user_event_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/url_util.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/safe_browsing/android/password_reuse_controller_android.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_referring_app_bridge_android.h"
#else
#include "chrome/browser/ui/browser_list.h"
#endif

using base::RecordAction;
using base::UserMetricsAction;
using content::BrowserThread;
using sync_pb::GaiaPasswordReuse;
using sync_pb::UserEventSpecifics;
using GaiaPasswordCaptured = UserEventSpecifics::GaiaPasswordCaptured;
using PasswordReuseDialogInteraction =
    GaiaPasswordReuse::PasswordReuseDialogInteraction;
using PasswordReuseLookup = GaiaPasswordReuse::PasswordReuseLookup;
using PasswordReuseEvent =
    safe_browsing::LoginReputationClientRequest::PasswordReuseEvent;
using SafeBrowsingStatus =
    GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus;

namespace safe_browsing {

using ReusedPasswordAccountType =
    LoginReputationClientRequest::PasswordReuseEvent::ReusedPasswordAccountType;

namespace {

// The number of user gestures we trace back for login event attribution.
const int kPasswordEventAttributionUserGestureLimit = 2;

// Probability for sending password protection reports for domains on the
// allowlist for users opted into extended reporting, from non-incognito window.
const float kProbabilityForSendingReportsFromSafeURLs = 0.01;

// If user specifically mark a site as legitimate, we will keep this decision
// for 2 days.
const int kOverrideVerdictCacheDurationSec = 2 * 24 * 60 * 60;

// Frequency to log PasswordCapture event log. Random 24-28 days.
const int kPasswordCaptureEventLogFreqDaysMin = 24;
const int kPasswordCaptureEventLogFreqDaysExtra = 4;

int64_t GetMicrosecondsSinceWindowsEpoch(base::Time time) {
  return (time - base::Time()).InMicroseconds();
}

PasswordReuseLookup::ReputationVerdict GetVerdictToLogFromResponse(
    LoginReputationClientResponse::VerdictType response_verdict) {
  switch (response_verdict) {
    case LoginReputationClientResponse::SAFE:
      return PasswordReuseLookup::SAFE;
    case LoginReputationClientResponse::LOW_REPUTATION:
      return PasswordReuseLookup::LOW_REPUTATION;
    case LoginReputationClientResponse::PHISHING:
      return PasswordReuseLookup::PHISHING;
    case LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED:
      return PasswordReuseLookup::VERDICT_UNSPECIFIED;
  }
  NOTREACHED() << "Unexpected response_verdict: " << response_verdict;
  return PasswordReuseLookup::VERDICT_UNSPECIFIED;
}

// Given a |web_contents|, returns the navigation id of its last committed
// navigation.
int64_t GetLastCommittedNavigationID(content::WebContents* web_contents) {
  if (!web_contents)
    return 0;
  content::NavigationEntry* navigation =
      web_contents->GetController().GetLastCommittedEntry();
  return navigation
             ? GetMicrosecondsSinceWindowsEpoch(navigation->GetTimestamp())
             : 0;
}

// Opens a |url| from |current_web_contents| with |referrer|. |in_new_tab|
// indicates if opening in a new foreground tab or in current tab.
void OpenUrl(content::WebContents* current_web_contents,
             const GURL& url,
             const content::Referrer& referrer,
             bool in_new_tab) {
  content::OpenURLParams params(url, referrer,
                                in_new_tab
                                    ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                    : WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/false);
  current_web_contents->OpenURL(params);
}

int64_t GetNavigationIDFromPrefsByOrigin(PrefService* prefs,
                                         const Origin& origin) {
  const base::DictionaryValue* unhandled_sync_password_reuses =
      prefs->GetDictionary(prefs::kSafeBrowsingUnhandledGaiaPasswordReuses);
  if (!unhandled_sync_password_reuses)
    return 0;

  const base::Value* navigation_id_value =
      unhandled_sync_password_reuses->FindKey(origin.Serialize());

  int64_t navigation_id;
  return navigation_id_value &&
                 base::StringToInt64(navigation_id_value->GetString(),
                                     &navigation_id)
             ? navigation_id
             : 0;
}

// Return a new UserEventSpecifics w/o the navigation_id populated
std::unique_ptr<UserEventSpecifics> GetNewUserEventSpecifics() {
  auto specifics = std::make_unique<UserEventSpecifics>();
  specifics->set_event_time_usec(
      GetMicrosecondsSinceWindowsEpoch(base::Time::Now()));
  return specifics;
}

// Return a new UserEventSpecifics w/ the navigation_id populated
std::unique_ptr<UserEventSpecifics> GetUserEventSpecificsWithNavigationId(
    int64_t navigation_id) {
  if (navigation_id <= 0)
    return nullptr;

  auto specifics = GetNewUserEventSpecifics();
  specifics->set_navigation_id(navigation_id);
  return specifics;
}

// Return a new UserEventSpecifics populated from the web_contents
std::unique_ptr<UserEventSpecifics> GetUserEventSpecifics(
    content::WebContents* web_contents) {
  return GetUserEventSpecificsWithNavigationId(
      GetLastCommittedNavigationID(web_contents));
}

}  // namespace

ChromePasswordProtectionService::ChromePasswordProtectionService(
    SafeBrowsingService* sb_service,
    Profile* profile)
    : PasswordProtectionService(
          sb_service->database_manager(),
          sb_service->GetURLLoaderFactory(profile),
          HistoryServiceFactory::GetForProfile(
              profile,
              ServiceAccessType::EXPLICIT_ACCESS),
          profile->GetPrefs(),
          std::make_unique<SafeBrowsingPrimaryAccountTokenFetcher>(
              IdentityManagerFactory::GetForProfile(profile)),
          profile->IsOffTheRecord(),
          IdentityManagerFactory::GetForProfile(profile),
          /*try_token_fetch=*/true),
      ui_manager_(sb_service->ui_manager()),
      trigger_manager_(sb_service->trigger_manager()),
      profile_(profile),
      navigation_observer_manager_(sb_service->navigation_observer_manager()),
      pref_change_registrar_(new PrefChangeRegistrar),
      cache_manager_(VerdictCacheManagerFactory::GetForProfile(profile)) {
  pref_change_registrar_->Init(profile_->GetPrefs());

  scoped_refptr<password_manager::PasswordStore> password_store =
      GetProfilePasswordStore();
  // Password store can be null in tests.
  if (password_store) {
    // Subscribe to gaia hash password changes change notifications.
    hash_password_manager_subscription_ =
        password_store->RegisterStateCallbackOnHashPasswordManager(
            base::BindRepeating(&ChromePasswordProtectionService::
                                    CheckGaiaPasswordChangeForAllSignedInUsers,
                                base::Unretained(this)));
  }
  pref_change_registrar_->Add(
      prefs::kPasswordProtectionWarningTrigger,
      base::BindRepeating(
          &ChromePasswordProtectionService::OnWarningTriggerChanged,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kPasswordProtectionLoginURLs,
      base::BindRepeating(
          &ChromePasswordProtectionService::OnEnterprisePasswordUrlChanged,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kPasswordProtectionChangePasswordURL,
      base::BindRepeating(
          &ChromePasswordProtectionService::OnEnterprisePasswordUrlChanged,
          base::Unretained(this)));

  // TODO(nparker) Move the rest of the above code into Init()
  // without crashing unittests.
  Init();
}

void ChromePasswordProtectionService::Init() {
// The following code is disabled on Android. RefreshTokenIsAvailable cannot be
// used in unit tests, because it needs to interact with system accounts.
// Considering avoid running it during unit tests. See: crbug.com/1009957.
#if !defined(OS_ANDROID)
  // This code is shared by the normal ctor and testing ctor.

  sync_password_hash_ = GetSyncPasswordHashFromPrefs();
  if (!sync_password_hash_.empty()) {
    // Set a timer for when next to log the PasswordCapture event. The timer
    // value is stored in a pref to carry across restarts.
    base::TimeDelta delay =
        GetDelayFromPref(profile_->GetPrefs(),
                         prefs::kSafeBrowsingNextPasswordCaptureEventLogTime);

    // Bound it between 1 min and 28 days. Handles clock-resets.  We wait
    // 1 min to not slowdown browser-startup, and to improve the
    // probability that the sync system is initialized.
    base::TimeDelta min_delay = base::TimeDelta::FromMinutes(1);
    base::TimeDelta max_delay =
        base::TimeDelta::FromDays(kPasswordCaptureEventLogFreqDaysMin +
                                  kPasswordCaptureEventLogFreqDaysExtra);
    if (delay < min_delay)
      delay = min_delay;
    else if (delay > max_delay)
      delay = max_delay;
    SetLogPasswordCaptureTimer(delay);
  }
#endif
}

void ChromePasswordProtectionService::Shutdown() {
  if (pref_change_registrar_)
    pref_change_registrar_->RemoveAll();
  hash_password_manager_subscription_ = {};
}

ChromePasswordProtectionService::~ChromePasswordProtectionService() = default;

// static
ChromePasswordProtectionService*
ChromePasswordProtectionService::GetPasswordProtectionService(
    Profile* profile) {
  if (g_browser_process && g_browser_process->safe_browsing_service()) {
    return static_cast<safe_browsing::ChromePasswordProtectionService*>(
        g_browser_process->safe_browsing_service()
            ->GetPasswordProtectionService(profile));
  }
  return nullptr;
}

// static
bool ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
    content::WebContents* web_contents,
    PasswordType password_type) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  ChromePasswordProtectionService* service =
      ChromePasswordProtectionService::GetPasswordProtectionService(profile);

  // |service| could be null if safe browsing service is disabled.
  if (!service)
    return false;

  if (password_type == PasswordType::ENTERPRISE_PASSWORD)
    return service->HasUnhandledEnterprisePasswordReuse(web_contents);

  bool enable_warning_for_non_sync_users = base::FeatureList::IsEnabled(
      safe_browsing::kPasswordProtectionForSignedInUsers);
  DCHECK(password_type == PasswordType::PRIMARY_ACCOUNT_PASSWORD ||
         password_type == PasswordType::SAVED_PASSWORD ||
         (enable_warning_for_non_sync_users &&
          password_type == PasswordType::OTHER_GAIA_PASSWORD));
  // Otherwise, checks if there's any unhandled sync password reuses matches
  // this origin.
  auto* unhandled_sync_password_reuses = profile->GetPrefs()->GetDictionary(
      prefs::kSafeBrowsingUnhandledGaiaPasswordReuses);
  return unhandled_sync_password_reuses
             ? (unhandled_sync_password_reuses->FindKey(
                    Origin::Create(web_contents->GetLastCommittedURL())
                        .Serialize()) != nullptr)
             : false;
}

safe_browsing::LoginReputationClientRequest::UrlDisplayExperiment
ChromePasswordProtectionService::GetUrlDisplayExperiment() const {
  safe_browsing::LoginReputationClientRequest::UrlDisplayExperiment experiment;
  experiment.set_simplified_url_display_enabled(
      base::FeatureList::IsEnabled(safe_browsing::kSimplifiedUrlDisplay));
  // Delayed warnings parameters:
  experiment.set_delayed_warnings_enabled(
      base::FeatureList::IsEnabled(safe_browsing::kDelayedWarnings));
  experiment.set_delayed_warnings_mouse_clicks_enabled(
      safe_browsing::kDelayedWarningsEnableMouseClicks.Get());
  // Actual URL display experiments:
  experiment.set_reveal_on_hover(base::FeatureList::IsEnabled(
      omnibox::kRevealSteadyStateUrlPathQueryAndRefOnHover));
  experiment.set_hide_on_interaction(base::FeatureList::IsEnabled(
      omnibox::kHideSteadyStateUrlPathQueryAndRefOnInteraction));
  experiment.set_elide_to_registrable_domain(
      base::FeatureList::IsEnabled(omnibox::kMaybeElideToRegistrableDomain));
  return experiment;
}

void ChromePasswordProtectionService::ShowModalWarning(
    PasswordProtectionRequest* request,
    LoginReputationClientResponse::VerdictType verdict_type,
    const std::string& verdict_token,
    ReusedPasswordAccountType password_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(password_type.account_type() == ReusedPasswordAccountType::GMAIL ||
         password_type.account_type() == ReusedPasswordAccountType::GSUITE ||
         password_type.account_type() ==
             ReusedPasswordAccountType::NON_GAIA_ENTERPRISE ||
         (password_type.account_type() ==
          ReusedPasswordAccountType::SAVED_PASSWORD));
  PasswordProtectionRequestContent* request_content =
      static_cast<PasswordProtectionRequestContent*>(request);
  content::WebContents* web_contents = request_content->web_contents();
  RequestOutcome outcome = request->request_outcome();
  // Don't show warning again if there is already a modal warning showing.
  if (IsModalWarningShowingInWebContents(web_contents))
    return;

  // Exit fullscreen if this |web_contents| is showing in fullscreen mode.
  if (web_contents->IsFullscreen())
    web_contents->ExitFullscreen(true);

#if defined(OS_ANDROID)
  (new PasswordReuseControllerAndroid(
       web_contents, this, password_type,
       base::BindOnce(&ChromePasswordProtectionService::OnUserAction,
                      base::Unretained(this), web_contents, password_type,
                      outcome, verdict_type, verdict_token,
                      WarningUIType::MODAL_DIALOG)))
      ->ShowDialog();
#else   // !defined(OS_ANDROID)
  ShowPasswordReuseModalWarningDialog(
      web_contents, this, password_type,
      base::BindOnce(&ChromePasswordProtectionService::OnUserAction,
                     base::Unretained(this), web_contents, password_type,
                     outcome, verdict_type, verdict_token,
                     WarningUIType::MODAL_DIALOG));
#endif  // defined(OS_ANDROID)

  LogWarningAction(WarningUIType::MODAL_DIALOG, WarningAction::SHOWN,
                   password_type);
  switch (password_type.account_type()) {
    case ReusedPasswordAccountType::SAVED_PASSWORD:
      OnModalWarningShownForSavedPassword(web_contents, password_type,
                                          verdict_token);
      break;
    case ReusedPasswordAccountType::GMAIL:
    case ReusedPasswordAccountType::GSUITE:
      OnModalWarningShownForGaiaPassword(web_contents, password_type,
                                         verdict_token);
      break;
    case ReusedPasswordAccountType::NON_GAIA_ENTERPRISE:
      OnModalWarningShownForEnterprisePassword(web_contents, password_type,
                                               verdict_token);
      break;
    default:
      return;
  }
}

void ChromePasswordProtectionService::OnModalWarningShownForSavedPassword(
    content::WebContents* web_contents,
    ReusedPasswordAccountType password_type,
    const std::string& verdict_token) {
  UpdateSecurityState(SB_THREAT_TYPE_SAVED_PASSWORD_REUSE, password_type,
                      web_contents);
  // Starts preparing post-warning report.
  MaybeStartThreatDetailsCollection(web_contents, verdict_token, password_type);
}

void ChromePasswordProtectionService::OnModalWarningShownForGaiaPassword(
    content::WebContents* web_contents,
    ReusedPasswordAccountType password_type,
    const std::string& verdict_token) {
  if (!IsIncognito()) {
    DictionaryPrefUpdate update(
        profile_->GetPrefs(), prefs::kSafeBrowsingUnhandledGaiaPasswordReuses);
    // Since base::Value doesn't support int64_t type, we convert the navigation
    // ID to string format and store it in the preference dictionary.
    update->SetKey(
        Origin::Create(web_contents->GetLastCommittedURL()).Serialize(),
        base::Value(
            base::NumberToString(GetLastCommittedNavigationID(web_contents))));
  }
  SBThreatType threat_type;
  if (password_type.is_account_syncing()) {
    threat_type = SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE;
  } else {
    threat_type = SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE;
  }
  UpdateSecurityState(threat_type, password_type, web_contents);

  // Starts preparing post-warning report.
  MaybeStartThreatDetailsCollection(web_contents, verdict_token, password_type);
}

void ChromePasswordProtectionService::OnModalWarningShownForEnterprisePassword(
    content::WebContents* web_contents,
    ReusedPasswordAccountType password_type,
    const std::string& verdict_token) {
  web_contents_with_unhandled_enterprise_reuses_.insert(web_contents);
  UpdateSecurityState(SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE, password_type,
                      web_contents);
  // Starts preparing post-warning report.
  MaybeStartThreatDetailsCollection(web_contents, verdict_token, password_type);
}

void ChromePasswordProtectionService::ShowInterstitial(
    content::WebContents* web_contents,
    ReusedPasswordAccountType password_type) {
  DCHECK(password_type.account_type() ==
             ReusedPasswordAccountType::NON_GAIA_ENTERPRISE ||
         password_type.account_type() == ReusedPasswordAccountType::GSUITE);
  // Exit fullscreen if this |web_contents| is showing in fullscreen mode.
  if (web_contents->IsFullscreen())
    web_contents->ExitFullscreen(/*will_cause_resize=*/true);

  content::OpenURLParams params(
      GURL(chrome::kChromeUIResetPasswordURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/false);
  std::string post_data =
      base::NumberToString(static_cast<std::underlying_type_t<PasswordType>>(
          ConvertReusedPasswordAccountTypeToPasswordType(password_type)));

  params.post_data = network::ResourceRequestBody::CreateFromBytes(
      post_data.data(), post_data.size());
  web_contents->OpenURL(params);

  LogWarningAction(WarningUIType::INTERSTITIAL, WarningAction::SHOWN,
                   password_type);
}

void ChromePasswordProtectionService::OnUserAction(
    content::WebContents* web_contents,
    ReusedPasswordAccountType password_type,
    RequestOutcome outcome,
    LoginReputationClientResponse::VerdictType verdict_type,
    const std::string& verdict_token,
    WarningUIType ui_type,
    WarningAction action) {
  // Only log modal warning dialog action for all password types except for
  // signed-in non-syncing type for now. We log for signed-in non-syncing type
  // only when we are about to send the event to SecurityEventRecorder because
  // we don't want to count non-unconsented primary accounts.
  bool is_signed_in_non_syncing =
      !password_type.is_account_syncing() &&
      (password_type.account_type() == ReusedPasswordAccountType::GMAIL ||
       password_type.account_type() == ReusedPasswordAccountType::GSUITE);
  if (!is_signed_in_non_syncing)
    LogWarningAction(ui_type, action, password_type);

  switch (ui_type) {
    case WarningUIType::PAGE_INFO:
      HandleUserActionOnPageInfo(web_contents, password_type, action);
      break;
    case WarningUIType::MODAL_DIALOG:
      HandleUserActionOnModalWarning(web_contents, password_type, outcome,
                                     verdict_type, verdict_token, action);
      break;
    case WarningUIType::INTERSTITIAL:
      DCHECK_EQ(WarningAction::CHANGE_PASSWORD, action);
      HandleResetPasswordOnInterstitial(web_contents, action);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ChromePasswordProtectionService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ChromePasswordProtectionService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ChromePasswordProtectionService::MaybeStartThreatDetailsCollection(
    content::WebContents* web_contents,
    const std::string& token,
    ReusedPasswordAccountType password_type) {
  // |trigger_manager_| can be null in test.
  if (!trigger_manager_)
    return;

  security_interstitials::UnsafeResource resource;
  if (password_type.account_type() ==
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE) {
    resource.threat_type = SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE;
  } else if (password_type.account_type() ==
             ReusedPasswordAccountType::SAVED_PASSWORD) {
    resource.threat_type = SB_THREAT_TYPE_SAVED_PASSWORD_REUSE;
  } else if (password_type.is_account_syncing()) {
    resource.threat_type = SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE;
  } else {
    resource.threat_type = SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE;
  }
  resource.url = web_contents->GetLastCommittedURL();
  resource.web_contents_getter = security_interstitials::GetWebContentsGetter(
      web_contents->GetMainFrame()->GetProcess()->GetID(),
      web_contents->GetMainFrame()->GetRoutingID());
  resource.token = token;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess();
  // Ignores the return of |StartCollectingThreatDetails()| here and
  // let TriggerManager decide whether it should start data
  // collection.
  trigger_manager_->StartCollectingThreatDetails(
      safe_browsing::TriggerType::GAIA_PASSWORD_REUSE, web_contents, resource,
      url_loader_factory, /*history_service=*/nullptr,
      TriggerManager::GetSBErrorDisplayOptions(*profile_->GetPrefs(),
                                               web_contents));
}

void ChromePasswordProtectionService::MaybeFinishCollectingThreatDetails(
    content::WebContents* web_contents,
    bool did_proceed) {
  // |trigger_manager_| can be null in test.
  if (!trigger_manager_)
    return;

  // Since we don't keep track the threat details in progress, it is safe to
  // ignore the result of |FinishCollectingThreatDetails()|. TriggerManager will
  // take care of whether report should be sent.
  trigger_manager_->FinishCollectingThreatDetails(
      safe_browsing::TriggerType::GAIA_PASSWORD_REUSE, web_contents,
      base::TimeDelta::FromMilliseconds(0), did_proceed, /*num_visit=*/0,
      TriggerManager::GetSBErrorDisplayOptions(*profile_->GetPrefs(),
                                               web_contents));
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseDetectedEvent(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (IsIncognito() && !WebUIInfoSingleton::HasListener())
    return;

  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile_);
  if (!user_event_service)
    return;

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecifics(web_contents);
  if (!specifics)
    return;

  auto* const status = specifics->mutable_gaia_password_reuse_event()
                           ->mutable_reuse_detected()
                           ->mutable_status();
  status->set_enabled(IsSafeBrowsingEnabled());

  ExtendedReportingLevel erl = GetExtendedReportingLevel(*GetPrefs());
  switch (erl) {
    case SBER_LEVEL_OFF:
      status->set_safe_browsing_reporting_population(SafeBrowsingStatus::NONE);
      break;
    case SBER_LEVEL_LEGACY:
      status->set_safe_browsing_reporting_population(
          SafeBrowsingStatus::EXTENDED_REPORTING);
      break;
    case SBER_LEVEL_SCOUT:
      status->set_safe_browsing_reporting_population(SafeBrowsingStatus::SCOUT);
      break;
  }

  WebUIInfoSingleton::GetInstance()->AddToPGEvents(*specifics);
  user_event_service->RecordUserEvent(std::move(specifics));
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseDialogInteraction(
    int64_t navigation_id,
    PasswordReuseDialogInteraction::InteractionResult interaction_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (IsIncognito() && !WebUIInfoSingleton::HasListener())
    return;

  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile_);
  if (!user_event_service)
    return;

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecificsWithNavigationId(navigation_id);
  if (!specifics)
    return;

  PasswordReuseDialogInteraction* const dialog_interaction =
      specifics->mutable_gaia_password_reuse_event()
          ->mutable_dialog_interaction();
  dialog_interaction->set_interaction_result(interaction_result);

  WebUIInfoSingleton::GetInstance()->AddToPGEvents(*specifics);
  user_event_service->RecordUserEvent(std::move(specifics));
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseLookupResult(
    content::WebContents* web_contents,
    PasswordReuseLookup::LookupResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (IsIncognito() && !WebUIInfoSingleton::HasListener())
    return;

  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile_);
  if (!user_event_service)
    return;

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecifics(web_contents);
  if (!specifics)
    return;

  auto* const reuse_lookup =
      specifics->mutable_gaia_password_reuse_event()->mutable_reuse_lookup();
  reuse_lookup->set_lookup_result(result);
  WebUIInfoSingleton::GetInstance()->AddToPGEvents(*specifics);
  user_event_service->RecordUserEvent(std::move(specifics));
}

void ChromePasswordProtectionService::
    MaybeLogPasswordReuseLookupResultWithVerdict(
        content::WebContents* web_contents,
        PasswordType password_type,
        PasswordReuseLookup::LookupResult result,
        PasswordReuseLookup::ReputationVerdict verdict,
        const std::string& verdict_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (IsIncognito() && !WebUIInfoSingleton::HasListener())
    return;

  PasswordReuseLookup reuse_lookup;
  reuse_lookup.set_lookup_result(result);
  reuse_lookup.set_verdict(verdict);
  reuse_lookup.set_verdict_token(verdict_token);

  // If password_type == OTHER_GAIA_PASSWORD, the account is not syncing.
  // Therefore, we have to use the security event recorder to log events to mark
  // the account at risk.
  if (password_type == PasswordType::OTHER_GAIA_PASSWORD) {
    sync_pb::GaiaPasswordReuse gaia_password_reuse_event;
    *gaia_password_reuse_event.mutable_reuse_lookup() = reuse_lookup;

    auto* identity_manager = IdentityManagerFactory::GetForProfileIfExists(
        profile_->GetOriginalProfile());
    if (identity_manager) {
      CoreAccountInfo unconsented_primary_account_info =
          identity_manager->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin);
      // SecurityEventRecorder only supports unconsented primary accounts.
      if (gaia::AreEmailsSame(unconsented_primary_account_info.email,
                              username_for_last_shown_warning())) {
        // We currently only send a security event recorder ONLY when a
        // signed-in non-syncing user clicks on "Protect Account" button.
        LogWarningAction(WarningUIType::MODAL_DIALOG,
                         WarningAction::CHANGE_PASSWORD,
                         GetPasswordProtectionReusedPasswordAccountType(
                             password_type, username_for_last_shown_warning()));
        WebUIInfoSingleton::GetInstance()->AddToSecurityEvents(
            gaia_password_reuse_event);
        SecurityEventRecorderFactory::GetForProfile(profile_)
            ->RecordGaiaPasswordReuse(gaia_password_reuse_event);
      }
    }
  } else {
    syncer::UserEventService* user_event_service =
        browser_sync::UserEventServiceFactory::GetForProfile(profile_);
    if (!user_event_service)
      return;

    std::unique_ptr<UserEventSpecifics> specifics =
        GetUserEventSpecifics(web_contents);
    if (!specifics)
      return;

    *(specifics->mutable_gaia_password_reuse_event())->mutable_reuse_lookup() =
        reuse_lookup;
    WebUIInfoSingleton::GetInstance()->AddToPGEvents(*specifics);
    user_event_service->RecordUserEvent(std::move(specifics));
  }
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseLookupEvent(
    content::WebContents* web_contents,
    RequestOutcome outcome,
    PasswordType password_type,
    const LoginReputationClientResponse* response) {
  switch (outcome) {
    case RequestOutcome::MATCHED_ALLOWLIST:
      MaybeLogPasswordReuseLookupResult(web_contents,
                                        PasswordReuseLookup::WHITELIST_HIT);
      break;
    case RequestOutcome::RESPONSE_ALREADY_CACHED:
      MaybeLogPasswordReuseLookupResultWithVerdict(
          web_contents, password_type, PasswordReuseLookup::CACHE_HIT,
          GetVerdictToLogFromResponse(response->verdict_type()),
          response->verdict_token());
      break;
    case RequestOutcome::SUCCEEDED:
      MaybeLogPasswordReuseLookupResultWithVerdict(
          web_contents, password_type, PasswordReuseLookup::REQUEST_SUCCESS,
          GetVerdictToLogFromResponse(response->verdict_type()),
          response->verdict_token());
      break;
    case RequestOutcome::URL_NOT_VALID_FOR_REPUTATION_COMPUTING:
      MaybeLogPasswordReuseLookupResult(web_contents,
                                        PasswordReuseLookup::URL_UNSUPPORTED);
      break;
    case RequestOutcome::MATCHED_ENTERPRISE_ALLOWLIST:
    case RequestOutcome::MATCHED_ENTERPRISE_LOGIN_URL:
    case RequestOutcome::MATCHED_ENTERPRISE_CHANGE_PASSWORD_URL:
      MaybeLogPasswordReuseLookupResult(
          web_contents, PasswordReuseLookup::ENTERPRISE_WHITELIST_HIT);
      break;
    case RequestOutcome::PASSWORD_ALERT_MODE:
    case RequestOutcome::TURNED_OFF_BY_ADMIN:
      MaybeLogPasswordReuseLookupResult(
          web_contents, PasswordReuseLookup::TURNED_OFF_BY_POLICY);
      break;
    case RequestOutcome::CANCELED:
    case RequestOutcome::TIMEDOUT:
    case RequestOutcome::DISABLED_DUE_TO_INCOGNITO:
    case RequestOutcome::REQUEST_MALFORMED:
    case RequestOutcome::FETCH_FAILED:
    case RequestOutcome::RESPONSE_MALFORMED:
    case RequestOutcome::SERVICE_DESTROYED:
    case RequestOutcome::DISABLED_DUE_TO_FEATURE_DISABLED:
    case RequestOutcome::DISABLED_DUE_TO_USER_POPULATION:
    case RequestOutcome::SAFE_BROWSING_DISABLED:
    case RequestOutcome::USER_NOT_SIGNED_IN:
    case RequestOutcome::EXCLUDED_COUNTRY:
      MaybeLogPasswordReuseLookupResult(web_contents,
                                        PasswordReuseLookup::REQUEST_FAILURE);
      break;
    case RequestOutcome::UNKNOWN:
    case RequestOutcome::DEPRECATED_NO_EXTENDED_REPORTING:
      NOTREACHED() << __FUNCTION__
                   << ": outcome: " << static_cast<int>(outcome);
      break;
  }
}

void ChromePasswordProtectionService::
    CheckGaiaPasswordChangeForAllSignedInUsers(const std::string& username) {
  // If the sync password has changed, report the change.
  std::string new_sync_password_hash = GetSyncPasswordHashFromPrefs();
  if (sync_password_hash_ != new_sync_password_hash) {
    sync_password_hash_ = new_sync_password_hash;
    OnGaiaPasswordChanged(username, /*is_other_gaia_password=*/false);
    return;
  }

  // For non sync password changes, we have to loop through all the password
  // hashes and find the hash associated with the username.
  password_manager::HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(profile_->GetPrefs());
  for (const auto& hash_data :
       hash_password_manager.RetrieveAllPasswordHashes()) {
    if (password_manager::AreUsernamesSame(
            hash_data.username, /*is_username1_gaia_account=*/true, username,
            /*is_username2_gaia_account=*/true)) {
      OnGaiaPasswordChanged(username, /*is_other_gaia_password=*/true);
      break;
    }
  }
}

void ChromePasswordProtectionService::OnGaiaPasswordChanged(
    const std::string& username,
    bool is_other_gaia_password) {
  DictionaryPrefUpdate unhandled_gaia_password_reuses(
      profile_->GetPrefs(), prefs::kSafeBrowsingUnhandledGaiaPasswordReuses);
  LogNumberOfReuseBeforeSyncPasswordChange(
      unhandled_gaia_password_reuses->size());
  unhandled_gaia_password_reuses->Clear();
  if (!is_other_gaia_password)
    MaybeLogPasswordCapture(/*did_log_in=*/true);
  for (auto& observer : observer_list_)
    observer.OnGaiaPasswordChanged();

// Disabled on Android, because enterprise reporting extension is not supported.
#if !defined(OS_ANDROID)
  // Only report if the current password changed is the primary account and it's
  // not a Gmail account or if the current password changed is a content area
  // account and it's not a Gmail account.
  if ((!is_other_gaia_password && !IsPrimaryAccountGmail()) ||
      (is_other_gaia_password && !IsOtherGaiaAccountGmail(username)))
    ReportPasswordChanged();
#endif
}

GURL ChromePasswordProtectionService::GetEnterpriseChangePasswordURL() const {
  // If change password URL is specified in preferences, returns the
  // corresponding pref value.
  GURL enterprise_change_password_url =
      GetPasswordProtectionChangePasswordURLPref(*profile_->GetPrefs());
  if (!enterprise_change_password_url.is_empty())
    return enterprise_change_password_url;

  return GetDefaultChangePasswordURL();
}

GURL ChromePasswordProtectionService::GetDefaultChangePasswordURL() const {
  // Computes the default GAIA change password URL.
  const AccountInfo account_info = GetAccountInfo();
  std::string account_email = account_info.email;
  // This page will prompt for re-auth and then will prompt for a new password.
  std::string account_url =
      "https://myaccount.google.com/signinoptions/"
      "password?utm_source=Google&utm_campaign=PhishGuard";
  url::RawCanonOutputT<char> percent_encoded_email;
  url::RawCanonOutputT<char> percent_encoded_account_url;
  url::EncodeURIComponent(account_email.c_str(), account_email.length(),
                          &percent_encoded_email);
  url::EncodeURIComponent(account_url.c_str(), account_url.length(),
                          &percent_encoded_account_url);
  GURL change_password_url = GURL(base::StringPrintf(
      "https://accounts.google.com/"
      "AccountChooser?Email=%s&continue=%s",
      std::string(percent_encoded_email.data(), percent_encoded_email.length())
          .c_str(),
      std::string(percent_encoded_account_url.data(),
                  percent_encoded_account_url.length())
          .c_str()));
  return google_util::AppendGoogleLocaleParam(
      change_password_url, g_browser_process->GetApplicationLocale());
}

void ChromePasswordProtectionService::HandleUserActionOnModalWarning(
    content::WebContents* web_contents,
    ReusedPasswordAccountType password_type,
    RequestOutcome outcome,
    LoginReputationClientResponse::VerdictType verdict_type,
    const std::string& verdict_token,
    WarningAction action) {
  const Origin origin = Origin::Create(web_contents->GetLastCommittedURL());
  int64_t navigation_id =
      GetNavigationIDFromPrefsByOrigin(profile_->GetPrefs(), origin);

  if (action == WarningAction::IGNORE_WARNING) {
    AddModelWarningBypasstoPref();
  }

  if (action == WarningAction::CHANGE_PASSWORD) {
    RecordAction(UserMetricsAction(
        "PasswordProtection.ModalWarning.ChangePasswordButtonClicked"));
    LogDialogMetricsOnChangePassword(web_contents, password_type, navigation_id,
                                     outcome, verdict_type, verdict_token);
    OpenChangePasswordUrl(web_contents, password_type);
  } else if (action == WarningAction::IGNORE_WARNING &&
             password_type.is_account_syncing()) {
    RecordAction(UserMetricsAction(
        "PasswordProtection.ModalWarning.IgnoreButtonClicked"));
    // No need to change state.
    MaybeLogPasswordReuseDialogInteraction(
        navigation_id, PasswordReuseDialogInteraction::WARNING_ACTION_IGNORED);
  } else if (action == WarningAction::CLOSE &&
             password_type.is_account_syncing()) {
    RecordAction(
        UserMetricsAction("PasswordProtection.ModalWarning.CloseWarning"));
    // No need to change state.
    MaybeLogPasswordReuseDialogInteraction(
        navigation_id, PasswordReuseDialogInteraction::WARNING_UI_IGNORED);
  }
  RemoveWarningRequestsByWebContents(web_contents);
  MaybeFinishCollectingThreatDetails(
      web_contents,
      /*did_proceed=*/action == WarningAction::CHANGE_PASSWORD);
}

void ChromePasswordProtectionService::LogDialogMetricsOnChangePassword(
    content::WebContents* web_contents,
    ReusedPasswordAccountType password_type,
    int64_t navigation_id,
    RequestOutcome outcome,
    LoginReputationClientResponse::VerdictType verdict_type,
    const std::string& verdict_token) {
  if (password_type.is_account_syncing() ||
      password_type.account_type() ==
          ReusedPasswordAccountType::SAVED_PASSWORD) {
    MaybeLogPasswordReuseDialogInteraction(
        navigation_id, PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN);
  } else {
    // |outcome| is only recorded as succeeded or response_already_cached.
    MaybeLogPasswordReuseLookupResultWithVerdict(
        web_contents, PasswordType::OTHER_GAIA_PASSWORD,
        outcome == RequestOutcome::SUCCEEDED
            ? PasswordReuseLookup::REQUEST_SUCCESS
            : PasswordReuseLookup::CACHE_HIT,
        GetVerdictToLogFromResponse(verdict_type), verdict_token);
  }
}

void ChromePasswordProtectionService::AddModelWarningBypasstoPref() {
  auto* metrics_collector =
      SafeBrowsingMetricsCollectorFactory::GetForProfile(profile_);
  if (metrics_collector) {
    metrics_collector->AddSafeBrowsingEventToPref(
        SafeBrowsingMetricsCollector::EventType::PASSWORD_REUSE_MODAL_BYPASS);
  }
}

void ChromePasswordProtectionService::OpenChangePasswordUrl(
    content::WebContents* web_contents,
    ReusedPasswordAccountType password_type) {
  if (password_type.account_type() ==
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE) {
    // Directly open enterprise change password page for enterprise password
    // reuses.
    OpenUrl(web_contents, GetEnterpriseChangePasswordURL(), content::Referrer(),
            /*in_new_tab=*/true);
    web_contents_with_unhandled_enterprise_reuses_.erase(web_contents);
  } else if (password_type.account_type() !=
             ReusedPasswordAccountType::SAVED_PASSWORD) {
    // Opens accounts.google.com in a new tab.
    OpenUrl(web_contents, GetDefaultChangePasswordURL(), content::Referrer(),
            /*in_new_tab=*/true);
  } else {
#if BUILDFLAG(FULL_SAFE_BROWSING)
    // Opens chrome://settings/passwords/check in a new tab.
    chrome::ShowPasswordCheck(chrome::FindBrowserWithWebContents(web_contents));
    password_manager::LogPasswordCheckReferrer(
        password_manager::PasswordCheckReferrer::kPhishGuardDialog);
#endif
  }
}

void ChromePasswordProtectionService::HandleUserActionOnPageInfo(
    content::WebContents* web_contents,
    ReusedPasswordAccountType password_type,
    WarningAction action) {
  GURL url = web_contents->GetLastCommittedURL();
  const Origin origin = Origin::Create(url);

  if (action == WarningAction::CHANGE_PASSWORD) {
    RecordAction(UserMetricsAction(
        "PasswordProtection.PageInfo.ChangePasswordButtonClicked"));
    OpenChangePasswordUrl(web_contents, password_type);
    return;
  }

  if (action == WarningAction::MARK_AS_LEGITIMATE) {
    RecordAction(
        UserMetricsAction("PasswordProtection.PageInfo.MarkSiteAsLegitimate"));
    // TODO(vakh): There's no good enum to report this dialog interaction.
    // This needs to be investigated.
    UpdateSecurityState(SB_THREAT_TYPE_SAFE, password_type, web_contents);
    if (password_type.account_type() ==
        ReusedPasswordAccountType::NON_GAIA_ENTERPRISE) {
      web_contents_with_unhandled_enterprise_reuses_.erase(web_contents);
    } else {
      DictionaryPrefUpdate update(
          profile_->GetPrefs(),
          prefs::kSafeBrowsingUnhandledGaiaPasswordReuses);
      update->RemoveKey(origin.Serialize());
    }

    // If the site is marked as legitimate and the phished password is
    // a saved password, remove the matching saved password credentials
    // from the compromised credentials table as the user has considered
    // the site safe.
    if (password_type.account_type() ==
        ReusedPasswordAccountType::SAVED_PASSWORD) {
      RemovePhishedSavedPasswordCredential(
          saved_passwords_matching_reused_credentials());
    }
    for (auto& observer : observer_list_)
      observer.OnMarkingSiteAsLegitimate(url);
    return;
  }

  NOTREACHED();
}

void ChromePasswordProtectionService::HandleResetPasswordOnInterstitial(
    content::WebContents* web_contents,
    WarningAction action) {
  RecordAction(
      UserMetricsAction("PasswordProtection.Interstitial.ResetPassword"));
  // Opens enterprise change password page in current tab for user to change
  // password.
  OpenUrl(web_contents, GetEnterpriseChangePasswordURL(),
          content::Referrer(web_contents->GetLastCommittedURL(),
                            network::mojom::ReferrerPolicy::kDefault),
          /*in_new_tab=*/false);
}

std::u16string ChromePasswordProtectionService::GetWarningDetailText(
    ReusedPasswordAccountType password_type,
    std::vector<size_t>* placeholder_offsets) const {
  DCHECK(password_type.account_type() == ReusedPasswordAccountType::GSUITE ||
         password_type.account_type() == ReusedPasswordAccountType::GMAIL ||
         password_type.account_type() ==
             ReusedPasswordAccountType::NON_GAIA_ENTERPRISE ||
         (password_type.account_type() ==
          ReusedPasswordAccountType::SAVED_PASSWORD));
  if (password_type.account_type() ==
      ReusedPasswordAccountType::NON_GAIA_ENTERPRISE) {
    return l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE);
  }

  if (password_type.account_type() ==
      ReusedPasswordAccountType::SAVED_PASSWORD) {
    return GetWarningDetailTextForSavedPasswords(placeholder_offsets);
  }

  bool enable_warning_for_non_sync_users = base::FeatureList::IsEnabled(
      safe_browsing::kPasswordProtectionForSignedInUsers);
  if (enable_warning_for_non_sync_users &&
      !password_type.is_account_syncing()) {
    return l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SIGNED_IN_NON_SYNC);
  }
  if (password_type.account_type() != ReusedPasswordAccountType::GSUITE) {
    return l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SYNC);
  }

  std::string org_name = GetOrganizationName(password_type);
  if (!org_name.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE_WITH_ORG_NAME,
        base::UTF8ToUTF16(org_name));
  }
  return l10n_util::GetStringUTF16(
      IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE);
}

std::vector<std::u16string>
ChromePasswordProtectionService::GetPlaceholdersForSavedPasswordWarningText()
    const {
  const std::vector<std::string>& matching_domains =
      saved_passwords_matching_domains();
  const std::list<std::string>& spoofed_domains = common_spoofed_domains();

  // Show most commonly spoofed domains first.
  // This looks through the top priority spoofed domains and then checks to see
  // if it's in the matching domains.
  std::vector<std::u16string> placeholders;
  for (auto priority_domain_iter = spoofed_domains.begin();
       priority_domain_iter != spoofed_domains.end(); ++priority_domain_iter) {
    std::string matching_domain;

    // Check if any of the matching domains is equal or a suffix to the current
    // priority domain.
    if (std::find_if(matching_domains.begin(), matching_domains.end(),
                     [priority_domain_iter,
                      &matching_domain](const std::string& domain) {
                       // Assigns the matching_domain to add into the priority
                       // placeholders. This value is only used if the return
                       // value of this function is true.
                       matching_domain = domain;
                       const base::StringPiece domainStringPiece(domain);
                       // Checks for two cases:
                       // 1. if the matching domain is equal to the current
                       // priority domain or
                       // 2. if "," + the current priority is a suffix of the
                       // matching domain The second case covers eTLD+1.
                       return (domain == *priority_domain_iter) ||
                              base::EndsWith(domainStringPiece,
                                             "." + *priority_domain_iter);
                     }) != matching_domains.end()) {
      placeholders.push_back(base::UTF8ToUTF16(matching_domain));
    }
  }

  // If there are less than 3 saved default domains, check the saved
  //  password domains to see if there are more that can be added to the
  //  warning text.
  int domains_idx = placeholders.size();
  for (size_t idx = 0; idx < matching_domains.size() && domains_idx < 3;
       idx++) {
    // Do not add duplicate domains if it was already in the default domains.
    if (std::find(placeholders.begin(), placeholders.end(),
                  base::UTF8ToUTF16(matching_domains[idx])) !=
        placeholders.end()) {
      continue;
    }
    placeholders.push_back(base::UTF8ToUTF16(matching_domains[idx]));
    domains_idx++;
  }
  return placeholders;
}

std::u16string
ChromePasswordProtectionService::GetWarningDetailTextForSavedPasswords(
    std::vector<size_t>* placeholder_offsets) const {
  std::vector<std::u16string> placeholders =
      GetPlaceholdersForSavedPasswordWarningText();
  // If showing the saved passwords domain experiment is not on or if there is
  // are no saved domains, default to original saved passwords reuse warning.
  return placeholders.empty()
             ? l10n_util::GetStringUTF16(
                   IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_SAVED)
             : GetWarningDetailTextToCheckSavedPasswords(placeholder_offsets);
}

std::u16string
ChromePasswordProtectionService::GetWarningDetailTextToCheckSavedPasswords(
    std::vector<size_t>* placeholder_offsets) const {
  std::vector<std::u16string> placeholders =
      GetPlaceholdersForSavedPasswordWarningText();
  if (placeholders.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_1_DOMAIN, placeholders,
        placeholder_offsets);
  } else if (placeholders.size() == 2) {
    return l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_2_DOMAIN, placeholders,
        placeholder_offsets);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_PAGE_INFO_CHECK_PASSWORD_DETAILS_SAVED_3_DOMAIN, placeholders,
        placeholder_offsets);
  }
}

std::string ChromePasswordProtectionService::GetOrganizationName(
    ReusedPasswordAccountType password_type) const {
  if (password_type.account_type() != ReusedPasswordAccountType::GSUITE) {
    return std::string();
  }

  std::string email =
      password_type.is_account_syncing()
          ? GetAccountInfo().email
          : GetSignedInNonSyncAccount(username_for_last_shown_warning()).email;
  return email.empty() ? std::string() : gaia::ExtractDomainName(email);
}

// Disabled on Android, because enterprise reporting extension is not supported.
#if !defined(OS_ANDROID)
void ChromePasswordProtectionService::MaybeReportPasswordReuseDetected(
    PasswordProtectionRequest* request,
    const std::string& username,
    PasswordType password_type,
    bool is_phishing_url) {
  auto reused_password_account_type =
      GetPasswordProtectionReusedPasswordAccountType(password_type, username);
  if (reused_password_account_type.account_type() ==
      ReusedPasswordAccountType::UNKNOWN) {
    return;
  }

  // When a PasswordFieldFocus event is sent, a PasswordProtectionRequest is
  // sent which means the password reuse type is unknown. We do not want to
  // report these events as PasswordReuse events. Also do not send reports for
  // Gmail accounts.
  bool can_log_password_reuse_event =
      (password_type == PasswordType::ENTERPRISE_PASSWORD ||
       reused_password_account_type.account_type() ==
           ReusedPasswordAccountType::GSUITE) &&
      (password_type != PasswordType::PASSWORD_TYPE_UNKNOWN);
  if (!IsIncognito() && can_log_password_reuse_event) {
    // User name should only be empty when MaybeStartPasswordFieldOnFocusRequest
    // is called.
    std::string username_or_email =
        username.empty() ? GetAccountInfo().email : username;
    auto* router =
        extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
            profile_);
    if (router) {
      PasswordProtectionRequestContent* request_content =
          static_cast<PasswordProtectionRequestContent*>(request);
      router->OnPolicySpecifiedPasswordReuseDetected(
          request_content->web_contents()->GetLastCommittedURL(),
          username_or_email, is_phishing_url);
    }
  }
}

void ChromePasswordProtectionService::ReportPasswordChanged() {
  if (!IsIncognito()) {
    auto* router =
        extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(
            profile_);
    if (router) {
      router->OnPolicySpecifiedPasswordChanged(GetAccountInfo().email);
    }
  }
}
#endif

bool ChromePasswordProtectionService::HasUnhandledEnterprisePasswordReuse(
    content::WebContents* web_contents) const {
  return web_contents_with_unhandled_enterprise_reuses_.find(web_contents) !=
         web_contents_with_unhandled_enterprise_reuses_.end();
}

void ChromePasswordProtectionService::OnWarningTriggerChanged() {
  const base::Value* pref_value = pref_change_registrar_->prefs()->Get(
      prefs::kPasswordProtectionWarningTrigger);
  // If password protection is not turned off, do nothing.
  if (static_cast<PasswordProtectionTrigger>(pref_value->GetInt()) !=
      PASSWORD_PROTECTION_OFF) {
    return;
  }

  // Clears captured enterprise password hashes or GSuite sync password hashes.
  scoped_refptr<password_manager::PasswordStore> password_store =
      GetProfilePasswordStore();

  password_store->ClearAllNonGmailPasswordHash();
  password_store->ClearAllEnterprisePasswordHash();
}

void ChromePasswordProtectionService::OnEnterprisePasswordUrlChanged() {
  GetProfilePasswordStore()->ScheduleEnterprisePasswordURLUpdate();
}

bool ChromePasswordProtectionService::CanShowInterstitial(
    ReusedPasswordAccountType password_type,
    const GURL& main_frame_url) {
  bool is_supported_password_type =
      password_type.account_type() == ReusedPasswordAccountType::GSUITE ||
      password_type.account_type() ==
          ReusedPasswordAccountType::NON_GAIA_ENTERPRISE;
  return IsInPasswordAlertMode(password_type) && is_supported_password_type &&
         !IsURLAllowlistedForPasswordEntry(main_frame_url);
}

void ChromePasswordProtectionService::SetLogPasswordCaptureTimer(
    const base::TimeDelta& delay) {
  // This will replace any pending timer.
  log_password_capture_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&ChromePasswordProtectionService::MaybeLogPasswordCapture,
                     base::Unretained(this), false));
}

void ChromePasswordProtectionService::MaybeLogPasswordCapture(bool did_log_in) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We skip this event and not set a timer if the profile is in incognito. When
  // the user logs in in the future, MaybeLogPasswordCapture() will be called
  // immediately then and will restart the timer.
  if (IsIncognito() || sync_password_hash_.empty())
    return;

  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile_);
  if (!user_event_service)
    return;

  std::unique_ptr<UserEventSpecifics> specifics = GetNewUserEventSpecifics();
  auto* const password_captured =
      specifics->mutable_gaia_password_captured_event();
  password_captured->set_event_trigger(
      did_log_in ? GaiaPasswordCaptured::USER_LOGGED_IN
                 : GaiaPasswordCaptured::EXPIRED_28D_TIMER);

  WebUIInfoSingleton::GetInstance()->AddToPGEvents(*specifics);
  user_event_service->RecordUserEvent(std::move(specifics));

  // Set a timer to log it again in 24-28 days. Spread it to avoid hammering the
  // backend with fixed cycle after this code lands in Stable.
  base::TimeDelta delay = base::TimeDelta::FromDays(
      (kPasswordCaptureEventLogFreqDaysMin +
       base::RandInt(0, kPasswordCaptureEventLogFreqDaysExtra)));
  SetLogPasswordCaptureTimer(delay);

  // Write the deadline to a pref to carry over restarts.
  SetDelayInPref(profile_->GetPrefs(),
                 prefs::kSafeBrowsingNextPasswordCaptureEventLogTime, delay);
}

void ChromePasswordProtectionService::UpdateSecurityState(
    SBThreatType threat_type,
    ReusedPasswordAccountType password_type,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const GURL url = web_contents->GetLastCommittedURL();
  if (!url.is_valid())
    return;

  const GURL url_with_empty_path = url.GetWithEmptyPath();
  if (threat_type == SB_THREAT_TYPE_SAFE) {
    ui_manager_->RemoveAllowlistUrlSet(url_with_empty_path, web_contents,
                                       /*from_pending_only=*/false);
    // Overrides cached verdicts.
    LoginReputationClientResponse verdict;
    GetCachedVerdict(url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                     password_type, &verdict);
    verdict.set_verdict_type(LoginReputationClientResponse::SAFE);
    verdict.set_cache_duration_sec(kOverrideVerdictCacheDurationSec);
    CacheVerdict(url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 password_type, verdict, base::Time::Now());
    return;
  }

  SBThreatType current_threat_type = SB_THREAT_TYPE_UNUSED;
  // If user already click-through interstitial warning, or if there's already
  // a dangerous security state showing, we'll override it.
  if (ui_manager_->IsUrlAllowlistedOrPendingForWebContents(
          url_with_empty_path, /*is_subresource=*/false,
          web_contents->GetController().GetLastCommittedEntry(), web_contents,
          /*allowlist_only=*/false, &current_threat_type)) {
    DCHECK_NE(SB_THREAT_TYPE_UNUSED, current_threat_type);
    if (current_threat_type == threat_type)
      return;
    // Resets previous threat type.
    ui_manager_->RemoveAllowlistUrlSet(url_with_empty_path, web_contents,
                                       /*from_pending_only=*/false);
  }
  ui_manager_->AddToAllowlistUrlSet(url_with_empty_path, web_contents,
                                    /*is_pending=*/true, threat_type);
}

void ChromePasswordProtectionService::FillReferrerChain(
    const GURL& event_url,
    SessionID event_tab_id,
    LoginReputationClientRequest::Frame* frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SafeBrowsingNavigationObserverManager::AttributionResult result =
      navigation_observer_manager_->IdentifyReferrerChainByEventURL(
          event_url, event_tab_id, kPasswordEventAttributionUserGestureLimit,
          frame->mutable_referrer_chain());
  size_t referrer_chain_length = frame->referrer_chain().size();
  UMA_HISTOGRAM_COUNTS_100(
      "SafeBrowsing.ReferrerURLChainSize.PasswordEventAttribution",
      referrer_chain_length);
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.ReferrerAttributionResult.PasswordEventAttribution", result,
      SafeBrowsingNavigationObserverManager::ATTRIBUTION_FAILURE_TYPE_MAX);

  // Determines how many recent navigation events to append to referrer chain.
  size_t recent_navigations_to_collect =
      profile_ ? SafeBrowsingNavigationObserverManager::
                     CountOfRecentNavigationsToAppend(*profile_, result)
               : 0u;
  navigation_observer_manager_->AppendRecentNavigations(
      recent_navigations_to_collect, frame->mutable_referrer_chain());
}

std::string ChromePasswordProtectionService::GetSyncPasswordHashFromPrefs() {
  if (!sync_password_hash_provider_for_testing_.is_null())
    return sync_password_hash_provider_for_testing_.Run();

  password_manager::HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(profile_->GetPrefs());
  base::Optional<password_manager::PasswordHashData> sync_hash_data =
      hash_password_manager.RetrievePasswordHash(GetAccountInfo().email,
                                                 /*is_gaia_password=*/true);
  return sync_hash_data ? base::NumberToString(sync_hash_data->hash)
                        : std::string();
}

PrefService* ChromePasswordProtectionService::GetPrefs() {
  return profile_->GetPrefs();
}

bool ChromePasswordProtectionService::IsSafeBrowsingEnabled() {
  return ::safe_browsing::IsSafeBrowsingEnabled(*GetPrefs());
}

bool ChromePasswordProtectionService::IsExtendedReporting() {
  return IsExtendedReportingEnabled(*GetPrefs());
}

bool ChromePasswordProtectionService::IsIncognito() {
  return profile_->IsOffTheRecord();
}

bool ChromePasswordProtectionService::IsInPasswordAlertMode(
    ReusedPasswordAccountType password_type) {
  return GetPasswordProtectionWarningTriggerPref(password_type) ==
         PASSWORD_REUSE;
}

bool ChromePasswordProtectionService::IsPingingEnabled(
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type) {
  if (!IsSafeBrowsingEnabled()) {
    return false;
  }
  bool extended_reporting_enabled = IsExtendedReporting();
  if (trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT) {
    if (password_type.account_type() ==
        ReusedPasswordAccountType::SAVED_PASSWORD) {
      return true;
    }

    // Only override policy if password protection is off for Gmail users.
    if (GetPasswordProtectionWarningTriggerPref(password_type) ==
        PASSWORD_PROTECTION_OFF) {
      return false;
    }
    // If the account type is UNKNOWN (i.e. AccountInfo fields could not be
    // retrieved from server), pings should be gated by SBER.
    if (password_type.account_type() == ReusedPasswordAccountType::UNKNOWN) {
      return extended_reporting_enabled;
    }

// Only saved password reuse warnings are shown on Android, so other types of
// password reuse events should be gated by extended reporting.
#if defined(OS_ANDROID)
    return extended_reporting_enabled;
#else
    return true;
#endif
  }

  return !IsIncognito() && extended_reporting_enabled;
}

RequestOutcome ChromePasswordProtectionService::GetPingNotSentReason(
    LoginReputationClientRequest::TriggerType trigger_type,
    const GURL& url,
    ReusedPasswordAccountType password_type) {
  DCHECK(!CanSendPing(trigger_type, url, password_type));
  if (IsInExcludedCountry()) {
    return RequestOutcome::EXCLUDED_COUNTRY;
  }
  if (!IsSafeBrowsingEnabled()) {
    return RequestOutcome::SAFE_BROWSING_DISABLED;
  }
  if (IsIncognito()) {
    return RequestOutcome::DISABLED_DUE_TO_INCOGNITO;
  }
  if (trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT &&
      password_type.account_type() !=
          ReusedPasswordAccountType::SAVED_PASSWORD &&
      GetPasswordProtectionWarningTriggerPref(password_type) ==
          PASSWORD_PROTECTION_OFF) {
    return RequestOutcome::TURNED_OFF_BY_ADMIN;
  }
  PrefService* prefs = profile_->GetPrefs();
  if (IsURLAllowlistedByPolicy(url, *prefs)) {
    return RequestOutcome::MATCHED_ENTERPRISE_ALLOWLIST;
  }
  if (MatchesPasswordProtectionChangePasswordURL(url, *prefs)) {
    return RequestOutcome::MATCHED_ENTERPRISE_CHANGE_PASSWORD_URL;
  }
  if (MatchesPasswordProtectionLoginURL(url, *prefs)) {
    return RequestOutcome::MATCHED_ENTERPRISE_LOGIN_URL;
  }
  if (IsInPasswordAlertMode(password_type)) {
    return RequestOutcome::PASSWORD_ALERT_MODE;
  }
  return RequestOutcome::DISABLED_DUE_TO_USER_POPULATION;
}

void ChromePasswordProtectionService::FillUserPopulation(
    LoginReputationClientRequest* request_proto) {
  *request_proto->mutable_population() = GetUserPopulation(profile_);
}

bool ChromePasswordProtectionService::IsPrimaryAccountSyncing() const {
  syncer::SyncService* sync =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  return sync && sync->IsSyncFeatureActive() && !sync->IsLocalSyncEnabled();
}

bool ChromePasswordProtectionService::IsPrimaryAccountSignedIn() const {
  return !GetAccountInfo().account_id.empty() &&
         !GetAccountInfo().hosted_domain.empty();
}

// TODO(bdea): Combine the next two methods.
bool ChromePasswordProtectionService::IsPrimaryAccountGmail() const {
  return GetAccountInfo().hosted_domain == kNoHostedDomainFound;
}

bool ChromePasswordProtectionService::IsOtherGaiaAccountGmail(
    const std::string& username) const {
  return GetSignedInNonSyncAccount(username).hosted_domain ==
         kNoHostedDomainFound;
}

AccountInfo ChromePasswordProtectionService::GetSignedInNonSyncAccount(
    const std::string& username) const {
  auto* identity_manager = IdentityManagerFactory::GetForProfileIfExists(
      profile_->GetOriginalProfile());

  if (!identity_manager)
    return AccountInfo();

  std::vector<CoreAccountInfo> signed_in_accounts =
      identity_manager->GetAccountsWithRefreshTokens();
  auto account_iterator =
      std::find_if(signed_in_accounts.begin(), signed_in_accounts.end(),
                   [username](const auto& account) {
                     return password_manager::AreUsernamesSame(
                         account.email,
                         /*is_username1_gaia_account=*/true, username,
                         /*is_username2_gaia_account=*/true);
                   });
  if (account_iterator == signed_in_accounts.end())
    return AccountInfo();

  return identity_manager
      ->FindExtendedAccountInfoForAccountWithRefreshToken(*account_iterator)
      .value_or(AccountInfo());
}

bool ChromePasswordProtectionService::IsInExcludedCountry() {
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (!variations_service)
    return false;
  return base::Contains(GetExcludedCountries(),
                        variations_service->GetStoredPermanentCountry());
}

PasswordReuseEvent::SyncAccountType
ChromePasswordProtectionService::GetSyncAccountType() const {
  const AccountInfo account_info = GetAccountInfo();
  if (!IsPrimaryAccountSignedIn()) {
    return PasswordReuseEvent::NOT_SIGNED_IN;
  }

  // For gmail or googlemail account, the hosted_domain will always be
  // kNoHostedDomainFound.
  return account_info.hosted_domain == kNoHostedDomainFound
             ? PasswordReuseEvent::GMAIL
             : PasswordReuseEvent::GSUITE;
}

void ChromePasswordProtectionService::
    RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
        bool all_history,
        const history::URLRows& deleted_rows) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DictionaryPrefUpdate unhandled_sync_password_reuses(
      profile_->GetPrefs(), prefs::kSafeBrowsingUnhandledGaiaPasswordReuses);
  if (all_history) {
    unhandled_sync_password_reuses->Clear();
    return;
  }

  for (const history::URLRow& row : deleted_rows) {
    if (!row.url().SchemeIsHTTPOrHTTPS())
      continue;
    unhandled_sync_password_reuses->RemoveKey(
        Origin::Create(row.url()).Serialize());
  }
}

bool ChromePasswordProtectionService::UserClickedThroughSBInterstitial(
    PasswordProtectionRequest* request) {
  PasswordProtectionRequestContent* request_content =
      static_cast<PasswordProtectionRequestContent*>(request);
  content::WebContents* web_contents = request_content->web_contents();
  SBThreatType current_threat_type;
  if (!ui_manager_->IsUrlAllowlistedOrPendingForWebContents(
          web_contents->GetLastCommittedURL().GetWithEmptyPath(),
          /*is_subresource=*/false,
          web_contents->GetController().GetLastCommittedEntry(), web_contents,
          /*allowlist_only=*/true, &current_threat_type)) {
    return false;
  }
  return current_threat_type == SB_THREAT_TYPE_URL_PHISHING ||
         current_threat_type == SB_THREAT_TYPE_URL_MALWARE ||
         current_threat_type == SB_THREAT_TYPE_URL_UNWANTED ||
         current_threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING ||
         current_threat_type == SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE;
}

AccountInfo ChromePasswordProtectionService::GetAccountInfo() const {
  auto* identity_manager = IdentityManagerFactory::GetForProfileIfExists(
      profile_->GetOriginalProfile());
  if (!identity_manager)
    return AccountInfo();

  base::Optional<AccountInfo> primary_account_info =
      identity_manager->FindExtendedAccountInfoForAccountWithRefreshToken(
          identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync));

  return primary_account_info.value_or(AccountInfo());
}

ChromePasswordProtectionService::ChromePasswordProtectionService(
    Profile* profile,
    scoped_refptr<SafeBrowsingUIManager> ui_manager,
    StringProvider sync_password_hash_provider,
    VerdictCacheManager* cache_manager)
    : PasswordProtectionService(nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                false,
                                nullptr,
                                /*try_token_fetch=*/false),
      ui_manager_(ui_manager),
      trigger_manager_(nullptr),
      profile_(profile),
      cache_manager_(cache_manager),
      sync_password_hash_provider_for_testing_(sync_password_hash_provider) {
  Init();
}

std::unique_ptr<PasswordProtectionNavigationThrottle>
MaybeCreateNavigationThrottle(content::NavigationHandle* navigation_handle) {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());
  ChromePasswordProtectionService* service =
      ChromePasswordProtectionService::GetPasswordProtectionService(profile);
  // |service| can be null in tests.
  return service ? service->MaybeCreateNavigationThrottle(navigation_handle)
                 : nullptr;
}

PasswordProtectionTrigger
ChromePasswordProtectionService::GetPasswordProtectionWarningTriggerPref(
    ReusedPasswordAccountType password_type) const {
  if (password_type.account_type() == ReusedPasswordAccountType::GMAIL ||
      (password_type.account_type() ==
       ReusedPasswordAccountType::SAVED_PASSWORD))
    return PHISHING_REUSE;

  bool is_policy_managed = profile_->GetPrefs()->HasPrefPath(
      prefs::kPasswordProtectionWarningTrigger);
  PasswordProtectionTrigger trigger_level =
      static_cast<PasswordProtectionTrigger>(profile_->GetPrefs()->GetInteger(
          prefs::kPasswordProtectionWarningTrigger));
  return is_policy_managed ? trigger_level : PHISHING_REUSE;
}

bool ChromePasswordProtectionService::IsURLAllowlistedForPasswordEntry(
    const GURL& url) const {
  if (!profile_)
    return false;

  PrefService* prefs = profile_->GetPrefs();
  return IsURLAllowlistedByPolicy(url, *prefs) ||
         MatchesPasswordProtectionChangePasswordURL(url, *prefs) ||
         MatchesPasswordProtectionLoginURL(url, *prefs);
}

void ChromePasswordProtectionService::PersistPhishedSavedPasswordCredential(
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials) {
  if (!profile_)
    return;

  for (const auto& credential : matching_reused_credentials) {
    password_manager::PasswordStore* password_store =
        GetStoreForReusedCredential(credential);
    // Password store can be null in tests.
    if (!password_store) {
      continue;
    }
    password_store->AddInsecureCredential(password_manager::InsecureCredential(
        credential.signon_realm, credential.username, base::Time::Now(),
        password_manager::InsecureType::kPhished,
        password_manager::IsMuted(false)));
  }
}

void ChromePasswordProtectionService::RemovePhishedSavedPasswordCredential(
    const std::vector<password_manager::MatchingReusedCredential>&
        matching_reused_credentials) {
  if (!profile_)
    return;

  for (const auto& credential : matching_reused_credentials) {
    password_manager::PasswordStore* password_store =
        GetStoreForReusedCredential(credential);
    // Password store can be null in tests.
    if (!password_store) {
      continue;
    }
    password_store->RemoveInsecureCredentials(
        credential.signon_realm, credential.username,
        password_manager::RemoveInsecureCredentialsReason::
            kMarkSiteAsLegitimate);
  }
}

#if defined(OS_ANDROID)
LoginReputationClientRequest::ReferringAppInfo
ChromePasswordProtectionService::GetReferringAppInfo(
    content::WebContents* web_contents) {
  return safe_browsing::GetReferringAppInfo(web_contents);
}
#endif

password_manager::PasswordStore*
ChromePasswordProtectionService::GetProfilePasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsIncognito
  // itself when it shouldn't access the PasswordStore.
  return PasswordStoreFactory::GetForProfile(profile_,
                                             ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

password_manager::PasswordStore*
ChromePasswordProtectionService::GetAccountPasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsIncognito
  // itself when it shouldn't access the PasswordStore.
  return AccountPasswordStoreFactory::GetForProfile(
             profile_, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

void ChromePasswordProtectionService::SanitizeReferrerChain(
    ReferrerChain* referrer_chain) {
  SafeBrowsingNavigationObserverManager::SanitizeReferrerChain(referrer_chain);
}

bool ChromePasswordProtectionService::CanSendSamplePing() {
  // Send a sample ping only 1% of the time.
  return IsExtendedReporting() && !IsIncognito() &&
         (bypass_probability_for_tests_ ||
          base::RandDouble() <= kProbabilityForSendingReportsFromSafeURLs);
}

// Stores |verdict| in |settings| based on its |trigger_type|, |url|,
// reused |password_type|, |verdict| and |receive_time|.
void ChromePasswordProtectionService::CacheVerdict(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type,
    const LoginReputationClientResponse& verdict,
    const base::Time& receive_time) {
  if (!CanGetReputationOfURL(url) || IsIncognito())
    return;
  cache_manager_->CachePhishGuardVerdict(trigger_type, password_type, verdict,
                                         receive_time);
}

// Looks up |settings| to find the cached verdict response. If verdict is not
// available or is expired, return VERDICT_TYPE_UNSPECIFIED. Can be called on
// any thread.
LoginReputationClientResponse::VerdictType
ChromePasswordProtectionService::GetCachedVerdict(
    const GURL& url,
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordAccountType password_type,
    LoginReputationClientResponse* out_response) {
  if (!url.is_valid() || !CanGetReputationOfURL(url))
    return LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED;
  return cache_manager_->GetCachedPhishGuardVerdict(
      url, trigger_type, password_type, out_response);
}

int ChromePasswordProtectionService::GetStoredVerdictCount(
    LoginReputationClientRequest::TriggerType trigger_type) {
  return cache_manager_->GetStoredPhishGuardVerdictCount(trigger_type);
}

#if BUILDFLAG(FULL_SAFE_BROWSING)
gfx::Size ChromePasswordProtectionService::GetCurrentContentAreaSize() const {
  return BrowserView::GetBrowserViewForBrowser(
             BrowserList::GetInstance()->GetLastActive())
      ->GetContentsSize();
}
#endif  // FULL_SAFE_BROWSING

password_manager::PasswordStore*
ChromePasswordProtectionService::GetStoreForReusedCredential(
    const password_manager::MatchingReusedCredential& reused_credential) {
  if (!profile_)
    return nullptr;
  return reused_credential.in_store ==
                 password_manager::PasswordForm::Store::kAccountStore
             ? GetAccountPasswordStore()
             : GetProfilePasswordStore();
}

}  // namespace safe_browsing
