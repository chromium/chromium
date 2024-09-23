// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "components/component_updater/installer_policies/first_party_sets_component_installer_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FirstPartySetsBrowserTestBase : public InProcessBrowserTest {
 public:
  GURL kUrlB = GURL("https://b.test");

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* GetFrame() {
    return content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  }

 private:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->RemoveSwitch(switches::kDisableComponentUpdate);
  }

  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitWithFeatures(
        /* enabled_features = */ {net::features::kForceThirdPartyCookieBlocking,
                                  net::features::kWaitForFirstPartySetsInit},
        /* disabled_features = */ {});
    CHECK(component_dir_.CreateUniqueTempDir());
    component_updater::FirstPartySetsComponentInstallerPolicy::
        WriteComponentForTesting(base::Version("1.2.3"),
                                 component_dir_.GetPath(),
                                 GetComponentContents());
  }

  virtual std::string GetComponentContents() const = 0;

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir component_dir_;
};

class FirstPartySetsBrowserTestWithSetChangeNotAffectingSitesUnderTest
    : public FirstPartySetsBrowserTestBase {
 private:
  std::string GetComponentContents() const override {
    if (GetTestPreCount() > 0) {
      return R"({"primary": "https://a.test",)"
             R"("associatedSites": ["https://b.test"]})";
    }

    // Before the last attempt, we change the Related Website Sets without
    // affecting the sites under test.
    // This will test that site data is only deleted if a site leaves a set.
    return R"({"primary": "https://a.test",)"
           R"("associatedSites": ["https://b.test", "https://c.test"]})";
  }
};

IN_PROC_BROWSER_TEST_F(
    FirstPartySetsBrowserTestWithSetChangeNotAffectingSitesUnderTest,
    PRE_CookieNotDeleted) {
  // Set a cookie for b.test/.
  ASSERT_TRUE(
      content::SetCookie(web_contents()->GetBrowserContext(), kUrlB,
                         "foo=bar;SameSite=None;Secure;Max-Age=2147483647"));
}

IN_PROC_BROWSER_TEST_F(
    FirstPartySetsBrowserTestWithSetChangeNotAffectingSitesUnderTest,
    CookieNotDeleted) {
  // After restart, check the cookies of b.test. Since b.test stayed in the
  // Related Website Sets, its cookies are still present.
  EXPECT_EQ("foo=bar",
            content::GetCookies(web_contents()->GetBrowserContext(), kUrlB));
}

class FirstPartySetsBrowserTestWithSiteLeavingSet
    : public FirstPartySetsBrowserTestBase {
 private:
  std::string GetComponentContents() const override {
    if (GetTestPreCount() > 0) {
      return R"({"primary": "https://a.test",)"
             R"("associatedSites": ["https://b.test"]})";
    }

    // Before the last attempt, we move b.test out of the Related Website Sets.
    // This will test if storage grants and cookies are removed as expected.
    return R"({"primary": "https://a.test",)"
           R"("associatedSites": ["https://c.test"]})";
  }
};

IN_PROC_BROWSER_TEST_F(FirstPartySetsBrowserTestWithSiteLeavingSet,
                       PRE_CookieDeleted) {
  // Set a cookie for b.test/.
  ASSERT_TRUE(
      content::SetCookie(web_contents()->GetBrowserContext(), kUrlB,
                         "foo=bar;SameSite=None;Secure;Max-Age=2147483647"));
}

IN_PROC_BROWSER_TEST_F(FirstPartySetsBrowserTestWithSiteLeavingSet,
                       CookieDeleted) {
  // After restart, check the cookies of b.test. Since b.test moved out of the
  // Related Website Sets, its cookies are going to be deleted.
  EXPECT_EQ("",
            content::GetCookies(web_contents()->GetBrowserContext(), kUrlB));
}

}  // namespace
