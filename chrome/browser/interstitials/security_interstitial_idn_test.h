// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_IDN_TEST_H_
#define CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_IDN_TEST_H_

#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class WebContents;
}  // namespace content

class GURL;

namespace security_interstitials {
class SecurityInterstitialPage;
}  // namespace security_interstitials

namespace chrome_browser_interstitials {

// This class is used for testing the display of IDN names in security
// interstitials.
class SecurityInterstitialIDNTest : public InProcessBrowserTest {
 public:
  // Run a test that creates an interstitial with an IDN request URL
  // and checks that it is properly decoded.
  testing::AssertionResult VerifyIDNDecoded() const;

 protected:
  virtual security_interstitials::SecurityInterstitialPage* CreateInterstitial(
      content::WebContents* contents,
      const GURL& request_url) const = 0;
};

}  // namespace chrome_browser_interstitials

#endif  // CHROME_BROWSER_INTERSTITIALS_SECURITY_INTERSTITIAL_IDN_TEST_H_
