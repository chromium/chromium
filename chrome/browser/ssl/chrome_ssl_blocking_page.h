// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_SSL_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SSL_CHROME_SSL_BLOCKING_PAGE_H_

#include "base/macros.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_blocking_page_base.h"

// Contains utilities for Chrome-specific construction of SSL pages.
class ChromeSSLBlockingPage {
 public:
  // Creates an SSL blocking page. If the blocking page isn't shown, the caller
  // is responsible for cleaning up the blocking page, otherwise the
  // interstitial takes ownership when shown. |options_mask| must be a bitwise
  // mask of SSLErrorUI::SSLErrorOptionsMask values.
  static SSLBlockingPage* Create(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      int options_mask,
      const base::Time& time_triggered,
      const GURL& support_url,
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter);

  // Does setup on |page| that is specific to the client (Chrome).
  static void DoChromeSpecificSetup(SSLBlockingPageBase* page);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ChromeSSLBlockingPage);
};

#endif  // CHROME_BROWSER_SSL_CHROME_SSL_BLOCKING_PAGE_H_
