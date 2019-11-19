// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_BLOCKED_INTERCEPTION_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SSL_BLOCKED_INTERCEPTION_BLOCKING_PAGE_H_

#include "base/macros.h"
#include "chrome/browser/ssl/ssl_blocking_page_base.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/security_interstitials/core/blocked_interception_ui.h"
#include "net/ssl/ssl_info.h"

class BlockedInterceptionBlockingPage : public SSLBlockingPageBase {
 public:
  // Interstitial type, used in tests.
  static const InterstitialPageDelegate::TypeID kTypeForTesting;

  BlockedInterceptionBlockingPage(
      content::WebContents* web_contents,
      int cert_error,
      const GURL& request_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
      const net::SSLInfo& ssl_info);
  ~BlockedInterceptionBlockingPage() override;

  // InterstitialPageDelegate method:
  InterstitialPageDelegate::TypeID GetTypeForTesting() override;

 protected:
  // InterstitialPageDelegate implementation:
  void CommandReceived(const std::string& command) override;
  void OverrideEntry(content::NavigationEntry* entry) override;

  // SecurityInterstitialPage implementation:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;

 private:
  const net::SSLInfo ssl_info_;

  const std::unique_ptr<security_interstitials::BlockedInterceptionUI>
      blocked_interception_ui_;

  DISALLOW_COPY_AND_ASSIGN(BlockedInterceptionBlockingPage);
};

#endif  // CHROME_BROWSER_SSL_BLOCKED_INTERCEPTION_BLOCKING_PAGE_H_
