// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include "base/json/json_reader.h"
#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForTabsPermission) {
  auto extension = ExtensionBuilder("Extension with tabs permission")
                       .AddAPIPermission("tabs")
                       .Build();
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension.get(),
                                            mojom::ContextType::kUnspecified,
                                            GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForNoPermission) {
  auto extension = ExtensionBuilder("Extension with no permissions").Build();
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension.get(),
                                            mojom::ContextType::kUnspecified,
                                            GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully,
            scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForHostPermission) {
  auto extension = ExtensionBuilder("Extension with host permission")
                       .AddHostPermission("*://www.google.com/*")
                       .Build();
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(
          extension.get(), mojom::ContextType::kUnspecified,
          GURL("http://www.google.com/some/path"));
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForNoExtension) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(nullptr,
                                            mojom::ContextType::kUnspecified,
                                            GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully,
            scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kScrubTabFully, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForWebUI) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(nullptr, mojom::ContextType::kWebUi,
                                            GURL("http://www.google.com"));
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.committed_info);
  EXPECT_EQ(ExtensionTabUtil::kDontScrubTab, scrub_tab_behavior.pending_info);
}

TEST(ExtensionTabUtilTest, ScrubTabBehaviorForWebUIUntrusted) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(nullptr,
                                            mojom::ContextType::kUntrustedWebUi,
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

class ChromeExtensionNavigationTest : public ExtensionServiceTestBase {
 public:
  ChromeExtensionNavigationTest() = default;

  ChromeExtensionNavigationTest(const ChromeExtensionNavigationTest&) = delete;
  ChromeExtensionNavigationTest& operator=(
      const ChromeExtensionNavigationTest&) = delete;

  void SetUp() override;
};

void ChromeExtensionNavigationTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeExtensionServiceWithUpdater();
}

TEST_F(ChromeExtensionNavigationTest, PrepareURLForNavigation) {
  auto extension = ExtensionBuilder("test").Build();
  // A fully qualified URL should return the same URL.
  {
    const std::string kTestUrl("http://google.com");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kTestUrl, extension.get(), browser_context());
    EXPECT_THAT(url, base::test::ValueIs(GURL(kTestUrl)));
  }
  // A relative path should return a URL relative to the extension's base URL.
  {
    const std::string kTestPath("foo");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kTestPath, extension.get(), browser_context());
    EXPECT_THAT(url, base::test::ValueIs(extension->GetResourceURL(kTestPath)));
  }
  // A kill URL should return false and set the error. There are several
  // different potential kill URLs and this just checks one of them.
  {
    const std::string kKillURL("chrome://crash");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kKillURL, extension.get(), browser_context());
    EXPECT_THAT(url,
                base::test::ErrorIs(ExtensionTabUtil::kNoCrashBrowserError));
  }
  // Hang URLs and other similar debug urls should also return false and set the
  // error.
  {
    const std::string kHangURL("chrome://hang");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kHangURL, extension.get(), browser_context());
    ASSERT_FALSE(url.has_value());
    EXPECT_EQ(ExtensionTabUtil::kNoCrashBrowserError, url.error());
  }
  // JavaScript URLs should return false and set the error.
  {
    const std::string kJavaScriptURL("javascript:alert('foo');");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kJavaScriptURL, extension.get(), browser_context());
    ASSERT_FALSE(url.has_value());
    EXPECT_EQ(ExtensionTabUtil::kJavaScriptUrlsNotAllowedInExtensionNavigations,
              url.error());
  }
  // File URLs should return false and set the error.
  {
    const std::string kFileURL("file:///etc/passwd");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kFileURL, extension.get(), browser_context());
    ASSERT_FALSE(url.has_value());
    EXPECT_EQ(ExtensionTabUtil::kFileUrlsNotAllowedInExtensionNavigations,
              url.error());
  }
  // File URLs with view-source scheme should return false and set the error.
  {
    const std::string kViewSourceFileURL("view-source:file:///etc/passwd");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kViewSourceFileURL, extension.get(), browser_context());
    ASSERT_FALSE(url.has_value());
    EXPECT_EQ(ExtensionTabUtil::kFileUrlsNotAllowedInExtensionNavigations,
              url.error());
  }
  // File URLs are returned when the extension has access to file.
  {
    util::SetAllowFileAccess(extension->id(), browser_context(), true);
    const std::string kFileURLWithAccess("file:///etc/passwd");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kFileURLWithAccess, extension.get(), browser_context());
    EXPECT_THAT(url, base::test::ValueIs(GURL(kFileURLWithAccess)));
  }
  // Regression test for crbug.com/1487908. Ensure that file URLs are returned
  // when the call originates from non-extension contexts (e.g. WebUI contexts).
  {
    const std::string kFileURL("file:///etc/passwd");
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kFileURL, /*extension=*/nullptr, browser_context());
    EXPECT_THAT(url, base::test::ValueIs(GURL(kFileURL)));
  }
}

TEST_F(ChromeExtensionNavigationTest,
       PrepareURLForNavigationWithEnterprisePolicy) {
  // Set the extension to allow file URL navigation via enterprise policy.
  std::string extension_id = "abcdefghijklmnopabcdefghijklmnop";
  std::string json = base::StringPrintf(
      R"({
        "%s": {
          "file_url_navigation_allowed": true
        }
      })",
      extension_id.c_str());

  std::optional<base::Value> settings = base::JSONReader::Read(json);
  testing_pref_service()->SetManagedPref(
      pref_names::kExtensionManagement,
      base::Value::ToUniquePtrValue(std::move(settings.value())));

  auto extension = ExtensionBuilder("test").SetID(extension_id).Build();

  // File URLs are returned when the extension has access to file.
  const std::string kFileURLWithEnterprisePolicy("file:///etc/passwd");
  auto url = ExtensionTabUtil::PrepareURLForNavigation(
      kFileURLWithEnterprisePolicy, extension.get(), browser_context());
  EXPECT_THAT(url, base::test::ValueIs(GURL(kFileURLWithEnterprisePolicy)));
}

TEST_F(ChromeExtensionNavigationTest, PrepareURLForNavigationWithPDFViewer) {
  // Set ID for PDF viewer extension.
  auto extension =
      ExtensionBuilder("test").SetID(extension_misc::kPdfExtensionId).Build();

  // File URLs are returned when the extension has access to file.
  const std::string kFileURLWithPDFViewer("file:///etc/passwd");
  auto url = ExtensionTabUtil::PrepareURLForNavigation(
      kFileURLWithPDFViewer, extension.get(), browser_context());
  EXPECT_THAT(url, base::test::ValueIs(GURL(kFileURLWithPDFViewer)));
}

TEST_F(ChromeExtensionNavigationTest, PrepareURLForNavigationOnDevtools) {
  const std::string kDevtoolsURL(
      "devtools://devtools/bundled/devtools_app.html");
  // A devtools url should return false and set the error.
  {
    auto no_permission_extension = ExtensionBuilder("none").Build();
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kDevtoolsURL, no_permission_extension.get(), browser_context());
    EXPECT_THAT(
        url, base::test::ErrorIs(ExtensionTabUtil::kCannotNavigateToDevtools));
  }
  // Having the devtools permissions should allow access.
  {
    auto devtools_extension = ExtensionBuilder("devtools")
                                  .SetManifestKey("devtools_page", "foo.html")
                                  .Build();
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kDevtoolsURL, devtools_extension.get(), browser_context());
    EXPECT_THAT(url, base::test::ValueIs(kDevtoolsURL));
  }
  // Having the debugger permissions should also allow access.
  {
    auto debugger_extension =
        ExtensionBuilder("debugger").AddAPIPermission("debugger").Build();
    auto url = ExtensionTabUtil::PrepareURLForNavigation(
        kDevtoolsURL, debugger_extension.get(), browser_context());
    EXPECT_THAT(url, base::test::ValueIs(kDevtoolsURL));
  }
}

TEST_F(ChromeExtensionNavigationTest,
       PrepareURLForNavigationOnChromeUntrusted) {
  const std::string kChromeUntrustedURL("chrome-untrusted://terminal/");
  auto extension = ExtensionBuilder("none").Build();
  auto url = ExtensionTabUtil::PrepareURLForNavigation(
      kChromeUntrustedURL, extension.get(), browser_context());
  EXPECT_THAT(url, base::test::ErrorIs(
                       ExtensionTabUtil::kCannotNavigateToChromeUntrusted));
}

}  // namespace extensions
