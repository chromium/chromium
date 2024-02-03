// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_WARN_PAGE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_WARN_PAGE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/security_interstitials/content/security_interstitial_page.h"

namespace safe_browsing {
class BaseUIManager;
}  // namespace safe_browsing

class GURL;

// This class is responsible for showing/hiding the interstitial page that
// occurs when a url is suspicious and a warning needs to be displayed as per
// rules configured by the admin of an enterprise managed browser.
class EnterpriseWarnPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // Interstitial type, used in tests.
  static const security_interstitials::SecurityInterstitialPage::TypeID
      kTypeForTesting;

  // |request_url| is the URL which triggered the interstitial page.
  EnterpriseWarnPage(
      safe_browsing::BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& request_url,
      const safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList&
          unsafe_resources,
      std::unique_ptr<
          security_interstitials::SecurityInterstitialControllerClient>
          controller);

  EnterpriseWarnPage(const EnterpriseWarnPage&) = delete;
  EnterpriseWarnPage& operator=(const EnterpriseWarnPage&) = delete;

  ~EnterpriseWarnPage() override;

  // SecurityInterstitialPage method:
  security_interstitials::SecurityInterstitialPage::TypeID GetTypeForTesting()
      override;

  std::string GetCustomMessageForTesting();

 protected:
  // SecurityInterstitialPage implementation:
  void CommandReceived(const std::string& command) override;
  void PopulateInterstitialStrings(base::Value::Dict& load_time_data) override;
  void OnInterstitialClosing() override;
  int GetHTMLTemplateId() override;

 private:
  const raw_ptr<safe_browsing::BaseUIManager> ui_manager_;
  const safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList
      unsafe_resources_;
  void PopulateStringsForSharedHTML(base::Value::Dict& load_time_data);
};

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_INTERSTITIALS_ENTERPRISE_WARN_PAGE_H_
