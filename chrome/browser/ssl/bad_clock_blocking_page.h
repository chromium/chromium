// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_BAD_CLOCK_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SSL_BAD_CLOCK_BLOCKING_PAGE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ssl/ssl_blocking_page_base.h"
#include "components/security_interstitials/content/ssl_cert_reporter.h"
#include "components/ssl_errors/error_classification.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "net/ssl/ssl_info.h"

class GURL;

namespace security_interstitials {
class BadClockUI;
}

// This class is responsible for showing/hiding the interstitial page that
// occurs when an SSL error is triggered by a clock misconfiguration. It
// creates the UI using security_interstitials::BadClockUI and then
// displays it. It deletes itself when the interstitial page is closed.
class BadClockBlockingPage : public SSLBlockingPageBase {
 public:
  // Interstitial type, used in tests.
  static const InterstitialPageDelegate::TypeID kTypeForTesting;

  // If the blocking page isn't shown, the caller is responsible for cleaning
  // up the blocking page. Otherwise, the interstitial takes ownership when
  // shown.
  BadClockBlockingPage(content::WebContents* web_contents,
                       int cert_error,
                       const net::SSLInfo& ssl_info,
                       const GURL& request_url,
                       const base::Time& time_triggered,
                       ssl_errors::ClockState clock_state,
                       std::unique_ptr<SSLCertReporter> ssl_cert_reporter);

  ~BadClockBlockingPage() override;

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

  const std::unique_ptr<security_interstitials::BadClockUI> bad_clock_ui_;

  DISALLOW_COPY_AND_ASSIGN(BadClockBlockingPage);
};

#endif  // CHROME_BROWSER_SSL_BAD_CLOCK_BLOCKING_PAGE_H_
