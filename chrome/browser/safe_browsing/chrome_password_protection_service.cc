// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/google/core/common/google_util.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/common/utils.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/password_protection/password_protection_navigation_throttle.h"
#include "components/safe_browsing/password_protection/password_protection_request.h"
#include "components/safe_browsing/triggers/trigger_throttler.h"
#include "components/safe_browsing/web_ui/safe_browsing_ui.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync/user_events/user_event_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/url_util.h"

using content::BrowserThread;
using sync_pb::UserEventSpecifics;
using GaiaPasswordCaptured = UserEventSpecifics::GaiaPasswordCaptured;
using GaiaPasswordReuse = UserEventSpecifics::GaiaPasswordReuse;
using PasswordReuseDialogInteraction =
    GaiaPasswordReuse::PasswordReuseDialogInteraction;
using PasswordReuseLookup = GaiaPasswordReuse::PasswordReuseLookup;
using PasswordReuseEvent =
    safe_browsing::LoginReputationClientRequest::PasswordReuseEvent;
using SafeBrowsingStatus =
    GaiaPasswordReuse::PasswordReuseDetected::SafeBrowsingStatus;

namespace safe_browsing {

namespace {

// The number of user gestures we trace back for login event attribution.
const int kPasswordEventAttributionUserGestureLimit = 2;

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
      NOTREACHED() << "Unexpected response_verdict: " << response_verdict;
      return PasswordReuseLookup::VERDICT_UNSPECIFIED;
  }
  NOTREACHED() << "Unexpected response_verdict: " << response_verdict;
  return PasswordReuseLookup::VERDICT_UNSPECIFIED;
}

// Given a |web_contents|, returns the navigation id of its last committed
// navigation.
int64_t GetLastCommittedNavigationID(const content::WebContents* web_contents) {
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

int64_t GetFirstNavIdOrZero(PrefService* prefs) {
  const base::DictionaryValue* unhandled_sync_password_reuses =
      prefs->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  if (!unhandled_sync_password_reuses ||
      unhandled_sync_password_reuses->empty()) {
    return 0;
  }
  base::DictionaryValue::Iterator itr(*unhandled_sync_password_reuses);
  int64_t navigation_id;
  return base::StringToInt64(itr.value().GetString(), &navigation_id)
             ? navigation_id
             : 0;
}

int64_t GetNavigationIDFromPrefsByOrigin(PrefService* prefs,
                                         const Origin& origin) {
  const base::DictionaryValue* unhandled_sync_password_reuses =
      prefs->GetDictionary(prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
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
          sb_service->GetURLLoaderFactory(),
          HistoryServiceFactory::GetForProfile(
              profile,
              ServiceAccessType::EXPLICIT_ACCESS),
          HostContentSettingsMapFactory::GetForProfile(profile)),
      ui_manager_(sb_service->ui_manager()),
      trigger_manager_(sb_service->trigger_manager()),
      profile_(profile),
      navigation_observer_manager_(sb_service->navigation_observer_manager()),
      pref_change_registrar_(new PrefChangeRegistrar) {
  pref_change_registrar_->Init(profile_->GetPrefs());
  pref_change_registrar_->Add(
      password_manager::prefs::kPasswordHashDataList,
      base::Bind(&ChromePasswordProtectionService::CheckGaiaPasswordChange,
                 base::Unretained(this)));
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
}

ChromePasswordProtectionService::~ChromePasswordProtectionService() {
  if (content_settings())
    CleanUpExpiredVerdicts();

  if (pref_change_registrar_)
    pref_change_registrar_->RemoveAll();
}

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
bool ChromePasswordProtectionService::ShouldShowChangePasswordSettingUI(
    Profile* profile) {
  ChromePasswordProtectionService* service =
      ChromePasswordProtectionService::GetPasswordProtectionService(profile);
  if (!service)
    return false;
  auto* unhandled_sync_password_reuses = profile->GetPrefs()->GetDictionary(
      prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  return unhandled_sync_password_reuses &&
         !unhandled_sync_password_reuses->empty();
}

// static
bool ChromePasswordProtectionService::ShouldShowPasswordReusePageInfoBubble(
    content::WebContents* web_contents,
    ReusedPasswordType password_type) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  ChromePasswordProtectionService* service =
      ChromePasswordProtectionService::GetPasswordProtectionService(profile);

  // |service| could be null if safe browsing service is disabled.
  if (!service)
    return false;

  if (password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD)
    return service->HasUnhandledEnterprisePasswordReuse(web_contents);

  DCHECK_EQ(PasswordReuseEvent::SIGN_IN_PASSWORD, password_type);
  // Otherwise, checks if there's any unhandled sync password reuses matches
  // this origin.
  auto* unhandled_sync_password_reuses = profile->GetPrefs()->GetDictionary(
      prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  return unhandled_sync_password_reuses
             ? (unhandled_sync_password_reuses->FindKey(
                    Origin::Create(web_contents->GetLastCommittedURL())
                        .Serialize()) != nullptr)
             : false;
}

// static
bool ChromePasswordProtectionService::IsPasswordReuseProtectionConfigured(
    Profile* profile) {
  ChromePasswordProtectionService* service =
      ChromePasswordProtectionService::GetPasswordProtectionService(profile);
  return service &&
         service->GetPasswordProtectionWarningTriggerPref() == PASSWORD_REUSE;
}

const policy::BrowserPolicyConnector*
ChromePasswordProtectionService::GetBrowserPolicyConnector() const {
  return g_browser_process->browser_policy_connector();
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

void ChromePasswordProtectionService::ShowModalWarning(
    content::WebContents* web_contents,
    const std::string& verdict_token,
    ReusedPasswordType password_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(password_type == PasswordReuseEvent::SIGN_IN_PASSWORD ||
         password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD);
  // Don't show warning again if there is already a modal warning showing.
  if (IsModalWarningShowingInWebContents(web_contents))
    return;

  // Exit fullscreen if this |web_contents| is showing in fullscreen mode.
  if (web_contents->IsFullscreenForCurrentTab())
    web_contents->ExitFullscreen(true);

  ShowPasswordReuseModalWarningDialog(
      web_contents, this, password_type,
      base::BindOnce(&ChromePasswordProtectionService::OnUserAction,
                     base::Unretained(this), web_contents, password_type,
                     WarningUIType::MODAL_DIALOG));

  if (password_type == PasswordReuseEvent::SIGN_IN_PASSWORD)
    OnModalWarningShownForSignInPassword(web_contents, verdict_token);
  else
    OnModalWarningShownForEnterprisePassword(web_contents, verdict_token);
}

void ChromePasswordProtectionService::OnModalWarningShownForSignInPassword(
    content::WebContents* web_contents,
    const std::string& verdict_token) {
  LogWarningAction(WarningUIType::MODAL_DIALOG, WarningAction::SHOWN,
                   PasswordReuseEvent::SIGN_IN_PASSWORD, GetSyncAccountType());

  if (GetSyncAccountType() == PasswordReuseEvent::GSUITE) {
    OnPolicySpecifiedPasswordReuseDetected(web_contents->GetLastCommittedURL(),
                                           /*is_phishing_url=*/true);
  }

  if (!IsIncognito()) {
    DictionaryPrefUpdate update(
        profile_->GetPrefs(), prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
    // Since base::Value doesn't support int64_t type, we convert the navigation
    // ID to string format and store it in the preference dictionary.
    update->SetKey(
        Origin::Create(web_contents->GetLastCommittedURL()).Serialize(),
        base::Value(
            base::Int64ToString(GetLastCommittedNavigationID(web_contents))));
  }

  UpdateSecurityState(SB_THREAT_TYPE_SIGN_IN_PASSWORD_REUSE,
                      PasswordReuseEvent::SIGN_IN_PASSWORD, web_contents);

  // Starts preparing post-warning report.
  MaybeStartThreatDetailsCollection(web_contents, verdict_token,
                                    PasswordReuseEvent::SIGN_IN_PASSWORD);
}

void ChromePasswordProtectionService::OnModalWarningShownForEnterprisePassword(
    content::WebContents* web_contents,
    const std::string& verdict_token) {
  LogWarningAction(WarningUIType::MODAL_DIALOG, WarningAction::SHOWN,
                   PasswordReuseEvent::ENTERPRISE_PASSWORD,
                   GetSyncAccountType());
  web_contents_with_unhandled_enterprise_reuses_.insert(web_contents);
  UpdateSecurityState(SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE,
                      PasswordReuseEvent::ENTERPRISE_PASSWORD, web_contents);
  // Starts preparing post-warning report.
  MaybeStartThreatDetailsCollection(web_contents, verdict_token,
                                    PasswordReuseEvent::ENTERPRISE_PASSWORD);
}

void ChromePasswordProtectionService::ShowInterstitial(
    content::WebContents* web_contents,
    ReusedPasswordType password_type) {
  DCHECK(password_type == PasswordReuseEvent::SIGN_IN_PASSWORD ||
         password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD);
  // Exit fullscreen if this |web_contents| is showing in fullscreen mode.
  if (web_contents->IsFullscreenForCurrentTab())
    web_contents->ExitFullscreen(/*will_cause_resize=*/true);

  GURL trigger_url = web_contents->GetLastCommittedURL();
  content::OpenURLParams params(
      GURL(chrome::kChromeUIResetPasswordURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      /*is_renderer_initiated=*/false);
  params.uses_post = true;
  std::string post_data = base::NumberToString(password_type);
  params.post_data = network::ResourceRequestBody::CreateFromBytes(
      post_data.data(), post_data.size());
  web_contents->OpenURL(params);

  LogWarningAction(WarningUIType::INTERSTITIAL, WarningAction::SHOWN,
                   password_type, GetSyncAccountType());

  if (password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD ||
      GetSyncAccountType() == PasswordReuseEvent::GSUITE) {
    OnPolicySpecifiedPasswordReuseDetected(trigger_url,
                                           /*is_phishing_url=*/false);
  }
}

void ChromePasswordProtectionService::OnUserAction(
    content::WebContents* web_contents,
    ReusedPasswordType password_type,
    WarningUIType ui_type,
    WarningAction action) {
  LogWarningAction(ui_type, action, password_type, GetSyncAccountType());

  switch (ui_type) {
    case WarningUIType::PAGE_INFO:
      HandleUserActionOnPageInfo(web_contents, password_type, action);
      break;
    case WarningUIType::MODAL_DIALOG:
      HandleUserActionOnModalWarning(web_contents, password_type, action);
      break;
    case WarningUIType::CHROME_SETTINGS:
      HandleUserActionOnSettings(web_contents, action);
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
    ReusedPasswordType password_type) {
  // |trigger_manager_| can be null in test.
  if (!trigger_manager_)
    return;

  security_interstitials::UnsafeResource resource;
  resource.threat_type = password_type == PasswordReuseEvent::SIGN_IN_PASSWORD
                             ? SB_THREAT_TYPE_SIGN_IN_PASSWORD_REUSE
                             : SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE;
  resource.url = web_contents->GetLastCommittedURL();
  resource.web_contents_getter = resource.GetWebContentsGetter(
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
                                               *web_contents));
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
                                               *web_contents));
}

PrefService* ChromePasswordProtectionService::GetPrefs() {
  return profile_->GetPrefs();
}

bool ChromePasswordProtectionService::IsSafeBrowsingEnabled() {
  return GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled);
}

bool ChromePasswordProtectionService::IsExtendedReporting() {
  return IsExtendedReportingEnabled(*GetPrefs());
}

bool ChromePasswordProtectionService::IsIncognito() {
  return profile_->IsOffTheRecord();
}

bool ChromePasswordProtectionService::IsPingingEnabled(
    LoginReputationClientRequest::TriggerType trigger_type,
    ReusedPasswordType password_type,
    RequestOutcome* reason) {
  if (!IsSafeBrowsingEnabled()) {
    *reason = RequestOutcome::SAFE_BROWSING_DISABLED;
    return false;
  }

  if (trigger_type == LoginReputationClientRequest::PASSWORD_REUSE_EVENT) {
    if (password_type == PasswordReuseEvent::SAVED_PASSWORD)
      return true;

    if (password_type == PasswordReuseEvent::SIGN_IN_PASSWORD &&
        GetSyncAccountType() == PasswordReuseEvent::NOT_SIGNED_IN) {
      *reason = RequestOutcome::USER_NOT_SIGNED_IN;
      return false;
    }

    PasswordProtectionTrigger trigger_level =
        GetPasswordProtectionWarningTriggerPref();
    if (trigger_level == PASSWORD_REUSE) {
      *reason = RequestOutcome::PASSWORD_ALERT_MODE;
      return false;
    } else if (trigger_level == PASSWORD_PROTECTION_OFF) {
      *reason = RequestOutcome::TURNED_OFF_BY_ADMIN;
      return false;
    }
    return true;
  }

  // Password field on focus pinging is enabled for !incognito &&
  // extended_reporting.
  if (IsIncognito()) {
    *reason = RequestOutcome::DISABLED_DUE_TO_INCOGNITO;
    return false;
  }
  if (!IsExtendedReporting()) {
    *reason = RequestOutcome::DISABLED_DUE_TO_USER_POPULATION;
    return false;
  }
  return true;
}

bool ChromePasswordProtectionService::IsHistorySyncEnabled() {
  browser_sync::ProfileSyncService* sync =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  return sync && sync->IsSyncFeatureActive() && !sync->IsLocalSyncEnabled() &&
         sync->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES);
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseDetectedEvent(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!IsEventLoggingEnabled() && !WebUIInfoSingleton::HasListener())
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

  if (!IsEventLoggingEnabled() && !WebUIInfoSingleton::HasListener())
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

PasswordReuseEvent::SyncAccountType
ChromePasswordProtectionService::GetSyncAccountType() const {
  const AccountInfo account_info = GetAccountInfo();
  if (account_info.account_id.empty() || account_info.hosted_domain.empty()) {
    return PasswordReuseEvent::NOT_SIGNED_IN;
  }

  // For gmail or googlemail account, the hosted_domain will always be
  // kNoHostedDomainFound.
  return account_info.hosted_domain ==
                 std::string(AccountTrackerService::kNoHostedDomainFound)
             ? PasswordReuseEvent::GMAIL
             : PasswordReuseEvent::GSUITE;
}


void ChromePasswordProtectionService::MaybeLogPasswordReuseLookupResult(
    content::WebContents* web_contents,
    PasswordReuseLookup::LookupResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!IsEventLoggingEnabled() && !WebUIInfoSingleton::HasListener())
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
        PasswordReuseLookup::LookupResult result,
        PasswordReuseLookup::ReputationVerdict verdict,
        const std::string& verdict_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!IsEventLoggingEnabled() && !WebUIInfoSingleton::HasListener())
    return;

  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(profile_);
  if (!user_event_service)
    return;

  std::unique_ptr<UserEventSpecifics> specifics =
      GetUserEventSpecifics(web_contents);
  if (!specifics)
    return;

  PasswordReuseLookup* const reuse_lookup =
      specifics->mutable_gaia_password_reuse_event()->mutable_reuse_lookup();
  reuse_lookup->set_lookup_result(result);
  reuse_lookup->set_verdict(verdict);
  reuse_lookup->set_verdict_token(verdict_token);
  WebUIInfoSingleton::GetInstance()->AddToPGEvents(*specifics);
  user_event_service->RecordUserEvent(std::move(specifics));
}

void ChromePasswordProtectionService::MaybeLogPasswordReuseLookupEvent(
    content::WebContents* web_contents,
    RequestOutcome outcome,
    const LoginReputationClientResponse* response) {
  switch (outcome) {
    case RequestOutcome::MATCHED_WHITELIST:
      MaybeLogPasswordReuseLookupResult(web_contents,
                                        PasswordReuseLookup::WHITELIST_HIT);
      break;
    case RequestOutcome::RESPONSE_ALREADY_CACHED:
      MaybeLogPasswordReuseLookupResultWithVerdict(
          web_contents, PasswordReuseLookup::CACHE_HIT,
          GetVerdictToLogFromResponse(response->verdict_type()),
          response->verdict_token());
      break;
    case RequestOutcome::SUCCEEDED:
      MaybeLogPasswordReuseLookupResultWithVerdict(
          web_contents, PasswordReuseLookup::REQUEST_SUCCESS,
          GetVerdictToLogFromResponse(response->verdict_type()),
          response->verdict_token());
      break;
    case RequestOutcome::URL_NOT_VALID_FOR_REPUTATION_COMPUTING:
      MaybeLogPasswordReuseLookupResult(web_contents,
                                        PasswordReuseLookup::URL_UNSUPPORTED);
      break;
    case RequestOutcome::MATCHED_ENTERPRISE_WHITELIST:
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
  // If logging is disabled, we'll skip this event and not set a timer. When the
  // user logs in in the future, MaybeLogPasswordCapture() will be called
  // immediately then and will restart the timer.
  if (!IsEventLoggingEnabled() || sync_password_hash_.empty())
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
    ReusedPasswordType password_type,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const GURL url = web_contents->GetLastCommittedURL();
  if (!url.is_valid())
    return;

  const GURL url_with_empty_path = url.GetWithEmptyPath();
  if (threat_type == SB_THREAT_TYPE_SAFE) {
    ui_manager_->RemoveWhitelistUrlSet(url_with_empty_path, web_contents,
                                       /*from_pending_only=*/false);
    // Overrides cached verdicts.
    LoginReputationClientResponse verdict;
    GetCachedVerdict(url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                     password_type, &verdict);
    verdict.set_verdict_type(LoginReputationClientResponse::SAFE);
    verdict.set_cache_duration_sec(kOverrideVerdictCacheDurationSec);
    CacheVerdict(url, LoginReputationClientRequest::PASSWORD_REUSE_EVENT,
                 password_type, &verdict, base::Time::Now());
    return;
  }

  SBThreatType current_threat_type = SB_THREAT_TYPE_UNUSED;
  // If user already click-through interstitial warning, or if there's already
  // a dangerous security state showing, we'll override it.
  if (ui_manager_->IsUrlWhitelistedOrPendingForWebContents(
          url_with_empty_path, /*is_subresource=*/false,
          web_contents->GetController().GetLastCommittedEntry(), web_contents,
          /*whitelist_only=*/false, &current_threat_type)) {
    DCHECK_NE(SB_THREAT_TYPE_UNUSED, current_threat_type);
    if (current_threat_type == threat_type)
      return;
    // Resets previous threat type.
    ui_manager_->RemoveWhitelistUrlSet(url_with_empty_path, web_contents,
                                       /*from_pending_only=*/false);
  }
  ui_manager_->AddToWhitelistUrlSet(url_with_empty_path, web_contents,
                                    /*is_pending=*/true, threat_type);
}

void ChromePasswordProtectionService::
    RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
        bool all_history,
        const history::URLRows& deleted_rows) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DictionaryPrefUpdate unhandled_sync_password_reuses(
      profile_->GetPrefs(), prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
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

void ChromePasswordProtectionService::CheckGaiaPasswordChange() {
  std::string new_sync_password_hash = GetSyncPasswordHashFromPrefs();
  if (sync_password_hash_ != new_sync_password_hash) {
    sync_password_hash_ = new_sync_password_hash;
    OnGaiaPasswordChanged();
  }
}

void ChromePasswordProtectionService::OnGaiaPasswordChanged() {
  DictionaryPrefUpdate unhandled_sync_password_reuses(
      profile_->GetPrefs(), prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
  LogNumberOfReuseBeforeSyncPasswordChange(
      unhandled_sync_password_reuses->size());
  unhandled_sync_password_reuses->Clear();
  MaybeLogPasswordCapture(/*did_log_in=*/true);
  for (auto& observer : observer_list_)
    observer.OnGaiaPasswordChanged();
  if (GetSyncAccountType() == PasswordReuseEvent::GSUITE)
    OnPolicySpecifiedPasswordChanged();
}

bool ChromePasswordProtectionService::UserClickedThroughSBInterstitial(
    content::WebContents* web_contents) {
  SBThreatType current_threat_type;
  if (!ui_manager_->IsUrlWhitelistedOrPendingForWebContents(
          web_contents->GetLastCommittedURL().GetWithEmptyPath(),
          /*is_subresource=*/false,
          web_contents->GetController().GetLastCommittedEntry(), web_contents,
          /*whitelist_only=*/true, &current_threat_type)) {
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

  return identity_manager ? identity_manager->GetPrimaryAccountInfo()
                          : AccountInfo();
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
    ReusedPasswordType password_type,
    WarningAction action) {
  const Origin origin = Origin::Create(web_contents->GetLastCommittedURL());
  int64_t navigation_id =
      GetNavigationIDFromPrefsByOrigin(profile_->GetPrefs(), origin);
  if (action == WarningAction::CHANGE_PASSWORD) {
    MaybeLogPasswordReuseDialogInteraction(
        navigation_id, PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN);
    // Directly open enterprise change password page for enterprise password
    // reuses.
    if (password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD) {
      OpenUrl(web_contents, GetEnterpriseChangePasswordURL(),
              content::Referrer(),
              /*in_new_tab=*/true);
      web_contents_with_unhandled_enterprise_reuses_.erase(web_contents);
    } else {
      // Opens accounts.google.com in a new tab.
      OpenUrl(web_contents, GetDefaultChangePasswordURL(), content::Referrer(),
              /*in_new_tab=*/true);
    }
  } else if (action == WarningAction::IGNORE_WARNING) {
    // No need to change state.
    MaybeLogPasswordReuseDialogInteraction(
        navigation_id, PasswordReuseDialogInteraction::WARNING_ACTION_IGNORED);
  } else if (action == WarningAction::CLOSE) {
    // No need to change state.
    MaybeLogPasswordReuseDialogInteraction(
        navigation_id, PasswordReuseDialogInteraction::WARNING_UI_IGNORED);
  } else {
    NOTREACHED();
  }

  RemoveWarningRequestsByWebContents(web_contents);
  MaybeFinishCollectingThreatDetails(
      web_contents,
      /*did_proceed=*/action == WarningAction::CHANGE_PASSWORD);
}

void ChromePasswordProtectionService::HandleUserActionOnPageInfo(
    content::WebContents* web_contents,
    ReusedPasswordType password_type,
    WarningAction action) {
  GURL url = web_contents->GetLastCommittedURL();
  const Origin origin = Origin::Create(url);

  if (action == WarningAction::CHANGE_PASSWORD) {
    // Directly open enterprise change password page in a new tab for enterprise
    // reuses.
    if (password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD) {
      OpenUrl(web_contents, GetEnterpriseChangePasswordURL(),
              content::Referrer(),
              /*in_new_tab=*/true);
      web_contents_with_unhandled_enterprise_reuses_.erase(web_contents);
      return;
    }

    // For sync password reuse, open accounts.google.com page in a new tab.
    OpenUrl(web_contents, GetDefaultChangePasswordURL(), content::Referrer(),
            /*in_new_tab=*/true);
    return;
  }

  if (action == WarningAction::MARK_AS_LEGITIMATE) {
    // TODO(vakh): There's no good enum to report this dialog interaction.
    // This needs to be investigated.
    UpdateSecurityState(SB_THREAT_TYPE_SAFE, password_type, web_contents);
    if (password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD) {
      web_contents_with_unhandled_enterprise_reuses_.erase(web_contents);
    } else {
      DictionaryPrefUpdate update(
          profile_->GetPrefs(),
          prefs::kSafeBrowsingUnhandledSyncPasswordReuses);
      update->RemoveKey(origin.Serialize());
    }
    for (auto& observer : observer_list_)
      observer.OnMarkingSiteAsLegitimate(url);
    return;
  }

  NOTREACHED();
}

void ChromePasswordProtectionService::HandleUserActionOnSettings(
    content::WebContents* web_contents,
    WarningAction action) {
  DCHECK_EQ(WarningAction::CHANGE_PASSWORD, action);

  // Gets the first navigation_id from kSafeBrowsingUnhandledSyncPasswordReuses.
  // If there's only one unhandled reuse, getting the first is correct.
  // If there are more than one, we have no way to figure out which
  // event the user is responding to, so just pick the first one.
  MaybeLogPasswordReuseDialogInteraction(
      GetFirstNavIdOrZero(profile_->GetPrefs()),
      PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN_ON_SETTINGS);
  // Opens change password page in a new tab for user to change password.
  OpenUrl(web_contents, GetDefaultChangePasswordURL(),
          content::Referrer(web_contents->GetLastCommittedURL(),
                            network::mojom::ReferrerPolicy::kDefault),
          /*in_new_tab=*/true);
}

void ChromePasswordProtectionService::HandleResetPasswordOnInterstitial(
    content::WebContents* web_contents,
    WarningAction action) {
  // Opens enterprise change password page in current tab for user to change
  // password.
  OpenUrl(web_contents, GetEnterpriseChangePasswordURL(),
          content::Referrer(web_contents->GetLastCommittedURL(),
                            network::mojom::ReferrerPolicy::kDefault),
          /*in_new_tab=*/false);
}

ChromePasswordProtectionService::ChromePasswordProtectionService(
    Profile* profile,
    scoped_refptr<HostContentSettingsMap> content_setting_map,
    scoped_refptr<SafeBrowsingUIManager> ui_manager,
    StringProvider sync_password_hash_provider)
    : PasswordProtectionService(nullptr,
                                nullptr,
                                nullptr,
                                content_setting_map.get()),
      ui_manager_(ui_manager),
      trigger_manager_(nullptr),
      profile_(profile),
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
ChromePasswordProtectionService::GetPasswordProtectionWarningTriggerPref()
    const {
  bool is_policy_managed = profile_->GetPrefs()->HasPrefPath(
      prefs::kPasswordProtectionWarningTrigger);
  PasswordProtectionTrigger trigger_level =
      static_cast<PasswordProtectionTrigger>(profile_->GetPrefs()->GetInteger(
          prefs::kPasswordProtectionWarningTrigger));
  PasswordReuseEvent::SyncAccountType account_type = GetSyncAccountType();
  switch (account_type) {
    case (PasswordReuseEvent::GMAIL):
      return is_policy_managed ? trigger_level : PHISHING_REUSE;
    case (PasswordReuseEvent::NOT_SIGNED_IN):
    case (PasswordReuseEvent::GSUITE): {
      return is_policy_managed ? trigger_level : PASSWORD_PROTECTION_OFF;
    }
  }
  NOTREACHED();
  return PASSWORD_PROTECTION_OFF;
}

bool ChromePasswordProtectionService::IsURLWhitelistedForPasswordEntry(
    const GURL& url,
    RequestOutcome* reason) const {
  if (!profile_)
    return false;

  PrefService* prefs = profile_->GetPrefs();
  if (IsURLWhitelistedByPolicy(url, *prefs)) {
    *reason = RequestOutcome::MATCHED_ENTERPRISE_WHITELIST;
    return true;
  }

  // Checks if |url| matches the change password url configured in enterprise
  // policy.
  if (MatchesPasswordProtectionChangePasswordURL(url, *prefs)) {
    *reason = RequestOutcome::MATCHED_ENTERPRISE_CHANGE_PASSWORD_URL;
    return true;
  }

  // Checks if |url| matches any login url configured in enterprise policy.
  if (MatchesPasswordProtectionLoginURL(url, *prefs)) {
    *reason = RequestOutcome::MATCHED_ENTERPRISE_LOGIN_URL;
    return true;
  }

  return false;
}

base::string16 ChromePasswordProtectionService::GetWarningDetailText(
    ReusedPasswordType password_type) const {
  DCHECK(password_type == PasswordReuseEvent::SIGN_IN_PASSWORD ||
         password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD);
  if (password_type == PasswordReuseEvent::ENTERPRISE_PASSWORD) {
    return l10n_util::GetStringUTF16(
        IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS_ENTERPRISE);
  }

  if (GetSyncAccountType() !=
      safe_browsing::LoginReputationClientRequest::PasswordReuseEvent::GSUITE) {
    return l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS);
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

std::string ChromePasswordProtectionService::GetOrganizationName(
    ReusedPasswordType password_type) const {
  if (GetSyncAccountType() != PasswordReuseEvent::GSUITE ||
      password_type != PasswordReuseEvent::SIGN_IN_PASSWORD) {
    return std::string();
  }

  std::string email = GetAccountInfo().email;
  return email.empty() ? std::string() : gaia::ExtractDomainName(email);
}

void ChromePasswordProtectionService::OnPolicySpecifiedPasswordReuseDetected(
    const GURL& url,
    bool is_phishing_url) {
  if (!IsIncognito()) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnPolicySpecifiedPasswordReuseDetected(url, GetAccountInfo().email,
                                                 is_phishing_url);
  }
}

void ChromePasswordProtectionService::OnPolicySpecifiedPasswordChanged() {
  if (!IsIncognito()) {
    extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnPolicySpecifiedPasswordChanged(GetAccountInfo().email);
  }
}

bool ChromePasswordProtectionService::HasUnhandledEnterprisePasswordReuse(
    content::WebContents* web_contents) const {
  return web_contents_with_unhandled_enterprise_reuses_.find(web_contents) !=
         web_contents_with_unhandled_enterprise_reuses_.end();
}

void ChromePasswordProtectionService::OnWarningTriggerChanged() {
  if (GetPasswordProtectionWarningTriggerPref() != PASSWORD_PROTECTION_OFF)
    return;

  // Clears captured enterprise password hashes or GSuite sync password hashes.
  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(profile_,
                                          ServiceAccessType::EXPLICIT_ACCESS);

  if (GetSyncAccountType() == PasswordReuseEvent::GSUITE) {
    password_store->ClearGaiaPasswordHash(GetAccountInfo().email);
  }
  password_store->ClearAllEnterprisePasswordHash();
}

void ChromePasswordProtectionService::OnEnterprisePasswordUrlChanged() {
  PasswordStoreFactory::GetForProfile(profile_,
                                      ServiceAccessType::EXPLICIT_ACCESS)
      ->ScheduleEnterprisePasswordURLUpdate();
}

bool ChromePasswordProtectionService::CanShowInterstitial(
    RequestOutcome reason,
    ReusedPasswordType password_type,
    const GURL& main_frame_url) {
  // If it's not password alert mode, no need to log any metric.
  if (reason != RequestOutcome::PASSWORD_ALERT_MODE ||
      (password_type != PasswordReuseEvent::SIGN_IN_PASSWORD &&
       password_type != PasswordReuseEvent::ENTERPRISE_PASSWORD)) {
    return false;
  }

  if (!IsURLWhitelistedForPasswordEntry(main_frame_url, &reason))
    reason = RequestOutcome::SUCCEEDED;
  LogPasswordAlertModeOutcome(reason, password_type);
  return reason == RequestOutcome::SUCCEEDED;
}

bool ChromePasswordProtectionService::IsUnderAdvancedProtection() {
  return AdvancedProtectionStatusManager::IsUnderAdvancedProtection(profile_);
}

gfx::Size ChromePasswordProtectionService::GetCurrentContentAreaSize() const {
  return BrowserView::GetBrowserViewForBrowser(
             BrowserList::GetInstance()->GetLastActive())
      ->GetContentsSize();
}

}  // namespace safe_browsing
