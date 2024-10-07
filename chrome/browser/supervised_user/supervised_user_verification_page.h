// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

// LINT.IfChange(FamilyLinkUserReauthenticationInterstitialState)
// State of the re-authentication interstitial indicatins if the user
// has interacted with the sign-in flow.
enum class FamilyLinkUserReauthenticationInterstitialState : int {
  kInterstitialShown = 0,
  kReauthenticationStarted = 1,
  kReauthenticationCompleted = 2,
  kMaxValue = kReauthenticationCompleted,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:FamilyLinkUserReauthenticationInterstitialState)

// This class is responsible for showing/hiding the interstitial page that
// occurs when a supervised user tries to access a page that requires
// verification.
class SupervisedUserVerificationPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // The purpose of the re-authentication interstitial determines its layout and
  // displayed texts.
  enum class VerificationPurpose {
    REAUTH_REQUIRED_SITE,  // Show the interstitial for YouTube, which requires
                           // authentication to determine content restrictions.
    DEFAULT_BLOCKED_SITE,  // Show the interstitial for blocked sites.
                           // Re-authentication is needed so that supervised
                           // users can ask for parent's approval.
    SAFE_SITES_BLOCKED_SITE,  // Show the interstitial for sites blocked by the
                              // explicit sites checker.
    MANUAL_BLOCKED_SITE,  // Show the interstitial for sites blocked manually.
  };

  // The status of the interstitial used for metrics recording purposes.
  enum class Status { SHOWN, REAUTH_STARTED, REAUTH_COMPLETED };

  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  // Whether the user is in a suitable auth state for this page to be shown.
  static bool ShouldShowPage(
      const supervised_user::ChildAccountService& child_account_service);

  // `request_url` is the URL which triggered the interstitial page. It can be
  // a main frame or a subresource URL.
  // `child_account_service` should only be null for demo interstitials, such as
  // for "chrome://interstitials/supervised-user-verify".
  SupervisedUserVerificationPage(
      content::WebContents* web_contents,
      const std::string& email_to_reauth,
      const GURL& request_url,
      VerificationPurpose verification_purpose,
      supervised_user::ChildAccountService* child_account_service,
      ukm::SourceId source_id,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client,
      bool is_main_frame = true,
      bool has_second_custodian = false);

  SupervisedUserVerificationPage(const SupervisedUserVerificationPage&) =
      delete;
  SupervisedUserVerificationPage& operator=(
      const SupervisedUserVerificationPage&) = delete;

  ~SupervisedUserVerificationPage() override;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

 protected:
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;
  void OnInterstitialClosing() override;
  int GetHTMLTemplateId() override;

 private:
  void CloseSignInTabs();
  // Returns true if the provided url matches a list of urls that
  // are known to be part of the sign-in flow.
  bool IsSignInUrl(const GURL& url);
  void OnGoogleAuthStateUpdate();
  void PopulateStringsForSharedHTML(base::Value::Dict& load_time_data);
  void RecordReauthStatusMetrics(Status status);
  void RecordYouTubeReauthStatusUkm(Status status);
  void RecordBlockedUrlReauthStatusUma(Status status);
  int GetBlockMessageReasonId();
  base::CallbackListSubscription google_auth_state_subscription_;
  const std::string email_to_reauth_;
  const GURL request_url_;
  const GURL sign_in_continue_url_;
  const GURL reauth_url_;
  const VerificationPurpose verification_purpose_;
  raw_ptr<supervised_user::ChildAccountService> child_account_service_;
  ukm::SourceId source_id_;
  bool is_main_frame_;
  bool has_second_custodian_;
  // List with unique tab identifiers for spawned sign-in tabs.
  std::list<uint32_t> signin_tabs_handle_id_list_;
  base::WeakPtrFactory<SupervisedUserVerificationPage> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_H_
