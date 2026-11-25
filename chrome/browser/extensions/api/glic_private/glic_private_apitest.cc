// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/extensions/api/glic_private/glic_private_api.h"
#include "chrome/browser/extensions/api/glic_private/glic_private_api_test_base.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/actor/core/task_source_info.h"
#include "components/prefs/pref_service.h"
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
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace extensions {

class GlicPrivateApiTest : public GlicPrivateApiTestBase {
 public:
  GlicPrivateApiTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{extensions_features::kApiGlicPrivate, {}},
         {extensions_features::kApiGlicAccessFromGoogleWebpage, {}},
         {extensions_features::kApiGlicAccessFromPromotionPage, {}},
         {features::kGlicAnchorEntryPointForOnboardedUsers, {}},
         {features::kGlicActor, {}},
         {features::kGlicActorPolicyControlExemption, {}}
#if BUILDFLAG(IS_CHROMEOS)
         ,
         {chromeos::features::kFeatureManagementGlic, {}}
#endif
        },
        /*disabled_features=*/{features::kGlicShowForSignedOut,
                               features::kGlicCountryFiltering,
                               features::kGlicLocaleFiltering});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicPrivateApiFullyEnabledTest
    : public glic::GlicBrowserTestMixin<GlicPrivateApiTest> {
 public:
  void SetUpOnMainThread() override {
    glic::GlicBrowserTestMixin<GlicPrivateApiTest>::SetUpOnMainThread();
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

class GlicPrivateApiIneligibleAccountTest
    : public glic::GlicBrowserTestMixin<GlicPrivateApiTest> {
 public:
  void SetUpOnMainThread() override {
    glic::GlicBrowserTestMixin<GlicPrivateApiTest>::SetUpOnMainThread();

    profile()->GetPrefs()->SetInteger(
        ::glic::prefs::kGlicCompletedFre,
        static_cast<int>(::glic::prefs::FreStatus::kCompleted));

    Profile* test_profile = profile();
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(test_profile);

    CoreAccountInfo primary_account =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    ASSERT_FALSE(primary_account.IsEmpty());

    AccountInfo account_info =
        identity_manager->FindExtendedAccountInfoByAccountId(
            primary_account.account_id);

    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_gemini_in_chrome(false);
    mutator.set_can_use_model_execution_features(false);
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiIneligibleAccountTest, GetState) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);
  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "ineligible_account"},
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
         {features::kGlicActor, {}},
         {features::kGlicActorPolicyControlExemption, {}}},
        {extensions_features::kApiGlicAccessFromPromotionPage,
         features::kGlicShowForSignedOut});
  }

  void SetUpOnMainThread() override {
    glic::GlicBrowserTestMixin<GlicPrivateApiTest>::SetUpOnMainThread();
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

IN_PROC_BROWSER_TEST_F(GlicPrivateApiUniversalCartOnlyTest,
                       InvokeWithMetadata) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseWithMetadataInterceptor();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "universal_cart_only"},
      {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiUniversalCartOnlyTest,
                       InvokeWithMetadataBase64Error) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseWithMetadataInterceptor(
      "Mock prompt", "invalid_base64_!!!");

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
         {features::kGlicActor, {}},
         {features::kGlicActorPolicyControlExemption, {}}},
        {extensions_features::kApiGlicAccessFromGoogleWebpage,
         features::kGlicShowForSignedOut});
  }

  void SetUpOnMainThread() override {
    glic::GlicBrowserTestMixin<GlicPrivateApiTest>::SetUpOnMainThread();
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
         {features::kGlicActor, {}},
         {features::kGlicActorPolicyControlExemption, {}}},
        {extensions_features::kApiGlicAccessFromGoogleWebpage,
         extensions_features::kApiGlicAccessFromPromotionPage,
         features::kGlicShowForSignedOut});
  }

  void SetUpOnMainThread() override {
    glic::GlicBrowserTestMixin<GlicPrivateApiTest>::SetUpOnMainThread();
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

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest,
                       HasActiveActuationTab_FalseWithPassiveGlicInstance) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  RegisterConversation(instance, "test_conversation_id");

  // Passive side panel registration without an active `ActorTask` returns
  // false. Omit `extension_url` so only the background service worker runs
  // `test.js`.
  EXPECT_TRUE(RunExtensionTest("glic_private",
                               {.custom_arg = "has_active_actuation_tab_false"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest,
                       HasActiveActuationTab_True) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  tabs::TabInterface* actuation_tab =
      CreateAndActivateTab(GURL(url::kAboutBlankURL));
  ASSERT_TRUE(actuation_tab);

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic,
                            "test_conversation_id"),
      actor::NoEnterprisePolicyChecker());
  actor::ActorTask* task = actor_service->GetTask(task_id);
  ASSERT_TRUE(task);
  actor::AddTabToTask(*actuation_tab, *task);
  // Verify paused (waiting-for-user) tasks still report as active even without
  // an open `GlicInstance`.
  task->Pause(/*from_actor=*/true);

  EXPECT_TRUE(RunExtensionTest("glic_private",
                               {.custom_arg = "has_active_actuation_tab_true"},
                               {.load_as_component = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(GlicPrivateApiFullyEnabledTest,
                       HasActiveActuationTab_Released) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  tabs::TabInterface* actuation_tab =
      CreateAndActivateTab(GURL(url::kAboutBlankURL));
  ASSERT_TRUE(actuation_tab);

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile());
  ASSERT_TRUE(actor_service);
  actor::TaskId task_id = actor_service->CreateTask(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kGlic,
                            "test_conversation_id"),
      actor::NoEnterprisePolicyChecker());
  actor::ActorTask* task = actor_service->GetTask(task_id);
  ASSERT_TRUE(task);
  actor::AddTabToTask(*actuation_tab, *task);
  // Pause so the task exits active actor control and `ActorTaskUnloadHandler`
  // does not intercept `CloseTab()` with a confirmation dialog, while still
  // keeping the task active until the tab is closed.
  task->Pause(/*from_actor=*/true);

  // Coordinate the before/after assertions in a single service-worker-only
  // extension load to avoid racing back-to-back `RunExtensionTest()` calls.
  SetCustomArg("has_active_actuation_tab_released");
  ResultCatcher catcher;
  ExtensionTestMessageListener close_tab_listener("close_actuation_tab",
                                                  ReplyBehavior::kWillReply);
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("glic_private"),
                            {.load_as_component = true}));

  {
    SCOPED_TRACE("waiting for extension to request closing actuation tab");
    ASSERT_TRUE(close_tab_listener.WaitUntilSatisfied());
  }

  // Close the actuated tab and wait for tab teardown to complete.
  {
    SCOPED_TRACE("waiting for actuation tab WebContents destruction");
    content::WebContentsDestroyedWatcher destroyed_watcher(
        actuation_tab->GetContents());
    GetTabListInterface()->CloseTab(actuation_tab->GetHandle());
    destroyed_watcher.Wait();
  }

  close_tab_listener.Reply("closed");
  {
    SCOPED_TRACE("waiting for extension test result after tab close");
    EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  }
}

#if !BUILDFLAG(IS_ANDROID)
class GlicPrivateApiActuationDisabledTest
    : public glic::GlicBrowserTestMixin<GlicPrivateApiTestBase> {
 public:
  GlicPrivateApiActuationDisabledTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{extensions_features::kApiGlicPrivate, {}},
         {extensions_features::kApiGlicAccessFromGoogleWebpage, {}},
         {extensions_features::kApiGlicAccessFromPromotionPage, {}}},
        {{features::kGlicActor, features::kGlicShowForSignedOut}});
  }

  void SetUpOnMainThread() override {
    glic::GlicBrowserTestMixin<GlicPrivateApiTestBase>::SetUpOnMainThread();
    SetupIdentityAndCapabilities();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicPrivateApiActuationDisabledTest, Invoke) {
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      kGlicPrivateTestExtensionId);

  auto interceptor = CreateMockPromptResponseInterceptor();

  EXPECT_TRUE(RunExtensionTest(
      "glic_private",
      {.extension_url = "test.html", .custom_arg = "actuation_disabled"},
      {.load_as_component = true}))
      << message_;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace extensions
