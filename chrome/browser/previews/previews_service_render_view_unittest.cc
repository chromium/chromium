// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_service.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

class PreviewsServiceRenderViewTest : public ChromeRenderViewHostTestHarness {
 public:
  PreviewsServiceRenderViewTest() = default;
  ~PreviewsServiceRenderViewTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        previews::features::kDeferAllScriptPreviews);
    ChromeRenderViewHostTestHarness::SetUp();
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

  content::BrowserContext* browser_context() {
    return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(PreviewsServiceRenderViewTest);
};

TEST_F(PreviewsServiceRenderViewTest, MatchesDeferAllScriptDenyListRegexp) {
  auto* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context()));

  const struct {
    std::string url;
    bool expected_denylist_match;
  } tests[] = {
      // Base case with no blocking regexes.
      {"https://www.foo.com", false},
      {"https://www.foo.com/a/b/c.html", false},

      // Contains substring "login" which should match the denylist.
      {"https://www.foo.com/a/login/c.html", true},
      {"https://www.foo.com/a/login/c.html?q=a", true},
      {"https://www.foo.com/a/b/login_now.html", true},
      {"https://www.foo.com/a/b/login_now.html?q=a", true},

      // Contains substring "logout" which should match the denylist.
      {"https://www.foo.com/a/logout/c.html", true},
      {"https://www.foo.com/a/logout/c.html?q=a", true},
      {"https://www.foo.com/a/b/logout_now.html", true},
      {"https://www.foo.com/a/b/logout_now.html?q=a", true},
      {"https://www.foo.com/a/b/logout_now.html?q=a", true},

      // Contains substring "banking" which should match the denylist.
      {"https://www.foo.com/a/banking/c.html", true},
      {"https://www.foo.com/a/banking/c.html?q=a", true},
      {"https://www.foo.com/a/b/banking_now.html", true},
      {"https://www.foo.com/a/b/banking_now.html?q=a", true},

      // Contains substring "logger" which should not match the denylist.
      {"https://www.foo.com/a/logger/c.html", false},
      {"https://www.foo.com/a/logger/c.html?q=a", false},
      {"https://www.foo.com/a/b/logger_now.html", false},
      {"https://www.foo.com/a/b/logger_now.html?q=a", false},
  };

  for (const auto& test : tests) {
    GURL url(test.url);
    EXPECT_TRUE(url.is_valid());
    EXPECT_EQ(test.expected_denylist_match,
              previews_service->MatchesDeferAllScriptDenyListRegexp(url))
        << " url=" << test.url;
  }
}
