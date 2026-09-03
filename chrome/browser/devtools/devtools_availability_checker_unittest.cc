// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_availability_checker.h"

#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
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

TEST_F(DevToolsAvailabilityCheckerTest, DeveloperToolsDisallowedByPolicy) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(
          policy::DeveloperToolsAvailability::kDisallowed));
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/page"));
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, IsInspectionAllowedNullWebContents) {
  // Passing nullptr for WebContents should default to allowed.
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(),
                                  static_cast<content::WebContents*>(nullptr)));
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
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

TEST_F(DevToolsAvailabilityCheckerTest, ExtensionNotOnAllowlistIsBlocked) {
  base::ListValue allowlist;
  allowlist.Append("allowed-extension-id");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test Extension").SetID("abc").Build();
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), extension.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, ExtensionDisallowedByGeneralPolicy) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::kDisallowed));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test Extension").SetID("abc").Build();
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), extension.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, ExtensionAllowlistPrecedence) {
  base::ListValue allowlist;
  allowlist.Append("abc");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  base::ListValue blocklist;
  blocklist.Append("abc");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test Extension").SetID("abc").Build();
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), extension.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       ExtensionForceInstalledDisallowedByPolicy) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::
                           kDisallowedForForceInstalledExtensions));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test Extension")
          .SetID("abc")
          .SetLocation(
              extensions::mojom::ManifestLocation::kExternalPolicyDownload)
          .Build();
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), extension.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, ExtensionForceInstalledButAllowlisted) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::
                           kDisallowedForForceInstalledExtensions));

  base::ListValue allowlist;
  allowlist.Append("abc");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test Extension")
          .SetID("abc")
          .SetLocation(
              extensions::mojom::ManifestLocation::kExternalPolicyDownload)
          .Build();
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), extension.get()));
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
          policy::DeveloperToolsAvailability::kDisallowed));

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
          policy::DeveloperToolsAvailability::kDisallowed));

  EXPECT_FALSE(IsInspectionAllowed(
      profile_.get(), static_cast<extensions::Extension*>(nullptr)));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

TEST_F(DevToolsAvailabilityCheckerTest, NoPolicy_DefaultAllowed) {
  // By default, devtools are allowed.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/page"));
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, SubframeBlockedByBlocklistPolicy) {
  base::ListValue blocklist;
  blocklist.Append("blocked.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://allowed.com/page"));
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://blocked.com/iframe"), subframe);

  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, SubframeAllowlistPrecedence) {
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
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/iframe"), subframe);

  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       NestedSubframeBlockedByBlocklistPolicy) {
  base::ListValue blocklist;
  blocklist.Append("blocked.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://allowed.com/page"));
  content::RenderFrameHost* subframe1 =
      content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
          ->AppendChild("subframe1");
  content::RenderFrameHostTester::For(subframe1)
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://allowed.com/subframe"), subframe1);

  content::RenderFrameHost* subframe2 =
      content::RenderFrameHostTester::For(subframe1)->AppendChild("subframe2");
  content::RenderFrameHostTester::For(subframe2)
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://blocked.com/iframe"), subframe2);

  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, AllSubframesAllowed) {
  base::ListValue blocklist;
  blocklist.Append("blocked.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://allowed.com/page"));
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://allowed.com/iframe"), subframe);

  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, SubframeNotOnAllowlistIsBlocked) {
  base::ListValue allowlist;
  allowlist.Append("allowed.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://allowed.com/page"));
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://other.com/iframe"), subframe);

  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       SubframeWithAboutBlankAllowedWhenMainFrameAllowed) {
  base::ListValue allowlist;
  allowlist.Append("allowed.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://allowed.com/page"));
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("about:blank"), subframe);

  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       DisallowedByGeneralPolicy_AllPagesAndSubframesBlocked) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::kDisallowed));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/page"));
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com/iframe"), subframe);

  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       DisallowedByGeneralPolicy_MainFrameBlockedEvenWithAllowlistedIframe) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::kDisallowed));

  base::ListValue allowlist;
  allowlist.Append("allowed.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                                std::move(allowlist));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/page"));
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://allowed.com/iframe"), subframe);

  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
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
          policy::DeveloperToolsAvailability::kDisallowed));

  auto web_app = web_app::test::CreateWebApp(GURL("https://example.com/"));
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), web_app.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, WebAppAllowedWhenPolicyIsAllowed) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(
          policy::DeveloperToolsAvailability::kAllowed));

  auto web_app = web_app::test::CreateWebApp(GURL("https://example.com/"));
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_app.get()));
}

TEST_F(DevToolsAvailabilityCheckerTest, IsInspectionAllowedNullWebApp) {
  // Passing nullptr for WebApp should default to allowed.
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(),
                                  static_cast<web_app::WebApp*>(nullptr)));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       PolicyInstalledIwaServiceWorkerDisallowedByPolicy) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::
                           kDisallowedForForceInstalledExtensions));

  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());

  const GURL iwa_url(
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/");
  base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
      web_app::IsolatedWebAppUrlInfo::Create(iwa_url);
  ASSERT_TRUE(url_info.has_value());

  auto web_app = web_app::test::CreateWebApp(
      url_info->origin().GetURL(), web_app::WebAppManagement::kIwaPolicy);

  auto* fake_provider = web_app::FakeWebAppProvider::Get(profile_.get());
  fake_provider->GetRegistrarMutable().registry().emplace(url_info->app_id(),
                                                          std::move(web_app));

  const GURL sw_url(
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/sw.js");
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), sw_url));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       UserInstalledIwaServiceWorkerAllowedByPolicy) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::
                           kDisallowedForForceInstalledExtensions));

  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());

  const GURL iwa_url(
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/");
  base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
      web_app::IsolatedWebAppUrlInfo::Create(iwa_url);
  ASSERT_TRUE(url_info.has_value());

  auto web_app =
      web_app::test::CreateWebApp(url_info->origin().GetURL(),
                                  web_app::WebAppManagement::kIwaUserInstalled);

  auto* fake_provider = web_app::FakeWebAppProvider::Get(profile_.get());
  fake_provider->GetRegistrarMutable().registry().emplace(url_info->app_id(),
                                                          std::move(web_app));

  const GURL sw_url(
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/sw.js");
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), sw_url));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       UninstalledIwaServiceWorkerAllowedByPolicy) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::
                           kDisallowedForForceInstalledExtensions));

  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());

  const GURL sw_url(
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/sw.js");
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), sw_url));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       PolicyInstalledIwaServiceWorkerAllowedWhenDevToolsAllowed) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::kAllowed));

  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());

  const GURL iwa_url(
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/");
  base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
      web_app::IsolatedWebAppUrlInfo::Create(iwa_url);
  ASSERT_TRUE(url_info.has_value());

  auto web_app = web_app::test::CreateWebApp(
      url_info->origin().GetURL(), web_app::WebAppManagement::kIwaPolicy);

  auto* fake_provider = web_app::FakeWebAppProvider::Get(profile_.get());
  fake_provider->GetRegistrarMutable().registry().emplace(url_info->app_id(),
                                                          std::move(web_app));

  const GURL sw_url(
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/sw.js");
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), sw_url));
}

TEST_F(DevToolsAvailabilityCheckerTest,
       PolicyInstalledIwaServiceWorkerDisallowedWhenDevToolsDisallowed) {
  profile_->GetPrefs()->SetInteger(
      prefs::kDevToolsAvailability,
      static_cast<int>(policy::DeveloperToolsAvailability::kDisallowed));

  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());

  const GURL iwa_url(
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/");
  base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
      web_app::IsolatedWebAppUrlInfo::Create(iwa_url);
  ASSERT_TRUE(url_info.has_value());

  auto web_app = web_app::test::CreateWebApp(
      url_info->origin().GetURL(), web_app::WebAppManagement::kIwaPolicy);

  auto* fake_provider = web_app::FakeWebAppProvider::Get(profile_.get());
  fake_provider->GetRegistrarMutable().registry().emplace(url_info->app_id(),
                                                          std::move(web_app));

  const GURL sw_url(
      "isolated-app://"
      "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/sw.js");
  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), sw_url));
}

#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(DevToolsAvailabilityCheckerTest, TargetLevelSubframeBlocked) {
  base::ListValue blocklist;
  blocklist.Append("blocked.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://allowed.com/page"));
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://blocked.com/iframe"), subframe);

  // The main page should still be inspectable.
  EXPECT_TRUE(IsInspectionAllowed(profile_.get(), web_contents_.get()));

  // But if we specifically check the blocked subframe's URL, it should be
  // blocked.
  EXPECT_FALSE(
      IsInspectionAllowed(profile_.get(), GURL("https://blocked.com/iframe")));
}

class DevToolsAvailabilityCheckerTargetLevelDisabledTest
    : public DevToolsAvailabilityCheckerTest {
 public:
  DevToolsAvailabilityCheckerTargetLevelDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kDevToolsTargetLevelEvaluation);
  }

  ~DevToolsAvailabilityCheckerTargetLevelDisabledTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DevToolsAvailabilityCheckerTargetLevelDisabledTest,
       SubframeBlockedByBlocklistPolicy) {
  base::ListValue blocklist;
  blocklist.Append("blocked.com");
  profile_->GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                                std::move(blocklist));

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://allowed.com/page"));
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(web_contents_->GetPrimaryMainFrame())
          ->AppendChild("subframe");
  content::RenderFrameHostTester::For(subframe)
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://blocked.com/iframe"), subframe);

  EXPECT_FALSE(IsInspectionAllowed(profile_.get(), web_contents_.get()));
}
