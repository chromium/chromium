// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/url_blocking_policy_test_utils.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace policy {

UrlBlockingPolicyTest::UrlBlockingPolicyTest() = default;

UrlBlockingPolicyTest::~UrlBlockingPolicyTest() = default;

void UrlBlockingPolicyTest::CheckURLIsBlockedInWebContents(
    content::WebContents* web_contents,
    const GURL& url) {
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());

  std::u16string blocked_page_title;
  if (url.has_host()) {
    blocked_page_title = base::UTF8ToUTF16(url.host());
  } else {
    // Local file paths show the full URL.
    blocked_page_title = base::UTF8ToUTF16(url.spec());
  }
  EXPECT_EQ(blocked_page_title, web_contents->GetTitle());

  // Verify that the expected error page is being displayed.
  EXPECT_EQ(true,
            content::EvalJs(
                web_contents,
                content::JsReplace(
                    "var textContent = document.body.textContent;"
                    "var hasError = "
                    "textContent.indexOf($1) >= 0;"
                    "hasError;",
                    l10n_util::GetStringUTF8(
                        IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_ADMINISTRATOR))));
}

void UrlBlockingPolicyTest::CheckURLIsBlocked(Browser* browser,
                                              const std::string& spec) {
  GURL url(spec);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, url));
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  CheckURLIsBlockedInWebContents(contents, url);
}

void UrlBlockingPolicyTest::CheckViewSourceURLIsBlocked(
    Browser* browser,
    const std::string& spec) {
  GURL url(spec);
  GURL view_source_url("view-source:" + spec);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser, view_source_url));
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  CheckURLIsBlockedInWebContents(contents, url);
}

}  // namespace policy
