// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_INTERSTITIALS_MANAGED_PROFILE_REQUIRED_PAGE_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_INTERSTITIALS_MANAGED_PROFILE_REQUIRED_PAGE_H_

#include <memory>
#include <string>

#include "chrome/browser/enterprise/connectors/common.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/security_interstitials/content/security_interstitial_page.h"

class GURL;

class ManagedProfileRequiredPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  // |request_url| is the URL which triggered the interstitial page. It can be
  // a main frame or a subresource URL.
  ManagedProfileRequiredPage(
      content::WebContents* web_contents,
      const GURL& request_url,
      const std::u16string& manager,
      const std::u16string& email,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller_client);

  ManagedProfileRequiredPage(const ManagedProfileRequiredPage&) = delete;
  ManagedProfileRequiredPage& operator=(const ManagedProfileRequiredPage&) =
      delete;

  ~ManagedProfileRequiredPage() override;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

  base::Value::Dict GetLoadTimeDataForTesting();

 protected:
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;
  void OnInterstitialClosing() override;

 private:
  void PopulateStringsForSharedHTML(base::Value::Dict& load_time_data);

  std::u16string manager_;
  std::u16string email_;
};

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_INTERSTITIALS_MANAGED_PROFILE_REQUIRED_PAGE_H_
