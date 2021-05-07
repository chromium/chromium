// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/security_interstitial_idn_test.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace chrome_browser_interstitials {

testing::AssertionResult SecurityInterstitialIDNTest::VerifyIDNDecoded() const {
  const char kHostname[] = "xn--d1abbgf6aiiy.xn--p1ai";
  const char kHostnameUnicode[] = "президент.рф";
  std::string request_url_spec = base::StringPrintf("https://%s/", kHostname);
  GURL request_url(request_url_spec);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(contents);
  security_interstitials::SecurityInterstitialPage* blocking_page =
      CreateInterstitial(contents, request_url);
  content::TestNavigationObserver observer(contents);
  contents->GetController().LoadPostCommitErrorPage(
      contents->GetMainFrame(), request_url, blocking_page->GetHTMLContents(),
      net::ERR_BLOCKED_BY_CLIENT);
  observer.Wait();
  delete blocking_page;
  if (ui_test_utils::FindInPage(contents, base::UTF8ToUTF16(kHostnameUnicode),
                                true /*forward*/, true /*case_sensitive*/,
                                nullptr, nullptr) == 1) {
    return testing::AssertionSuccess();
  }
  return testing::AssertionFailure() << "Interstitial not displaying text";
}

}  // namespace chrome_browser_interstitials
