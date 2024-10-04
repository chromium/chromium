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

// This class provides common functionalities for the supervised user
// re-authentication interstitials, such as opening the re-auth url in a new tab
// and closing this tab automatically on successful re-auth .
class SupervisedUserVerificationPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // The status of the interstitial used for metrics recording purposes.
  enum class Status { SHOWN, REAUTH_STARTED, REAUTH_COMPLETED };

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
      supervised_user::ChildAccountService* child_account_service,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);

  SupervisedUserVerificationPage(const SupervisedUserVerificationPage&) =
      delete;
  SupervisedUserVerificationPage& operator=(
      const SupervisedUserVerificationPage&) = delete;

  ~SupervisedUserVerificationPage() override;

 protected:
  void CommandReceived(const std::string& command) override;
  void OnInterstitialClosing() override;
  int GetHTMLTemplateId() override;
  virtual void RecordReauthStatusMetrics(Status status) = 0;
  void PopulateCommonStrings(base::Value::Dict& load_time_data);
  bool IsReauthCompleted();

 private:
  void CloseSignInTabs();
  // Returns true if the provided url matches a list of urls that
  // are known to be part of the sign-in flow.
  bool IsSignInUrl(const GURL& url);
  void OnGoogleAuthStateUpdate();
  bool is_reauth_completed_ = false;
  base::CallbackListSubscription google_auth_state_subscription_;
  const std::string email_to_reauth_;
  const GURL request_url_;
  const GURL sign_in_continue_url_;
  const GURL reauth_url_;
  raw_ptr<supervised_user::ChildAccountService> child_account_service_;
  // List with unique tab identifiers for spawned sign-in tabs.
  std::list<uint32_t> signin_tabs_handle_id_list_;
  base::WeakPtrFactory<SupervisedUserVerificationPage> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_H_
