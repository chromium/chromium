// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_

#include <map>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/security_events/security_event_recorder.h"
#include "chrome/browser/security_events/security_event_recorder_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_manager.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#include "components/safe_browsing/content/browser/triggers/trigger_manager.h"
#include "components/safe_browsing/core/browser/user_population.h"
#include "components/sessions/core/session_id.h"
#include "components/sync/protocol/gaia_password_reuse.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "ui/base/buildflags.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/password_manager/android/password_checkup_launcher_helper.h"
#endif

struct AccountInfo;
class PrefChangeRegistrar;
class PrefService;
class PrefChangeRegistrar;
class Profile;

namespace content {
class WebContents;
}

namespace safe_browsing {

class SafeBrowsingService;
class SafeBrowsingUIManager;
class ChromePasswordProtectionService;
class VerdictCacheManager;

using OnWarningDone = base::OnceCallback<void(WarningAction)>;
using StringProvider = base::RepeatingCallback<std::string()>;
using password_manager::metrics_util::PasswordType;
using url::Origin;

#if !BUILDFLAG(IS_ANDROID)
// Shows the desktop platforms specific password reuse modal dialog.
// Implemented in password_reuse_modal_warning_dialog.
void ShowPasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    ReusedPasswordAccountType password_type,
    OnWarningDone done_callback);
#endif

// Called by ChromeContentBrowserClient to create a
// PasswordProtectionCommitDeferringCondition if appropriate.
std::unique_ptr<PasswordProtectionCommitDeferringCondition>
MaybeCreateCommitDeferringCondition(
    content::NavigationHandle& navigation_handle);

// ChromePasswordProtectionService extends PasswordProtectionService by adding
// access to SafeBrowsingNaivigationObserverManager and Profile.
class ChromePasswordProtectionService : public PasswordProtectionService,
                                        public KeyedService {
 public:
  using ChangePhishedCredentialsCallback = base::RepeatingCallback<void(
      password_manager::PasswordStoreInterface*,
      const password_manager::MatchingReusedCredential&)>;
  // Observer is used to coordinate password protection UIs (e.g. modal warning,
  // change password card, etc) in reaction to user events.
  class Observer {
   public:
    // Called when user completes the GAIA password reset.
    virtual void OnGaiaPasswordChanged() = 0;

    // Called when user marks the site as legitimate.
    virtual void OnMarkingSiteAsLegitimate(const GURL& url) = 0;

    // Only to be used by tests. Subclasses must override to manually call the
    // respective button click handler.
    virtual void InvokeActionForTesting(WarningAction action) = 0;

    // Only to be used by tests.
    virtual WarningUIType GetObserverType() = 0;

   protected:
    virtual ~Observer() = default;
  };

  ChromePasswordProtectionService(SafeBrowsingService* sb_service,
                                  Profile* profile);

  ChromePasswordProtectionService(const ChromePasswordProtectionService&) =
      delete;
  ChromePasswordProtectionService& operator=(
      const ChromePasswordProtectionService&) = delete;

  ~ChromePasswordProtectionService() override;

  static ChromePasswordProtectionService* GetPasswordProtectionService(
      Profile* profile);

  // Called by SecurityStateTabHelper to determine if page info bubble should
  // show password reuse warning.
  static bool ShouldShowPasswordReusePageInfoBubble(
      content::WebContents* web_contents,
      PasswordType password_type);

  void ShowModalWarning(PasswordProtectionRequest* request,
                        LoginReputationClientResponse::VerdictType verdict_type,
                        const std::string& verdict_token,
                        ReusedPasswordAccountType password_type) override;

  void ShowInterstitial(content::WebContents* web_contents,
                        ReusedPasswordAccountType password_type) override;

  // Called when user interacts with password protection UIs.
  void OnUserAction(content::WebContents* web_contents,
                    ReusedPasswordAccountType password_type,
                    RequestOutcome outcome,
                    LoginReputationClientResponse::VerdictType verdict_type,
                    const std::string& verdict_token,
                    WarningUIType ui_type,
                    WarningAction action);

  // Called during the construction of Observer subclass.
  virtual void AddObserver(Observer* observer);

  // Called during the destruction of the observer subclass.
  virtual void RemoveObserver(Observer* observer);

  // Starts collecting threat details if user has extended reporting enabled and
  // is not in incognito mode.
  void MaybeStartThreatDetailsCollection(
      content::WebContents* web_contents,
      const std::string& token,
      ReusedPasswordAccountType password_type);

  // Sends threat details if user has extended reporting enabled and is not in
  // incognito mode.
  void MaybeFinishCollectingThreatDetails(content::WebContents* web_contents,
                                          bool did_proceed);

  // Check if Gaia password hash has changed. If it is changed, it will call
  // |OnGaiaPasswordChanged|. |username| is used to get the appropriate account
  // to check if the account is a Gmail account as no reporting is done for
  // those accounts. This method is only called if there was already an existing
  // password hash in the hash password manager reused password.
  void CheckGaiaPasswordChangeForAllSignedInUsers(const std::string& username);

  // Called when user's GAIA password changed. |username| is used to get
  // the account the password is associated with. |is_other_gaia_password|
  // specifies whether the account is syncing or not syncing (content area).
  void OnGaiaPasswordChanged(const std::string& username,
                             bool is_other_gaia_password);

  // Gets the enterprise change password URL if specified in policy,
  // otherwise gets the default GAIA change password URL.
  GURL GetEnterpriseChangePasswordURL() const;

  // Gets the GAIA change password URL based on |account_info_|.
  GURL GetDefaultChangePasswordURL() const;

  // Gets the detailed warning text that should show in the modal warning dialog
  // and page info bubble.
  std::u16string GetWarningDetailText(
      ReusedPasswordAccountType password_type) const;

  // If password protection trigger is configured via enterprise policy, gets
  // the name of the organization that owns the enterprise policy. Otherwise,
  // returns an empty string.
  std::string GetOrganizationName(
      ReusedPasswordAccountType password_type) const;

// The following functions are disabled on Android, because enterprise reporting
// extension is not supported.
#if !BUILDFLAG(IS_ANDROID)
  // If the browser is not incognito and the user is reusing their enterprise
  // password or is a GSuite user, triggers
  // safeBrowsingPrivate.OnPolicySpecifiedPasswordReuseDetected.
  // |username| can be an email address or a username for a non-GAIA or
  // saved-password reuse. No validation has been done on it.
  void MaybeReportPasswordReuseDetected(const GURL& main_frame_url,
                                        const std::string& username,
                                        PasswordType password_type,
                                        bool is_phishing_url,
                                        bool warning_shown) override;

  // Triggers "safeBrowsingPrivate.OnPolicySpecifiedPasswordChanged" API.
  void ReportPasswordChanged() override;
#endif

  // Returns true if there's any enterprise password reuses unhandled in
  // |web_contents|. "Unhandled" is defined as user hasn't clicked on
  // "Change Password" button in modal warning dialog.
  bool HasUnhandledEnterprisePasswordReuse(
      content::WebContents* web_contents) const;

  // If user has clicked through any Safe Browsing interstitial on |request|'s
  // web contents.
  bool UserClickedThroughSBInterstitial(
      PasswordProtectionRequest* request) override;

  // If |prefs::kPasswordProtectionWarningTrigger| is not managed by enterprise
  // policy, this function should always return PHISHING_REUSE. Otherwise,
  // returns the specified pref value adjusted for the given username's account
  // type.
  PasswordProtectionTrigger GetPasswordProtectionWarningTriggerPref(
      ReusedPasswordAccountType password_type) const override;

  // If |url| matches Safe Browsing allowlist domains, password protection
  // change password URL, or password protection login URLs in the enterprise
  // policy.
  bool IsURLAllowlistedForPasswordEntry(const GURL& url) const override;

  // Persist the phished saved password credential in the "compromised
  // credentials" table. Calls the password store to add a row for each
  // MatchingReusedCredential where the phished saved password is used on.
  void PersistPhishedSavedPasswordCredential(
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials) override;

  // Remove all rows of the phished saved password credential in the
  // "compromised credentials" table. Calls the password store to remove a row
  // for each MatchingReusedCredential where the phished saved password is used
  // on.
  void RemovePhishedSavedPasswordCredential(
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials) override;

#if BUILDFLAG(IS_ANDROID)
  LoginReputationClientRequest::ReferringAppInfo GetReferringAppInfo(
      content::WebContents* web_contents) override;
#endif
  // Returns the PasswordReuseManager associated with this instance.
  password_manager::PasswordReuseManager* GetPasswordReuseManager() const;

  // Returns the profile PasswordStoreInterface associated with this instance.
  password_manager::PasswordStoreInterface* GetProfilePasswordStore() const;

  // Returns the GAIA-account-scoped PasswordStoreInterface associated with this
  // instance. The account password store contains passwords stored in the
  // account and is accessible only when the user is signed in and non syncing.
  password_manager::PasswordStoreInterface* GetAccountPasswordStore() const;

  // Gets the type of sync account associated with current profile or
  // |NOT_SIGNED_IN|.
  LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType
  GetSyncAccountType() const override;
  safe_browsing::LoginReputationClientRequest::UrlDisplayExperiment
  GetUrlDisplayExperiment() const override;

  // Stores |verdict| in the cache based on its |trigger_type|, |url|,
  // reused |password_type|, |verdict| and |receive_time|.
  void CacheVerdict(const GURL& url,
                    LoginReputationClientRequest::TriggerType trigger_type,
                    ReusedPasswordAccountType password_type,
                    const LoginReputationClientResponse& verdict,
                    const base::Time& receive_time) override;

  // Returns the number of saved verdicts for the given |trigger_type|.
  int GetStoredVerdictCount(
      LoginReputationClientRequest::TriggerType trigger_type) override;

  // Looks up the cached verdict response. If verdict is not available or is
  // expired, return VERDICT_TYPE_UNSPECIFIED. Can be called on any thread.
  LoginReputationClientResponse::VerdictType GetCachedVerdict(
      const GURL& url,
      LoginReputationClientRequest::TriggerType trigger_type,
      ReusedPasswordAccountType password_type,
      LoginReputationClientResponse* out_response) override;

  // Sanitize referrer chain by only keeping origin information of all URLs.
  void SanitizeReferrerChain(ReferrerChain* referrer_chain) override;

  bool CanSendSamplePing() override;
#if defined(UNIT_TEST)
  void set_bypass_probability_for_tests(bool bypass_probability_for_tests) {
    bypass_probability_for_tests_ = bypass_probability_for_tests;
  }
#endif
  // Gets |account_info_| based on |profile_|.
  AccountInfo GetAccountInfo() const override;

  // Gets the UserPopulation value for this profile.
  ChromeUserPopulation::UserPopulation GetUserPopulationPref() const override;

  // KeyedService:
  // Called before the actual deletion of the object.
  void Shutdown() override;

 protected:
  // PasswordProtectionService overrides.

  // Obtains referrer chain of |event_url| and |event_tab_id| and add this
  // info into |frame|.
  void FillReferrerChain(const GURL& event_url,
                         SessionID event_tab_id,
                         LoginReputationClientRequest::Frame* frame) override;

  bool IsExtendedReporting() override;

  bool IsIncognito() override;

  bool IsInPasswordAlertMode(ReusedPasswordAccountType password_type) override;

  // Checks if pinging should be enabled based on the |trigger_type|,
  // |password_type|.
  bool IsPingingEnabled(LoginReputationClientRequest::TriggerType trigger_type,
                        ReusedPasswordAccountType password_type) override;

  // Populates the ChromeUserPopulation in |request_proto|.
  void FillUserPopulation(const GURL& main_frame_url,
                          LoginReputationClientRequest* request_proto) override;

  // If primary account is syncing history.
  bool IsPrimaryAccountSyncingHistory() const override;

  // If primary account is signed in.
  bool IsPrimaryAccountSignedIn() const override;

  // Checks whether the account associated with |username| is a Gmail account.
  bool IsAccountGmail(const std::string& username) const override;

  // Gets the AccountInfo for the account corresponding to |username| from the
  // list of signed-in users.
  AccountInfo GetAccountInfoForUsername(
      const std::string& username) const override;

  // If Safe browsing endpoint is not enabled in the country.
  bool IsInExcludedCountry() override;

  void MaybeLogPasswordReuseDetectedEvent(
      content::WebContents* web_contents) override;

  void MaybeLogPasswordReuseLookupEvent(
      content::WebContents* web_contents,
      RequestOutcome outcome,
      PasswordType password_type,
      const LoginReputationClientResponse* response) override;

  void HandleUserActionOnModalWarning(
      content::WebContents* web_contents,
      ReusedPasswordAccountType password_type,
      RequestOutcome outcome,
      LoginReputationClientResponse::VerdictType verdict_type,
      const std::string& verdict_token,
      WarningAction action);

  void HandleUserActionOnPageInfo(content::WebContents* web_contents,
                                  ReusedPasswordAccountType password_type,
                                  WarningAction action);

  void HandleResetPasswordOnInterstitial(content::WebContents* web_contents,
                                         WarningAction action);

  // Determines if we should show chrome://reset-password interstitial based on
  // the reused |password_type| and the |main_frame_url|.
  bool CanShowInterstitial(ReusedPasswordAccountType password_type,
                           const GURL& main_frame_url) override;

  // Updates security state for the current |web_contents| based on
  // |threat_type| and reused |password_type|, such that page info bubble will
  // show appropriate status when user clicks on the security chip.
  void UpdateSecurityState(SBThreatType threat_type,
                           ReusedPasswordAccountType password_type,
                           content::WebContents* web_contents) override;

  void RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
      bool all_history,
      const history::URLRows& deleted_rows) override;
  // Returns base-10 string representation of the uint64t hash.

  std::string GetSyncPasswordHashFromPrefs();

  void SetGaiaPasswordHashForTesting(const std::string& new_password_hash) {
    sync_password_hash_ = new_password_hash;
  }

  RequestOutcome GetPingNotSentReason(
      LoginReputationClientRequest::TriggerType trigger_type,
      const GURL& url,
      ReusedPasswordAccountType password_type) override;

  // Unit tests
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyUserPopulationForPasswordOnFocusPing);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyUserPopulationForSyncPasswordEntryPing);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyUserPopulationForSavedPasswordEntryPing);
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyPasswordReuseUserEventNotRecordedDueToIncognito);
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyPasswordReuseUserEventRecordedForOtherGaiaPassword);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordReuseDetectedUserEventRecorded);
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyPasswordReuseLookupEventNotRecordedFeatureNotEnabled);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordReuseLookupUserEventRecorded);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyGetSyncAccountType);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyUpdateSecurityState);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyGetChangePasswordURL);
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyUnhandledSyncPasswordReuseUponClearHistoryDeletion);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyCanShowInterstitial);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyCanShowInterstitial);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifySendsPingForAboutBlank);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventScheduledOnStartup);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventScheduledFromPref);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventReschedules);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventRecorded);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordReuseDetectedSecurityEventRecorded);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPersistPhishedSavedPasswordCredential);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyGetPingNotSentReason);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPageLoadToken);
  // Browser tests
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceBrowserTest,
                           VerifyCheckGaiaPasswordChange);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceBrowserTest,
                           OnEnterpriseTriggerOff);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceBrowserTest,
                           OnEnterpriseTriggerOffGSuite);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceBrowserTest,
                           VerifyIsInExcludedCountry);

 private:
  friend class MockChromePasswordProtectionService;
  friend class ChromePasswordProtectionServiceBrowserTest;
  friend class SecurityStateTabHelperTest;
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyOnPolicySpecifiedPasswordReuseDetectedEventForPhishingReuse);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyGetWarningDetailTextSavedDomains);

  // Gets prefs associated with |profile_|.
  PrefService* GetPrefs() const;

  // Returns whether the profile is valid and has safe browsing service enabled.
  bool IsSafeBrowsingEnabled();

  void MaybeLogPasswordReuseLookupResult(
      content::WebContents* web_contents,
      sync_pb::GaiaPasswordReuse::PasswordReuseLookup::LookupResult result);

  void MaybeLogPasswordReuseLookupResultWithVerdict(
      content::WebContents* web_contents,
      PasswordType password_type,
      sync_pb::GaiaPasswordReuse::PasswordReuseLookup::LookupResult result,
      sync_pb::GaiaPasswordReuse::PasswordReuseLookup::ReputationVerdict
          verdict,
      const std::string& verdict_token);

  void MaybeLogPasswordReuseDialogInteraction(
      int64_t navigation_id,
      sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction::
          InteractionResult interaction_result);

  void OnModalWarningShownForSavedPassword(
      content::WebContents* web_contents,
      ReusedPasswordAccountType password_type,
      const std::string& verdict_token);

  void OnModalWarningShownForGaiaPassword(
      content::WebContents* web_contents,
      ReusedPasswordAccountType password_type,
      const std::string& verdict_token);

  void OnModalWarningShownForEnterprisePassword(
      content::WebContents* web_contents,
      ReusedPasswordAccountType password_type,
      const std::string& verdict_token);

  // If enterprise admin turns off password protection, removes all captured
  // enterprise password hashes.
  void OnWarningTriggerChanged();

  // Informs PasswordReuseDetector that enterprise password URLs (login URL or
  // change password URL) have been changed.
  void OnEnterprisePasswordUrlChanged();

  // Log that we captured the password, either due to log-in or by timer.
  // This also sets the reoccuring timer.
  void MaybeLogPasswordCapture(bool did_log_in);
  void SetLogPasswordCaptureTimer(const base::TimeDelta& delay);

  // Open the page where the user can check their saved passwords
  // or change their phished credential, depending on the the |password_type|.
  void OpenPasswordCheck(content::WebContents* web_contents,
                         ReusedPasswordAccountType password_type);

  // Log user dialog interaction when the user clicks on the "Change Password"
  // or "Check Passwords" button.
  void LogDialogMetricsOnChangePassword(
      content::WebContents* web_contents,
      ReusedPasswordAccountType password_type,
      int64_t navigation_id,
      RequestOutcome outcome,
      LoginReputationClientResponse::VerdictType verdict_type,
      const std::string& verdict_token);

  // Add the bypass event to pref when the user ignore the modal warning.
  void AddModelWarningBypasstoPref();

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Get the content area size of current browsing window.
  gfx::Size GetCurrentContentAreaSize() const override;
#endif

  // Constructors used for tests only.
  ChromePasswordProtectionService(
      Profile* profile,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      StringProvider sync_password_hash_provider,
      VerdictCacheManager* cache_manager,
      ChangePhishedCredentialsCallback add_phished_credentials,
      ChangePhishedCredentialsCallback remove_phished_credentials);
#if BUILDFLAG(IS_ANDROID)
  ChromePasswordProtectionService(
      Profile* profile,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      StringProvider sync_password_hash_provider,
      VerdictCacheManager* cache_manager,
      ChangePhishedCredentialsCallback add_phished_credentials,
      ChangePhishedCredentialsCallback remove_phished_credentials,
      std::unique_ptr<PasswordCheckupLauncherHelper> checkup_launcher);
#endif

  // Code shared by both ctors.
  void Init();

  password_manager::PasswordStoreInterface* GetStoreForReusedCredential(
      const password_manager::MatchingReusedCredential& reused_credential);

  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  raw_ptr<TriggerManager, DanglingUntriaged> trigger_manager_;
  // Profile associated with this instance.
  raw_ptr<Profile> profile_;
  // Current sync password hash.
  std::string sync_password_hash_;
  base::ObserverList<Observer>::Unchecked observer_list_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::set<raw_ptr<content::WebContents, SetExperimental>>
      web_contents_with_unhandled_enterprise_reuses_;

  // Subscription for state changes. When the callback is notified, it means
  // HashPasswordManager password data list has changed.
  base::CallbackListSubscription hash_password_manager_subscription_;

  // Reference to the current profile's VerdictCacheManager. This is unowned.
  raw_ptr<VerdictCacheManager> cache_manager_;

  // Schedules the next time to log the PasswordCaptured event.
  base::OneShotTimer log_password_capture_timer_;

  // Calls `password_manager::AddPhishedCredentials`. Used to facilitate
  // testing.
  ChangePhishedCredentialsCallback add_phished_credentials_;

  // Calls `password_manager::RemovePhishedCredentials`. Used to facilitate
  // testing.
  ChangePhishedCredentialsCallback remove_phished_credentials_;

  // Bypasses the check for probability when sending sample pings.
  bool bypass_probability_for_tests_ = false;

  // Can be set for testing.
  raw_ptr<base::Clock> clock_;

  // Used to inject a different password hash, for testing. It's done as a
  // member callback rather than a virtual function because it's needed in the
  // constructor.
  StringProvider sync_password_hash_provider_for_testing_;

#if BUILDFLAG(IS_ANDROID)
  // Used on android to launch Password Checkup.
  std::unique_ptr<PasswordCheckupLauncherHelper> checkup_launcher_;
#endif
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
