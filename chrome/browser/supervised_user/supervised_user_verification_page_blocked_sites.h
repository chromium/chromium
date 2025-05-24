// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_BLOCKED_SITES_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_BLOCKED_SITES_H_

#include <memory>
#include <string>

#include "chrome/browser/supervised_user/supervised_user_verification_page.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "content/public/browser/web_contents.h"

class GURL;

// This verification page is displayed for un-authenticated supervised users who
// try to access a blocked site. Once re-authentication is completed, they can
// then ask their parents for approvals.
class SupervisedUserVerificationPageForBlockedSites
    : public SupervisedUserVerificationPage {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  // `request_url` is the URL which triggered the interstitial page. It can be
  // a main frame or a subframe URL.
  //
  // `child_account_service` should only be null for demo interstitials, such as
  // for "chrome://interstitials/supervised-user-verify".
  SupervisedUserVerificationPageForBlockedSites(
      content::WebContents* web_contents,
      const std::string& email_to_reauth,
      const GURL& request_url,
      supervised_user::ChildAccountService* child_account_service,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client,
      supervised_user::FilteringBehaviorReason block_reason,
      bool is_main_frame,
      bool has_second_custodian = false);

  SupervisedUserVerificationPageForBlockedSites(
      const SupervisedUserVerificationPageForBlockedSites&) = delete;
  SupervisedUserVerificationPageForBlockedSites& operator=(
      const SupervisedUserVerificationPageForBlockedSites&) = delete;

  ~SupervisedUserVerificationPageForBlockedSites() override;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

 protected:
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;

 private:
  void RecordReauthStatusMetrics(Status status) override;
  int GetBlockMessageReasonId();
  supervised_user::FilteringBehaviorReason block_reason_;
  bool is_main_frame_;
  bool has_second_custodian_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_BLOCKED_SITES_H_
