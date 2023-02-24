// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForTabsPermission) {
  auto extension = ExtensionBuilder("Extension with tabs permission")
                       .AddPermission("tabs")
                       .Build();
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          extension.get(), Feature::Context::UNSPECIFIED_CONTEXT,
          GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForNoPermission) {
  auto extension = ExtensionBuilder("Extension with no permissions").Build();
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          extension.get(), Feature::Context::UNSPECIFIED_CONTEXT,
          GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully,
            scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForHostPermission) {
  auto extension = ExtensionBuilder("Extension with host permission")
                       .AddPermission("*://www.google.com/*")
                       .Build();
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          extension.get(), Feature::Context::UNSPECIFIED_CONTEXT,
          GURL("http://www.google.com/some/path"));
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForNoExtension) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          nullptr, Feature::Context::UNSPECIFIED_CONTEXT,
          GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully,
            scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForWebUI) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(nullptr,
                                            Feature::Context::WEBUI_CONTEXT,
                                            GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForWebUIUntrusted) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          nullptr, Feature::Context::WEBUI_UNTRUSTED_CONTEXT,
          GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully,
            scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ResolvePossiblyRelativeURL) {
  auto extension = ExtensionBuilder("test").Build();
  EXPECT_EQ(ExtensionTabUtil::ResolvePossiblyRelativeURL(
                "http://example.com/path", extension.get()),
            GURL("http://example.com/path"));
  EXPECT_EQ(
      ExtensionTabUtil::ResolvePossiblyRelativeURL("path", extension.get()),
      GURL("chrome-extension://jpignaibiiemhngfjkcpokkamffknabf/path"));
  EXPECT_EQ(ExtensionTabUtil::ResolvePossiblyRelativeURL("path", nullptr),
            GURL("path"));
}

TEST(ExtensionTabUtilTest, PrepareURLForNavigation) {
  auto extension = ExtensionBuilder("test").Build();
  // A fully qualified URL should return the same URL.
  {
    const std::string kTestUrl("http://google.com");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kTestUrl, extension.get(), /*browser_context=*/nullptr);
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(GURL(kTestUrl), *url);
  }
  // A relative path should return a URL relative to the extension's base URL.
  {
    const std::string kTestPath("foo");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kTestPath, extension.get(), /*browser_context=*/nullptr);
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(extension->GetResourceURL(kTestPath), *url);
  }
  // A kill URL should return false and set the error. There are several
  // different potential kill URLs and this just checks one of them.
  {
    const std::string kKillURL("chrome://crash");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kKillURL, extension.get(), /*browser_context=*/nullptr);
    ASSERT_FALSE(url.has_value());
    EXPECT_EQ(tabs_constants::kNoCrashBrowserError, url.error());
  }
}

TEST(ExtensionTabUtilTest, PrepareURLForNavigationOnDevtools) {
  const std::string kDevtoolsURL(
      "devtools://devtools/bundled/devtools_app.html");
  // A devtools url should return false and set the error.
  {
    auto no_permission_extension = ExtensionBuilder("none").Build();
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kDevtoolsURL, no_permission_extension.get(),
        /*browser_context=*/nullptr);
    ASSERT_FALSE(url.has_value());
    EXPECT_EQ(tabs_constants::kCannotNavigateToDevtools, url.error());
  }
  // Having the devtools permissions should allow access.
  {
    auto devtools_extension = ExtensionBuilder("devtools")
                                  .SetManifestKey("devtools_page", "foo.html")
                                  .Build();
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kDevtoolsURL, devtools_extension.get(), /*browser_context=*/nullptr);
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(kDevtoolsURL, *url);
  }
  // Having the debugger permissions should also allow access.
  {
    auto debugger_extension =
        ExtensionBuilder("debugger").AddPermission("debugger").Build();
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kDevtoolsURL, debugger_extension.get(), /*browser_context=*/nullptr);
    ASSERT_TRUE(url.has_value());
    EXPECT_EQ(kDevtoolsURL, *url);
  }
}

TEST(ExtensionTabUtilTest, PrepareURLForNavigationOnChromeUntrusted) {
  const std::string kChromeUntrustedURL("chrome-untrusted://terminal/");
  auto extension = ExtensionBuilder("none").Build();
  auto url = ExtensionTabUtil::PrepareURLForNavigation(
      kChromeUntrustedURL, extension.get(), /*browser_context=*/nullptr);
  ASSERT_FALSE(url.has_value());
  EXPECT_EQ(tabs_constants::kCannotNavigateToChromeUntrusted, url.error());
}

}  // namespace extensions
