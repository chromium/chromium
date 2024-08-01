// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_H_

#include <memory>
#include <string>

#include "content/public/browser/web_contents.h"
#include "components/security_interstitials/content/security_interstitial_page.h"

class GURL;

// This class is responsible for showing/hiding the interstitial page that
// occurs when a supervised user tries to access a page that requires
// verification.
class SupervisedUserVerificationPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  // |request_url| is the URL which triggered the interstitial page. It can be
  // a main frame or a subresource URL.
  SupervisedUserVerificationPage(
      content::WebContents* web_contents,
      const std::string& email_to_reauth,
      const GURL& request_url,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);

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
  void PopulateStringsForSharedHTML(base::Value::Dict& load_time_data);

  const std::string email_to_reauth_;
  const GURL request_url_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_VERIFICATION_PAGE_H_
