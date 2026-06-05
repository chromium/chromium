// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/glic_private/glic_private_api.h"
#include "chrome/browser/extensions/api/glic_private/glic_private_api_test_base.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "url/gurl.h"

namespace extensions {

class GlicPrivateApiTest : public GlicPrivateApiTestBase {
 public:
  GlicPrivateApiTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{extensions_features::kApiGlicPrivate, {}},
         {extensions_features::kApiGlicAccessFromGoogleWebpage, {}},
         {extensions_features::kApiGlicAccessFromPromotionPage, {}},
         {features::kGlicActor,
          {{"glic_actor_policy_control_exemption", "true"}}}},
        {{features::kGlicShowForSignedOut}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicPrivateApiFullyEnabledTest
    : public glic::GlicBrowserTestMixin<GlicPrivateApiTest> {
 public:
  void SetUpOnMainThread() override {
    GlicPrivateApiTest::SetUpOnMainThread();
    SetupIdentityAndCapabilities();
  }
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest, GetState) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(),
                                                         /*create=*/true);
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetUserEnabledActuationOnWeb(true);

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "fully_enabled"},
      {.load_as_component = true}))
      << message_;
}

class GlicPrivateApiDisabledTest : public GlicPrivateApiTest {
 private:
  glic::GlicTestEnvironment glic_test_environment_{
      {.force_signin_and_glic_capability = false}};
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiDisabledTest, GetState) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);
  EXPECT_TRUE(RunExtensionTest(
      "glic_private", {.extension_url = "test.html", .custom_arg = "disabled"},
      {.load_as_component = true}))
      << message_;
}

class GlicPrivateApiNotReadyTest : public GlicPrivateApiTest {
 public:
  void SetUpOnMainThread() override {
    GlicPrivateApiTest::SetUpOnMainThread();
    SetupIdentityAndCapabilities();
  }

 private:
  glic::GlicTestEnvironment glic_test_environment_{
      {.fre_status = glic::prefs::FreStatus::kNotStarted}};
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiNotReadyTest, GetState) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);
  EXPECT_TRUE(RunExtensionTest(
      "glic_private", {.extension_url = "test.html", .custom_arg = "not_ready"},
      {.load_as_component = true}))
      << message_;
}

class GlicPrivateApiFeatureDisabledTest : public GlicPrivateApiTestBase {
 public:
  GlicPrivateApiFeatureDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {extensions_features::kApiGlicPrivate, features::kGlicActor});
  }

 private:
  glic::GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFeatureDisabledTest, GetState) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);
  // The API should be undefined when the feature is disabled.
  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "feature_disabled"},
      {.load_as_component = true}))
      << message_;
}

// Invoke is not supported in Android yet.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseInterceptor();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private", {.extension_url = "test.html", .custom_arg = "invoke"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest,
                       DISABLED_InvokeInNewTab) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseInterceptor();

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_new_tab"},
      {.load_as_component = true}))
      << message_;

  // Verify that at least one new tab was created.
  // Note: The test may run twice (in service worker and page), opening 2 tabs.
  EXPECT_GE(GetTabListInterface()->GetTabCount(), initial_tab_count + 1);

  // Verify that the active tab is the new tab page.
  content::WebContents* active_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  content::WaitForLoadStop(active_contents);
  EXPECT_EQ(active_contents->GetLastCommittedURL(),
            chrome::ChromeUINewTabURLAsGURL());
}

class GlicPrivateApiNewTabInBackgroundTest
    : public GlicPrivateApiFullyEnabledTest {
 public:
  GlicPrivateApiNewTabInBackgroundTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        extensions_features::kApiGlicAccessFromGoogleWebpage,
        {{"glic_open_new_tab_disposition", "background"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiNewTabInBackgroundTest,
                       InvokeInNewTabBackground) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseInterceptor();

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_new_tab"},
      {.load_as_component = true}))
      << message_;

  // Verify that at least one new tab was created.
  EXPECT_GE(GetTabListInterface()->GetTabCount(), initial_tab_count + 1);

  // Verify that the active tab is not the new tab page.
  content::WebContents* active_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  content::WaitForLoadStop(active_contents);
  EXPECT_NE(active_contents->GetLastCommittedURL(),
            chrome::ChromeUINewTabURLAsGURL());
}

class GlicPrivateApiNewTabForegroundIfNotConsentedTest
    : public GlicPrivateApiFullyEnabledTest {
 public:
  GlicPrivateApiNewTabForegroundIfNotConsentedTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        extensions_features::kApiGlicAccessFromGoogleWebpage,
        {{"glic_open_new_tab_disposition", "foreground_if_not_consented"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiNewTabForegroundIfNotConsentedTest,
                       InvokeInNewTabBackgroundWhenActuationEnabled) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseInterceptor();

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(),
                                                         /*create=*/true);
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetUserEnabledActuationOnWeb(true);

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_new_tab"},
      {.load_as_component = true}))
      << message_;

  // Verify that at least one new tab was created.
  EXPECT_GE(GetTabListInterface()->GetTabCount(), initial_tab_count + 1);

  // Verify that the active tab is not the new tab page.
  content::WebContents* active_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  content::WaitForLoadStop(active_contents);
  EXPECT_NE(active_contents->GetLastCommittedURL(),
            chrome::ChromeUINewTabURLAsGURL());
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiNewTabForegroundIfNotConsentedTest,
                       InvokeInNewTabForegroundWhenActuationDisabled) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseInterceptor();

  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile(),
                                                         /*create=*/true);
  ASSERT_TRUE(glic_service);
  glic_service->enabling().SetUserEnabledActuationOnWeb(false);

  int initial_tab_count = GetTabListInterface()->GetTabCount();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_new_tab"},
      {.load_as_component = true}))
      << message_;

  // Verify that at least one new tab was created.
  EXPECT_GE(GetTabListInterface()->GetTabCount(), initial_tab_count + 1);

  // Verify that the active tab is the new tab page.
  content::WebContents* active_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  content::WaitForLoadStop(active_contents);
  EXPECT_EQ(active_contents->GetLastCommittedURL(),
            chrome::ChromeUINewTabURLAsGURL());
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiDisabledTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);
  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_disabled"},
      {.load_as_component = true}))
      << message_;
}

class GlicPrivateApiUniversalCartOnlyTest
    : public glic::GlicBrowserTestMixin<GlicPrivateApiTest> {
 public:
  GlicPrivateApiUniversalCartOnlyTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{extensions_features::kApiGlicPrivate, {}},
         {extensions_features::kApiGlicAccessFromGoogleWebpage, {}},
         {features::kGlicActor,
          {{"glic_actor_policy_control_exemption", "true"}}}},
        {extensions_features::kApiGlicAccessFromPromotionPage,
         features::kGlicShowForSignedOut});
  }

  void SetUpOnMainThread() override {
    GlicPrivateApiTest::SetUpOnMainThread();
    SetupIdentityAndCapabilities();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiUniversalCartOnlyTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseInterceptor();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "universal_cart_only"},
      {.load_as_component = true}))
      << message_;
}

class GlicPrivateApiPromotionPageOnlyTest
    : public glic::GlicBrowserTestMixin<GlicPrivateApiTest> {
 public:
  GlicPrivateApiPromotionPageOnlyTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{extensions_features::kApiGlicPrivate, {}},
         {extensions_features::kApiGlicAccessFromPromotionPage, {}},
         {features::kGlicActor,
          {{"glic_actor_policy_control_exemption", "true"}}}},
        {extensions_features::kApiGlicAccessFromGoogleWebpage,
         features::kGlicShowForSignedOut});
  }

  void SetUpOnMainThread() override {
    GlicPrivateApiTest::SetUpOnMainThread();
    SetupIdentityAndCapabilities();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiPromotionPageOnlyTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseInterceptor();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "promotion_page_only"},
      {.load_as_component = true}))
      << message_;
}

class GlicPrivateApiBothAccessDisabledTest
    : public glic::GlicBrowserTestMixin<GlicPrivateApiTest> {
 public:
  GlicPrivateApiBothAccessDisabledTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{extensions_features::kApiGlicPrivate, {}},
         {features::kGlicActor,
          {{"glic_actor_policy_control_exemption", "true"}}}},
        {extensions_features::kApiGlicAccessFromGoogleWebpage,
         extensions_features::kApiGlicAccessFromPromotionPage,
         features::kGlicShowForSignedOut});
  }

  void SetUpOnMainThread() override {
    GlicPrivateApiTest::SetUpOnMainThread();
    SetupIdentityAndCapabilities();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiBothAccessDisabledTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseInterceptor();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "both_access_disabled"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFeatureDisabledTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);
  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_feature_disabled"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiNotReadyTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseInterceptor();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_not_ready"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest, InvokeServerErrors) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseInterceptor();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "invoke_server_error"},
      {.load_as_component = true}))
      << message_;
}

#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest, HasConversation) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "has_conversation_false"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest, HasConversation_True) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  RegisterConversation(instance, "test_conversation_id");

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "has_conversation_true"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest,
                       HasConversation_Released) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  // Create a second tab and activate it.
  tabs::TabInterface* second_tab = CreateAndActivateTab(
      embedded_test_server()->GetURL("example.com", "/simple.html"));

  // Open Glic for this new active tab and register the conversation.
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  RegisterConversation(instance, "test_conversation_id");

  // Verify it is present.
  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "has_conversation_true"},
      {.load_as_component = true}))
      << message_;

  // Close the second tab.
  GetTabListInterface()->CloseTab(second_tab->GetHandle());

  // Verify that it is no longer present.
  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "has_conversation_false"},
      {.load_as_component = true}))
      << message_;
}

}  // namespace extensions
