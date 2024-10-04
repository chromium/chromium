// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_YOUTUBE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_YOUTUBE_H_

#include "chrome/browser/supervised_user/supervised_user_verification_page.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/supervised_user/core/browser/child_account_service.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

// This verification page is displayed for un-authenticated supervised users who
// try to access YouTube. Once re-authentication is completed, YouTube content
// restrictions can be applied.
class SupervisedUserVerificationPageForYouTube
    : public SupervisedUserVerificationPage {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  // `request_url` is the Youtube URL which triggered the interstitial page. It
  // can be a main frame or an embedded YouTube URL.
  //
  // `child_account_service` should only be null for demo interstitials, e.g.
  // chrome://interstitials/supervised-user-verify.
  SupervisedUserVerificationPageForYouTube(
      content::WebContents* web_contents,
      const std::string& email_to_reauth,
      const GURL& request_url,
      supervised_user::ChildAccountService* child_account_service,
      ukm::SourceId source_id,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client,
      bool is_main_frame);

  SupervisedUserVerificationPageForYouTube(
      const SupervisedUserVerificationPageForYouTube&) = delete;
  SupervisedUserVerificationPageForYouTube& operator=(
      const SupervisedUserVerificationPageForYouTube&) = delete;

  ~SupervisedUserVerificationPageForYouTube() override;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

 protected:
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;

 private:
  void RecordReauthStatusMetrics(Status status) override;
  ukm::SourceId source_id_;
  bool is_main_frame_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_YOUTUBE_H_
