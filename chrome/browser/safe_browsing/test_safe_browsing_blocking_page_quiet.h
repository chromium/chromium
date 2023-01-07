// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_BLOCKING_PAGE_QUIET_H_
#define CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_BLOCKING_PAGE_QUIET_H_

#include "components/safe_browsing/content/browser/base_blocking_page.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/security_interstitials/core/base_safe_browsing_error_ui.h"
#include "components/security_interstitials/core/safe_browsing_quiet_error_ui.h"

namespace security_interstitials {

// This class is used in the testing of the quiet versions of the safe browsing
// interstitials via the Chrome browser, as it is currently only implemented
// by WebView.
class TestSafeBrowsingBlockingPageQuiet
    : public safe_browsing::BaseBlockingPage {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;

  TestSafeBrowsingBlockingPageQuiet(const TestSafeBrowsingBlockingPageQuiet&) =
      delete;
  TestSafeBrowsingBlockingPageQuiet& operator=(
      const TestSafeBrowsingBlockingPageQuiet&) = delete;

  ~TestSafeBrowsingBlockingPageQuiet() override;

  static TestSafeBrowsingBlockingPageQuiet* CreateBlockingPage(
      safe_browsing::BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResource& unsafe_resource,
      bool is_giant_webview);

  // std::unique_ptr<SafeBrowsingQuietErrorUI> sb_error_ui;
  std::string GetHTML();

 protected:
  // Don't instantiate this class directly, use CreateBlockingPage instead.
  TestSafeBrowsingBlockingPageQuiet(
      safe_browsing::BaseUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResourceList& unsafe_resources,
      const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
      bool is_giant_webview);

 private:
  security_interstitials::SafeBrowsingQuietErrorUI sb_error_ui_;
};

}  // namespace security_interstitials

#endif  // CHROME_BROWSER_SAFE_BROWSING_TEST_SAFE_BROWSING_BLOCKING_PAGE_QUIET_H_
