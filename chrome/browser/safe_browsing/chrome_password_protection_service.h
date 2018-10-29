// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_

#include <map>

#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "components/sessions/core/session_id.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "ui/base/ui_features.h"
#include "url/origin.h"

struct AccountInfo;
class PrefChangeRegistrar;
class PrefService;
class PrefChangeRegistrar;
class Profile;

namespace content {
class WebContents;
}

namespace policy {
class BrowserPolicyConnector;
}

namespace safe_browsing {

class SafeBrowsingService;
class SafeBrowsingNavigationObserverManager;
class SafeBrowsingUIManager;
class ChromePasswordProtectionService;

using OnWarningDone = base::OnceCallback<void(WarningAction)>;
using StringProvider = base::RepeatingCallback<std::string()>;
using url::Origin;

// Shows the platform-specific password reuse modal dialog.
void ShowPasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    ReusedPasswordType password_type,
    OnWarningDone done_callback);

// Called by ChromeContentBrowserClient to create a
// PasswordProtectionNavigationThrottle if appropriate.
std::unique_ptr<PasswordProtectionNavigationThrottle>
MaybeCreateNavigationThrottle(content::NavigationHandle* navigation_handle);

// ChromePasswordProtectionService extends PasswordProtectionService by adding
// access to SafeBrowsingNaivigationObserverManager and Profile.
class ChromePasswordProtectionService : public PasswordProtectionService {
 public:
  // Observer is used to coordinate password protection UIs (e.g. modal warning,
  // change password card, etc) in reaction to user events.
  class Observer {
   public:
    // Called when user completes the Gaia password reset.
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
  ~ChromePasswordProtectionService() override;

  static ChromePasswordProtectionService* GetPasswordProtectionService(
      Profile* profile);

  static bool ShouldShowChangePasswordSettingUI(Profile* profile);

  // Called by SecurityStateTabHelper to determine if page info bubble should
  // show password reuse warning.
  static bool ShouldShowPasswordReusePageInfoBubble(
      content::WebContents* web_contents,
      ReusedPasswordType password_type);

  // Called by ChromeWebUIControllerFactory class to determine if Chrome should
  // show chrome://reset-password page.
  static bool IsPasswordReuseProtectionConfigured(Profile* profile);

  void ShowModalWarning(content::WebContents* web_contents,
                        const std::string& verdict_token,
                        ReusedPasswordType password_type) override;

  void ShowInterstitial(content::WebContents* web_contens,
                        ReusedPasswordType password_type) override;

  // Called when user interacts with password protection UIs.
  void OnUserAction(content::WebContents* web_contents,
                    ReusedPasswordType password_type,
                    WarningUIType ui_type,
                    WarningAction action);

  // Called during the construction of Observer subclass.
  virtual void AddObserver(Observer* observer);

  // Called during the destruction of the observer subclass.
  virtual void RemoveObserver(Observer* observer);

  // Starts collecting threat details if user has extended reporting enabled and
  // is not in incognito mode.
  void MaybeStartThreatDetailsCollection(content::WebContents* web_contents,
                                         const std::string& token,
                                         ReusedPasswordType password_type);

  // Sends threat details if user has extended reporting enabled and is not in
  // incognito mode.
  void MaybeFinishCollectingThreatDetails(content::WebContents* web_contents,
                                          bool did_proceed);

  // Check if Gaia password hash is changed.
  void CheckGaiaPasswordChange();

  // Called when sync user's Gaia password changed.
  void OnGaiaPasswordChanged();

  // If user has clicked through any Safe Browsing interstitial on this given
  // |web_contents|.
  bool UserClickedThroughSBInterstitial(
      content::WebContents* web_contents) override;

  // If |prefs::kPasswordProtectionWarningTrigger| is not managed by enterprise
  // policy, this function should always return PHISHING_REUSE. Otherwise,
  // returns the specified pref value.
  PasswordProtectionTrigger GetPasswordProtectionWarningTriggerPref()
      const override;

  // Gets the enterprise change password URL if specified in policy,
  // otherwise gets the default GAIA change password URL.
  GURL GetEnterpriseChangePasswordURL() const;

  // Gets the GAIA change password URL based on |account_info_|.
  GURL GetDefaultChangePasswordURL() const;

  // If |url| matches Safe Browsing whitelist domains, password protection
  // change password URL, or password protection login URLs in the enterprise
  // policy.
  bool IsURLWhitelistedForPasswordEntry(const GURL& url,
                                        RequestOutcome* reason) const override;

  // Gets the type of sync account associated with current profile or
  // |NOT_SIGNED_IN|.
  LoginReputationClientRequest::PasswordReuseEvent::SyncAccountType
  GetSyncAccountType() const override;

  // Gets the detailed warning text that should show in the modal warning dialog
  // and page info bubble.
  base::string16 GetWarningDetailText(ReusedPasswordType password_type) const;

  // If password protection trigger is configured via enterprise policy, gets
  // the name of the organization that owns the enterprise policy. Otherwise,
  // returns an empty string.
  std::string GetOrganizationName(ReusedPasswordType password_type) const;

  // Triggers "safeBrowsingPrivate.OnPolicySpecifiedPasswordReuseDetected"
  // extension API for enterprise reporting.
  // |is_phishing_url| indicates if the password reuse happened on a phishing
  // page.
  void OnPolicySpecifiedPasswordReuseDetected(const GURL& url,
                                              bool is_phishing_url) override;

  // Triggers "safeBrowsingPrivate.OnPolicySpecifiedPasswordChanged" API.
  void OnPolicySpecifiedPasswordChanged() override;

  // Returns true if there's any enterprise password reuses unhandled in
  // |web_contents|.
  // "Unhandled" is defined as user hasn't clicked on "Change Password" button
  // in modal warning dialog.
  bool HasUnhandledEnterprisePasswordReuse(
      content::WebContents* web_contents) const;

 protected:
  // PasswordProtectionService overrides.
  const policy::BrowserPolicyConnector* GetBrowserPolicyConnector()
      const override;
  // Obtains referrer chain of |event_url| and |event_tab_id| and add this
  // info into |frame|.
  void FillReferrerChain(const GURL& event_url,
                         SessionID event_tab_id,
                         LoginReputationClientRequest::Frame* frame) override;

  bool IsExtendedReporting() override;

  bool IsIncognito() override;

  // Checks if pinging should be enabled based on the |trigger_type|,
  // |password_type| and user state, updates |reason| accordingly.
  bool IsPingingEnabled(LoginReputationClientRequest::TriggerType trigger_type,
                        ReusedPasswordType password_type,
                        RequestOutcome* reason) override;

  // If user enabled history syncing.
  bool IsHistorySyncEnabled() override;

  // If user is under advanced protection.
  bool IsUnderAdvancedProtection() override;

  void MaybeLogPasswordReuseDetectedEvent(
      content::WebContents* web_contents) override;

  void MaybeLogPasswordReuseLookupEvent(
      content::WebContents* web_contents,
      RequestOutcome outcome,
      const LoginReputationClientResponse* response) override;

  // Updates security state for the current |web_contents| based on
  // |threat_type| and reused |password_type|, such that page info bubble will
  // show appropriate status when user clicks on the security chip.
  void UpdateSecurityState(SBThreatType threat_type,
                           ReusedPasswordType password_type,
                           content::WebContents* web_contents) override;

  void RemoveUnhandledSyncPasswordReuseOnURLsDeleted(
      bool all_history,
      const history::URLRows& deleted_rows) override;

  // Gets |account_info_| based on |profile_|.
  virtual AccountInfo GetAccountInfo() const;

  void HandleUserActionOnModalWarning(content::WebContents* web_contents,
                                      ReusedPasswordType password_type,
                                      WarningAction action);

  void HandleUserActionOnPageInfo(content::WebContents* web_contents,
                                  ReusedPasswordType password_type,
                                  WarningAction action);

  void HandleUserActionOnSettings(content::WebContents* web_contents,
                                  WarningAction action);

  void HandleResetPasswordOnInterstitial(content::WebContents* web_contents,
                                         WarningAction action);

  // Returns base-10 string representation of the uint64t hash.
  std::string GetSyncPasswordHashFromPrefs();

  void SetGaiaPasswordHashForTesting(const std::string& new_password_hash) {
    sync_password_hash_ = new_password_hash;
  }

  // Determines if we should show chrome://reset-password interstitial based on
  // previous request outcome, the reused |password_type| and the
  // |main_frame_url|.
  bool CanShowInterstitial(RequestOutcome reason,
                           ReusedPasswordType password_type,
                           const GURL& main_frame_url) override;

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
      VerifyPasswordReuseUserEventNotRecordedDueToNotSignedIn);
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
                           VerifySendsPingForAboutBlank);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventScheduledOnStartup);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventScheduledFromPref);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventReschedules);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceTest,
                           VerifyPasswordCaptureEventRecorded);

  // Browser tests
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceBrowserTest,
                           VerifyCheckGaiaPasswordChange);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceBrowserTest,
                           OnEnterpriseTriggerOff);
  FRIEND_TEST_ALL_PREFIXES(ChromePasswordProtectionServiceBrowserTest,
                           OnEnterpriseTriggerOffGSuite);

 private:
  friend class MockChromePasswordProtectionService;
  friend class ChromePasswordProtectionServiceBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(
      ChromePasswordProtectionServiceTest,
      VerifyOnPolicySpecifiedPasswordReuseDetectedEventForPhishingReuse);

  // Gets prefs associated with |profile_|.
  PrefService* GetPrefs();

  // Returns whether the profile is valid and has safe browsing service enabled.
  bool IsSafeBrowsingEnabled();

  void MaybeLogPasswordReuseLookupResult(
      content::WebContents* web_contents,
      sync_pb::UserEventSpecifics::GaiaPasswordReuse::PasswordReuseLookup::
          LookupResult result);

  void MaybeLogPasswordReuseLookupResultWithVerdict(
      content::WebContents* web_contents,
      sync_pb::UserEventSpecifics::GaiaPasswordReuse::PasswordReuseLookup::
          LookupResult result,
      sync_pb::UserEventSpecifics::GaiaPasswordReuse::PasswordReuseLookup::
          ReputationVerdict verdict,
      const std::string& verdict_token);

  void MaybeLogPasswordReuseDialogInteraction(
      int64_t navigation_id,
      sync_pb::UserEventSpecifics::GaiaPasswordReuse::
          PasswordReuseDialogInteraction::InteractionResult interaction_result);

  // Log that we captured the password, either due to log-in or by timer.
  // This also sets the reoccuring timer.
  void MaybeLogPasswordCapture(bool did_log_in);
  void SetLogPasswordCaptureTimer(const base::TimeDelta& delay);

  void OnModalWarningShownForSignInPassword(content::WebContents* web_contents,
                                            const std::string& verdict_token);

  void OnModalWarningShownForEnterprisePassword(
      content::WebContents* web_contents,
      const std::string& verdict_token);

  // Constructor used for tests only.
  ChromePasswordProtectionService(
      Profile* profile,
      scoped_refptr<HostContentSettingsMap> content_setting_map,
      scoped_refptr<SafeBrowsingUIManager> ui_manager,
      StringProvider sync_password_hash_provider);

  // Code shared by both ctors.
  void Init();

  // If enterprise admin turns off password protection, removes all captured
  // enterprise password hashes.
  void OnWarningTriggerChanged();

  // Informs PasswordReuseDetector that enterprise password URLs (login URL or
  // change password URL) have been changed.
  void OnEnterprisePasswordUrlChanged();

  // Get the content area size of current browsing window.
  gfx::Size GetCurrentContentAreaSize() const override;

  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  TriggerManager* trigger_manager_;
  // Profile associated with this instance.
  Profile* profile_;
  // Current sync password hash.
  std::string sync_password_hash_;
  scoped_refptr<SafeBrowsingNavigationObserverManager>
      navigation_observer_manager_;
  base::ObserverList<Observer>::Unchecked observer_list_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::set<content::WebContents*>
      web_contents_with_unhandled_enterprise_reuses_;

  // Schedules the next time to log the PasswordCaptured event.
  base::OneShotTimer log_password_capture_timer_;

  // Used to inject a different password hash, for testing. It's done as a
  // member callback rather than a virtual function because it's needed in the
  // constructor.
  StringProvider sync_password_hash_provider_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(ChromePasswordProtectionService);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_PASSWORD_PROTECTION_SERVICE_H_
