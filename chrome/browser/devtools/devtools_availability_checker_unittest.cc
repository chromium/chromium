// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_availability_checker.h"

#include "build/build_config.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/web_app.h"
#include "components/webapps/common/web_app_id.h"
#endif

class DevToolsAvailabilityCheckerTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
  }

  void TearDown() override {
    web_contents_.reset();
    profile_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(DevToolsAvailabilityCheckerTest, UrlAllowedByPolicy) {
  base::ListValue allowlist;
  allowlist.Append("https://allowed.com/page");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://allowed.com/page"));
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, UrlBlockedByPolicy) {
  base::ListValue blocklist;
  blocklist.Append("blocked.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://blocked.com/panel"));
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, AllowlistTakesPrecedence) {
  base::ListValue allowlist;
  allowlist.Append("example.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  base::ListValue blocklist;
  blocklist.Append("example.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/page"));

  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       UrlAllowedWhenNotOnAllowlistNorBlocklist) {
  base::ListValue allowlist;
  allowlist.Append("allowed.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  base::ListValue blocklist;
  blocklist.Append("blocked.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  // When an allowlist is set, blocklist is set, and the URL is not on either
  // list, so allowed.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/page"));
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       UrlBlockedWhenNotOnAllowlistButOnBlocklist) {
  base::ListValue allowlist;
  allowlist.Append("allowed.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  base::ListValue blocklist;
  blocklist.Append("example.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  // When an allowlist is set, blocklist is set, and the URL is on the blocklist
  // but not the allowlist, so blocked.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/page"));
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       UrlAllowedWhenNotOnBlocklistAndAllowlistIsEmpty) {
  base::ListValue blocklist;
  blocklist.Append("blocked.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  // No allowlist is set, so fallback to default behavior, which is to allow
  // URLs not on the blocklist.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/page"));
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(DevToolsAvailabilityCheckerTest,
       UrlBlockedWhenNotOnAllowlistAndBlocklistIsEmpty) {
  base::ListValue allowlist;
  allowlist.Append("allowed.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  // When an allowlist is set and the blocklist is empty, any URL not on the
  // allowlist is blocked.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/page"));
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(DevToolsAvailabilityCheckerTest, DeveloperToolsDisallowedByPolicy) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed));
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/page"));
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, ExtensionAllowedByPolicy) {
  base::ListValue allowlist;
  allowlist.Append("abc");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test Extension").SetID("abc").Build();
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), extension.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, ExtensionBlockedByPolicy) {
  base::ListValue blocklist;
  blocklist.Append("abc");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test Extension").SetID("abc").Build();
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), extension.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       ExtensionNeitherAllowlistedNorBlocklisted) {
  base::ListValue allowlist;
  allowlist.Append("a");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  base::ListValue blocklist;
  blocklist.Append("b");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test Extension").SetID("c").Build();
  // Default is allowed if not explicitly blocked or allowlisted.
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), extension.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, IsInspectionAllowedNullWebContents) {
  // Passing nullptr for WebContents should default to allowed.
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(),
                                  static_cast<content::WebContents*>(nullptr)));
}

TEST_F(DevToolsAvailabilityCheckerTest, IsInspectionAllowedNullExtension) {
  // Passing nullptr for Extension should default to allowed.
  EXPECT_TRUE(IsInspectionAllowed(
      profile_.get(), static_cast<extensions::Extension*>(nullptr)));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       DisallowedForNullExtensionButAllowlistIsNotEmpty) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed));

  base::ListValue allowlist;
  allowlist.Append("foo.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  EXPECT_TRUE(IsInspectionAllowed(
      profile_.get(), static_cast<extensions::Extension*>(nullptr)));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       DisallowedForNullExtensionAndAllowlistIsEmpty) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed));

  EXPECT_FALSE(IsInspectionAllowed(
      profile_.get(), static_cast<extensions::Extension*>(nullptr)));
}

TEST_F(DevToolsAvailabilityCheckerTest, NoPolicy_DefaultAllowed) {
  // By default, devtools are allowed.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/page"));
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

#if !BUILDFLAG(IS_ANDROID)

TEST_F(DevToolsAvailabilityCheckerTest, WebAppAllowedByPolicy) {
  base::ListValue allowlist;
  allowlist.Append("https://allowed-app.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  auto web_app = web_app::test::CreateWebApp(GURL("https://allowed-app.com"));
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_app.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, WebAppBlockedByPolicy) {
  base::ListValue blocklist;
  blocklist.Append("blocked-app.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  auto web_app = web_app::test::CreateWebApp(GURL("https://blocked-app.com/"));
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), web_app.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, WebAppDisallowedByPolicy) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kDisallowed));

  auto web_app = web_app::test::CreateWebApp(GURL("https://example.com/"));
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), web_app.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, WebAppAllowedWhenPolicyIsAllowed) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(
          policy::DeveloperToolsPolicyHandler::Availability::kAllowed));

  auto web_app = web_app::test::CreateWebApp(GURL("https://example.com/"));
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_app.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, IsInspectionAllowedNullWebApp) {
  // Passing nullptr for WebApp should default to allowed.
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(),
                                  static_cast<web_app::WebApp*>(nullptr)));
}
#endif  // !BUILDFLAG(IS_ANDROID)
