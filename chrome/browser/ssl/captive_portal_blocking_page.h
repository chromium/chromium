// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CAPTIVE_PORTAL_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SSL_CAPTIVE_PORTAL_BLOCKING_PAGE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ssl/ssl_blocking_page_base.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace content {
class NavigationEntry;
class WebContents;
}

namespace net {
class SSLInfo;
}

class SSLCertReporter;

// This class is responsible for showing/hiding the interstitial page that is
// shown when a captive portal triggers an SSL error.
// It deletes itself when the interstitial page is closed.
//
// This class should only be used on the UI thread because its implementation
// uses captive_portal::CaptivePortalService, which can only be accessed on the
// UI thread. Only used when ENABLE_CAPTIVE_PORTAL_DETECTION is true.
class CaptivePortalBlockingPage : public SSLBlockingPageBase {
 public:
  // Interstitial type, for testing.
  static const void* const kTypeForTesting;

  CaptivePortalBlockingPage(content::WebContents* web_contents,
                            const GURL& request_url,
                            const GURL& login_url,
                            std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
                            const net::SSLInfo& ssl_info,
                            int cert_error);
  ~CaptivePortalBlockingPage() override;

  // InterstitialPageDelegate method:
  const void* GetTypeForTesting() override;

 protected:
  // Returns true if the connection is a Wi-Fi connection. Virtual for tests.
  virtual bool IsWifiConnection() const;
  // Returns the SSID of the connected Wi-Fi network, if any. Virtual for tests.
  virtual std::string GetWiFiSSID() const;

  // SecurityInterstitialPage methods:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;

  // InterstitialPageDelegate method:
  void CommandReceived(const std::string& command) override;
  void OverrideEntry(content::NavigationEntry* entry) override;

 private:
  // URL of the login page, opened when the user clicks the "Connect" button.
  // If empty, the default captive portal detection URL for the platform will be
  // used.
  const GURL login_url_;
  const net::SSLInfo ssl_info_;

  DISALLOW_COPY_AND_ASSIGN(CaptivePortalBlockingPage);
};

#endif  // CHROME_BROWSER_SSL_CAPTIVE_PORTAL_BLOCKING_PAGE_H_
