// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/config.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace {
constexpr char kEmail[] = "example@gmail.com";
constexpr char kExampleURL[] = "https://example.com";
using translate::testing::MockTranslateClient;

class MockTranslateLanguageProvider : public TranslateLanguageProvider {
 public:
  MOCK_METHOD(std::string,
              GetSourceLanguage,
              (translate::TranslateManager * translate_manager));
};

// Mock translate manager.  We need it for dependency injection.
class MockTranslateManager : public translate::TranslateManager {
 public:
  MOCK_METHOD(translate::LanguageState*, GetLanguageState, ());
  // Other methods are uninteresting, we don't want to mock them.  We use a
  // NiceMock so there are no warnings if other methods are called.

  explicit MockTranslateManager(translate::TranslateClient* translate_client)
      : TranslateManager(translate_client, nullptr, nullptr) {}
};

class CustomMockOptimizationGuideKeyedService
    : public MockOptimizationGuideKeyedService {
 public:
  CustomMockOptimizationGuideKeyedService() = default;
  ~CustomMockOptimizationGuideKeyedService() override = default;

  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL& url,
               optimization_guide::proto::OptimizationType optimization_type,
               optimization_guide::OptimizationGuideDecisionCallback callback));

  MOCK_METHOD(
      optimization_guide::OptimizationGuideDecision,
      CanApplyOptimization,
      (const GURL& url,
       optimization_guide::proto::OptimizationType optimization_type,
       optimization_guide::OptimizationMetadata* optimization_metadata));
  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&
                   optimization_types));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>& urls,
       const base::flat_set<optimization_guide::proto::OptimizationType>&
           optimization_types,
       optimization_guide::proto::RequestContext request_context,
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
           callback));
};

void RegisterMockOptimizationGuideKeyedServiceFactory(
    content::BrowserContext* context) {
  MockOptimizationGuideKeyedService::InitializeWithExistingTestLocalState();
  OptimizationGuideKeyedServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating([](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            testing::NiceMock<CustomMockOptimizationGuideKeyedService>>();
      }));
}

}  // namespace

class ComposeEnablingTest : public BrowserWithTestWindowTest {
 public:
  ComposeEnablingTest() {
    // Allows early registration of a override of the factory that instantiates
    // OptimizationGuideKeyedService.
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                RegisterMockOptimizationGuideKeyedServiceFactory));
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    scoped_feature_list_.InitWithFeatures(
        {compose::features::kEnableCompose,
         compose::features::kEnableComposeNudge},
        {});

    mock_translate_client_ =
        std::make_unique<MockTranslateClient>(&translate_driver_, nullptr);
    mock_translate_manager_ =
        std::make_unique<testing::NiceMock<MockTranslateManager>>(
            mock_translate_client_.get());

    // Note that AddTab makes its own ComposeEnabling as part of
    // ChromeComposeClient. This can cause confusion when debugging tests.
    // Don't confuse the two ComposeEnabling objects when debugging.
    AddTab(browser(), GURL(kExampleURL));
    context_menu_params_.is_content_editable_for_autofill = true;
    context_menu_params_.frame_origin = GetOrigin();

    opt_guide_ = static_cast<
        testing::NiceMock<CustomMockOptimizationGuideKeyedService>*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile()));
    ASSERT_TRUE(opt_guide_);

    // Build the ComposeEnabling object the tests will use, providing it with
    // mocks for its dependencies.
    // TODO(b/316625561) Simplify these tests more now that we have dependency
    // injection.
    compose_enabling_ = std::make_unique<ComposeEnabling>(
        &mock_translate_language_provider_, GetProfile(),
        identity_test_env_.identity_manager(), &opt_guide());
  }

  void TearDown() override {
    // We must destroy the ComposeEnabling while the opt_guide object is still
    // valid so we can call unregister in the destructor.
    compose_enabling_ = nullptr;
    // We must null out the opt_guide_ before calling ResetForTesting.
    opt_guide_ = nullptr;
    compose::ResetConfigForTesting();
    BrowserWithTestWindowTest::TearDown();
    MockOptimizationGuideKeyedService::ResetForTesting();
  }

  void SignIn(signin::ConsentLevel consent_level) {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail, consent_level);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  void SetMsbbState(bool new_state) {
    PrefService* prefs = GetProfile()->GetPrefs();
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        new_state);
  }

  CustomMockOptimizationGuideKeyedService& opt_guide() { return *opt_guide_; }

 protected:
  void SetLanguage(std::string lang) {
    ON_CALL(mock_translate_language_provider_, GetSourceLanguage(testing::_))
        .WillByDefault(Return(lang));
  }

  url::Origin GetOrigin() {
    return url::Origin::Create(browser()
                                   ->tab_strip_model()
                                   ->GetWebContentsAt(0)
                                   ->GetLastCommittedURL());
  }

  content::RenderFrameHost* GetRenderFrameHost() {
    return browser()
        ->tab_strip_model()
        ->GetWebContentsAt(0)
        ->GetPrimaryMainFrame();
  }

  void CheckIsEnabledError(ComposeEnabling* compose_enabling,
                           compose::ComposeShowStatus error_show_status) {
    EXPECT_EQ(compose_enabling->IsEnabled(),
              base::unexpected(error_show_status));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;

  content::ContextMenuParams context_menu_params_;

  base::CallbackListSubscription subscription_;
  raw_ptr<testing::NiceMock<CustomMockOptimizationGuideKeyedService>>
      opt_guide_;

  translate::testing::MockTranslateDriver translate_driver_;
  std::unique_ptr<MockTranslateClient> mock_translate_client_;
  std::unique_ptr<testing::NiceMock<MockTranslateManager>>
      mock_translate_manager_;

  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider_;

  std::unique_ptr<ComposeEnabling> compose_enabling_;
};

TEST_F(ComposeEnablingTest, EverythingDisabledTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {}, {compose::features::kEnableCompose,
           compose::features::kEnableComposeNudge});
  // We intentionally don't call sign in to make our state not signed in.
  SetMsbbState(false);
  EXPECT_NE(compose_enabling_->IsEnabled(), base::ok());
}

TEST_F(ComposeEnablingTest, FeatureNotEnabledTest) {
  // Ensure feature flag is off.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {}, {compose::features::kEnableCompose,
           compose::features::kEnableComposeNudge});
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // Turn on MSBB.
  SetMsbbState(true);

  CheckIsEnabledError(compose_enabling_.get(),
                      compose::ComposeShowStatus::kGenericBlocked);
}

TEST_F(ComposeEnablingTest, MsbbDisabledTest) {
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // MSBB turned off.
  SetMsbbState(false);
  CheckIsEnabledError(compose_enabling_.get(),
                      compose::ComposeShowStatus::kDisabledMsbb);
}

TEST_F(ComposeEnablingTest, NotSignedInTest) {
  // Intentionally skip the signin step.
  // Turn on MSBB.
  SetMsbbState(true);
  CheckIsEnabledError(compose_enabling_.get(),
                      compose::ComposeShowStatus::kSignedOut);
}

TEST_F(ComposeEnablingTest, SignedInErrorTest) {
  // Sign in, with error.
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSync);
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  // Turn on MSBB.
  SetMsbbState(true);
  CheckIsEnabledError(compose_enabling_.get(),
                      compose::ComposeShowStatus::kSignedOut);
}

TEST_F(ComposeEnablingTest, ComposeEligibleTest) {
  scoped_feature_list_.Reset();
  // Turn on the enable switch and off the eligible switch.
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose,
       compose::features::kEnableComposeNudge},
      {compose::features::kComposeEligible});
  ComposeEnabling::SkipUserEnabledCheckForTesting(true);
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // Turn on MSBB.
  SetMsbbState(true);

  // The ComposeEligible switch should win, and disable the feature.
  CheckIsEnabledError(compose_enabling_.get(),
                      compose::ComposeShowStatus::kNotComposeEligible);
}

TEST_F(ComposeEnablingTest, EverythingEnabledTest) {
  ComposeEnabling::SkipUserEnabledCheckForTesting(true);
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // Turn on MSBB.
  SetMsbbState(true);
  EXPECT_EQ(compose_enabling_->IsEnabled(), base::ok());
}

TEST_F(ComposeEnablingTest, StaticMethodEverythingDisabledTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {}, {compose::features::kEnableCompose,
           compose::features::kEnableComposeNudge});
  // We intentionally don't call sign in to make our state not signed in.
  SetMsbbState(false);
  EXPECT_FALSE(ComposeEnabling::IsEnabledForProfile(GetProfile()));
}

TEST_F(ComposeEnablingTest, StaticMethodFlagsNotEnabledTest) {
  scoped_feature_list_.Reset();
  EXPECT_FALSE(ComposeEnabling::IsEnabledForProfile(GetProfile()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuDisabledTest) {
  // We intentionally disable the feature.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {}, {compose::features::kEnableCompose,
           compose::features::kEnableComposeNudge});

  // Set the language to something we support. "en" is English.
  SetLanguage("en");

  EXPECT_FALSE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(),
      /*rfh=*/GetRenderFrameHost(), context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuLanguageTest) {
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);

  // Set the mock language to something we don't support. "eo" is Esperanto.
  SetLanguage("eo");

  EXPECT_FALSE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuLanguageBypassTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose,
       compose::features::kEnableComposeLanguageBypass},
      {});
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);

  // Set the mock language to something we don't support. "eo" is Esperanto.
  SetLanguage("eo");

  // Although the language is unsupported, ShouldTrigger should return true as
  // the bypass is enabled.
  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuEmptyLangugeTest) {
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);

  // Set the language to the empty string - translate doesn't have the answer
  // yet.
  SetLanguage("");

  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuUndeterminedLangugeTest) {
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);

  // Set the language to the "und" for a page where translate could not
  // determine the language.
  SetLanguage("und");

  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuFieldTypeTest) {
  // Set ContextMenuParams to non-contenteditable and non-textarea, which we do
  // not support.
  context_menu_params_.is_content_editable_for_autofill = false;
  context_menu_params_.form_control_type =
      blink::mojom::FormControlType::kInputButton;

  // Set the language to something we support. Not expected to be called.
  SetLanguage("en");

  EXPECT_FALSE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest,
       ShouldTriggerContextMenuAllEnabledContentEditableTest) {
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);

  // Set the language to something we support.
  SetLanguage("en");

  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuAllEnabledTextAreaTest) {
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);

  // Set the language to something we support.
  SetLanguage("en");

  // Set ContextMenuParams to textarea, which we support.
  context_menu_params_.is_content_editable_for_autofill = false;
  context_menu_params_.form_control_type =
      blink::mojom::FormControlType::kTextArea;

  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupDisabledTest) {
  // We intentionally disable the feature.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {}, {compose::features::kEnableCompose,
           compose::features::kEnableComposeNudge});

  std::string autocomplete_attribute;
  bool has_saved_state = false;

  // Set the language to something we support. "en" is English.
  SetLanguage("en");

  EXPECT_FALSE(compose_enabling_->ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), GetOrigin(), GURL(kExampleURL)));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupLanguageTest) {
  // Enable the feature.
  ComposeEnabling::SetEnabledForTesting(true);
  std::string autocomplete_attribute;
  bool has_saved_state = false;

  // Set the mock language to something we don't support. "eo" is Esperanto.
  SetLanguage("eo");

  EXPECT_FALSE(compose_enabling_->ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), GetOrigin(), GURL(kExampleURL)));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupLanguageBypassTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose,
       compose::features::kEnableComposeNudge,
       compose::features::kEnableComposeLanguageBypass},
      {});

  // Enable the feature.
  ComposeEnabling::SetEnabledForTesting(true);
  std::string autocomplete_attribute;
  bool has_saved_state = true;

  // Set the mock language to something we don't support. "eo" is Esperanto.
  // Not expected to be called.
  SetLanguage("eo");

  // Although the language is unsupported, ShouldTrigger should return true as
  // the bypass is enabled.
  EXPECT_TRUE(compose_enabling_->ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), GetOrigin(), GURL(kExampleURL)));
}

TEST_F(ComposeEnablingTest, ShouldNotTriggerProactivePopupAutocompleteOffTest) {
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);
  // Autocomplete is set to off for this page.
  std::string autocomplete_attribute("off");
  bool has_saved_state = false;

  // Set the language to something we support.
  SetLanguage("en");

  EXPECT_FALSE(compose_enabling_->ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), GetOrigin(), GURL(kExampleURL)));
}

TEST_F(ComposeEnablingTest, ShouldTriggerSavedStatePopupAutocompleteOffTest) {
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);
  // Autocomplete is set to off for this page.
  std::string autocomplete_attribute("off");
  bool has_saved_state = true;

  // Set the language to something we support.
  SetLanguage("en");

  EXPECT_TRUE(compose_enabling_->ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), GetOrigin(), GURL(kExampleURL)));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupWithSavedStateTest) {
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);
  std::string autocomplete_attribute;

  // test all variants of: popup with, popup without state.
  std::vector<std::pair<bool, bool>> tests = {
      {true, true}, {true, false}, {false, true}, {false, false}};

  for (auto [popup_with_state, popup_without_state] : tests) {
    compose::Config& config = compose::GetMutableConfigForTesting();
    config.popup_with_saved_state = popup_with_state;
    config.popup_with_no_saved_state = popup_without_state;

    SetLanguage("en");

    EXPECT_EQ(popup_with_state, compose_enabling_->ShouldTriggerPopup(
                                    autocomplete_attribute, GetProfile(),
                                    mock_translate_manager_.get(),
                                    /*has_saved_state=*/true, GetOrigin(),
                                    GetOrigin(), GURL(kExampleURL)));

    EXPECT_EQ(popup_without_state, compose_enabling_->ShouldTriggerPopup(
                                       autocomplete_attribute, GetProfile(),
                                       mock_translate_manager_.get(),
                                       /*has_saved_state=*/false, GetOrigin(),
                                       GetOrigin(), GURL(kExampleURL)));
  }
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupNudgeDisabledTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose},
      {compose::features::kEnableComposeNudge});

  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);
  std::string autocomplete_attribute;

  std::vector<std::pair<bool, bool>> tests = {
      // config: popup with, popup without. expect: trigger with, trigger
      // without.
      {true, true},
      {true, false},
      {false, true},
      {false, false}};

  for (auto [popup_with_state, popup_without_state] : tests) {
    compose::Config& config = compose::GetMutableConfigForTesting();
    config.popup_with_saved_state = popup_with_state;
    config.popup_with_no_saved_state = popup_without_state;

    SetLanguage("en");

    EXPECT_FALSE(compose_enabling_->ShouldTriggerPopup(
        autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
        /*has_saved_state=*/true, GetOrigin(), GetOrigin(), GURL(kExampleURL)));

    EXPECT_FALSE(compose_enabling_->ShouldTriggerPopup(
        autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
        /*has_saved_state=*/false, GetOrigin(), GetOrigin(),
        GURL(kExampleURL)));
  }
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupCrossOrigin) {
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);
  std::string autocomplete_attribute;
  bool has_saved_state = false;

  // Set the language to something we support.
  SetLanguage("en");
  EXPECT_FALSE(compose_enabling_->ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), url::Origin(), GURL(kExampleURL)));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuCrossOrigin) {
  base::HistogramTester histogram_tester;
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);

  // Set the language to something we support.
  SetLanguage("en");

  context_menu_params_.frame_origin = url::Origin();
  EXPECT_FALSE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));

  // Check that a response result OK metric was emitted.
  histogram_tester.ExpectUniqueSample(
      compose::kComposeShowStatus,
      compose::ComposeShowStatus::kFormFieldInCrossOriginFrame, 1);
}

TEST_F(ComposeEnablingTest, GetOptimizationGuidanceShowNudgeTest) {
  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  compose_hint_metadata.set_decision(
      compose::ComposeHintDecision::COMPOSE_HINT_DECISION_ENABLED);
  test_metadata.SetAnyMetadataForTesting(compose_hint_metadata);

  EXPECT_CALL(opt_guide(),
              CanApplyOptimization(
                  GURL(kExampleURL),
                  optimization_guide::proto::OptimizationType::COMPOSE,
                  ::testing::An<optimization_guide::OptimizationMetadata*>()))
      .WillRepeatedly(testing::DoAll(
          testing::SetArgPointee<2>(test_metadata),
          testing::Return(
              optimization_guide::OptimizationGuideDecision::kTrue)));

  GURL example(kExampleURL);
  compose::ComposeHintDecision decision =
      compose_enabling_->GetOptimizationGuidanceForUrl(example, GetProfile());

  // Verify response from CanApplyOptimization is as we expect.
  EXPECT_EQ(compose::ComposeHintDecision::COMPOSE_HINT_DECISION_ENABLED,
            decision);
}

TEST_F(ComposeEnablingTest, GetOptimizationGuidanceNoFeedbackTest) {
  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  compose_hint_metadata.set_decision(
      compose::ComposeHintDecision::COMPOSE_HINT_DECISION_ENABLED);
  test_metadata.SetAnyMetadataForTesting(compose_hint_metadata);

  EXPECT_CALL(opt_guide(),
              CanApplyOptimization(
                  GURL(kExampleURL),
                  optimization_guide::proto::OptimizationType::COMPOSE,
                  ::testing::An<optimization_guide::OptimizationMetadata*>()))
      .WillRepeatedly(testing::DoAll(
          testing::SetArgPointee<2>(test_metadata),
          testing::Return(
              optimization_guide::OptimizationGuideDecision::kFalse)));

  GURL example(kExampleURL);
  compose::ComposeHintDecision decision =
      compose_enabling_->GetOptimizationGuidanceForUrl(example, GetProfile());

  // Verify response from CanApplyOptimization is as we expect.
  EXPECT_EQ(compose::ComposeHintDecision::COMPOSE_HINT_DECISION_UNSPECIFIED,
            decision);
}

TEST_F(ComposeEnablingTest, GetOptimizationGuidanceNoComposeMetadataTest) {
  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  test_metadata.SetAnyMetadataForTesting(compose_hint_metadata);

  EXPECT_CALL(opt_guide(),
              CanApplyOptimization(
                  GURL(kExampleURL),
                  optimization_guide::proto::OptimizationType::COMPOSE,
                  ::testing::An<optimization_guide::OptimizationMetadata*>()))
      .WillRepeatedly(testing::DoAll(
          testing::SetArgPointee<2>(test_metadata),
          testing::Return(
              optimization_guide::OptimizationGuideDecision::kTrue)));

  GURL example(kExampleURL);
  compose::ComposeHintDecision decision =
      compose_enabling_->GetOptimizationGuidanceForUrl(example, GetProfile());

  // Verify response from CanApplyOptimization is as we expect.
  EXPECT_EQ(compose::ComposeHintDecision::COMPOSE_HINT_DECISION_UNSPECIFIED,
            decision);
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuOutOfPolicyURLTest) {
  // Enable everything.
  ComposeEnabling::SetEnabledForTesting(true);
  // Set the language to something we support.
  SetLanguage("en");

  // Set ContextMenuParams to textarea, which we support.
  context_menu_params_.is_content_editable_for_autofill = false;
  context_menu_params_.form_control_type =
      blink::mojom::FormControlType::kTextArea;

  base::HistogramTester histogram_tester;

  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  compose_hint_metadata.set_decision(
      compose::ComposeHintDecision::COMPOSE_HINT_DECISION_COMPOSE_DISABLED);
  test_metadata.SetAnyMetadataForTesting(compose_hint_metadata);

  EXPECT_CALL(opt_guide(),
              CanApplyOptimization(
                  GURL(kExampleURL),
                  optimization_guide::proto::OptimizationType::COMPOSE,
                  ::testing::An<optimization_guide::OptimizationMetadata*>()))
      .WillRepeatedly(testing::DoAll(
          testing::SetArgPointee<2>(test_metadata),
          testing::Return(
              optimization_guide::OptimizationGuideDecision::kTrue)));

  EXPECT_FALSE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));

  // Verify the metrics reflect the decision not to show the page.
  histogram_tester.ExpectUniqueSample(
      compose::kComposeShowStatus,
      compose::ComposeShowStatus::kPerUrlChecksFailed, 1);
}
