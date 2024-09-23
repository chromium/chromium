// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_enabling.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/autofill/core/common/aliases.h"
#include "components/compose/core/browser/compose_features.h"
#include "components/compose/core/browser/config.h"
#include "components/language/core/browser/language_model.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_driver.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::ErrorIs;
using base::test::FeatureRef;
using testing::_;
using testing::Return;

namespace {
constexpr char kEmail[] = "example@gmail.com";
constexpr char kExampleURL[] = "https://example.com";
constexpr char kExampleBadURL[] = "chrome://version";
using translate::testing::MockTranslateClient;

class TestLanguageModel : public language::LanguageModel {
  std::vector<LanguageDetails> GetLanguages() override {
    return {LanguageDetails("en", 1.0)};
  }
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
           callback,
       std::optional<optimization_guide::proto::RequestContextMetadata>
           request_context_metadata));
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

    // Set flags to their expected enabled/disabled state for these tests
    // without relyong on their default state. In other words, a change in
    // default state should not cause these tests to break.
    // Note: individual tests may reset flags to their default state.
    ResetFeaturesAndConfig(
        {compose::features::kEnableCompose,
         compose::features::kEnableComposeSavedStateNudge,
         compose::features::kEnableComposeLanguageBypassForContextMenu,
         compose::features::kEnableComposeSavedStateNotification},
        {compose::features::kEnableComposeProactiveNudge});

    mock_translate_ranker_ =
        std::make_unique<translate::testing::MockTranslateRanker>();
    mock_translate_client_ =
        std::make_unique<MockTranslateClient>(&translate_driver_, nullptr);
    translate_manager_ = std::make_unique<translate::TranslateManager>(
        mock_translate_client_.get(), mock_translate_ranker_.get(),
        language_model_.get());

    // Note that AddTab makes its own ComposeEnabling as part of
    // ChromeComposeClient. This can cause confusion when debugging tests.
    // Don't confuse the two ComposeEnabling objects when debugging.
    AddTab(browser(), GURL(kExampleBadURL));
    AddTab(browser(), GURL(kExampleURL));
    context_menu_params_.is_content_editable_for_autofill = true;
    context_menu_params_.frame_origin = GetOrigin();

    opt_guide_ = static_cast<
        testing::NiceMock<CustomMockOptimizationGuideKeyedService>*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(GetProfile()));
    ON_CALL(*opt_guide_,
            CanApplyOptimization(
                _, optimization_guide::proto::OptimizationType::COMPOSE,
                testing::An<optimization_guide::OptimizationMetadata*>()))
        .WillByDefault(
            [](const GURL& url,
               optimization_guide::proto::OptimizationType optimization_type,
               optimization_guide::OptimizationMetadata* metadata)
                -> optimization_guide::OptimizationGuideDecision {
              *metadata = {};
              compose::ComposeHintMetadata compose_hint_metadata;
              compose_hint_metadata.set_decision(
                  compose::ComposeHintDecision::COMPOSE_HINT_DECISION_ENABLED);
              metadata->SetAnyMetadataForTesting(compose_hint_metadata);
              return optimization_guide::OptimizationGuideDecision::kTrue;
            });

    ASSERT_TRUE(opt_guide_);

    // Build the ComposeEnabling object the tests will use, providing it with
    // mocks for its dependencies.
    // TODO(b/316625561) Simplify these tests more now that we have dependency
    // injection.
    compose_enabling_ = std::make_unique<ComposeEnabling>(
        GetProfile(), identity_test_env_.identity_manager(), &opt_guide());

    // Override un-mockable per-user checks.
    scoped_skip_user_check_ = ComposeEnabling::ScopedSkipUserCheckForTesting();
    // Set the user country to US, a Compose default-enabled country.
    scoped_country_override_ = ComposeEnabling::OverrideCountryForTesting("us");
  }

  void TearDown() override {
    // We must destroy the ComposeEnabling while the opt_guide object is still
    // valid so we can call unregister in the destructor.
    compose_enabling_ = nullptr;
    opt_guide_ = nullptr;
    scoped_feature_list_.Reset();
    compose::ResetConfigForTesting();
    BrowserWithTestWindowTest::TearDown();
    MockOptimizationGuideKeyedService::ResetForTesting();
  }

  void SetProactiveNudgePref(bool pref_value) {
    PrefService* prefs = GetProfile()->GetPrefs();
    prefs->SetBoolean(prefs::kEnableProactiveNudge, pref_value);
  }

  void AddDomainToProactiveNudgeDisabledSitesPref() {
    ScopedDictPrefUpdate update(GetProfile()->GetPrefs(),
                                prefs::kProactiveNudgeDisabledSitesWithTime);
    update->Set(GetOrigin().Serialize(), base::TimeToValue(base::Time::Now()));
  }

  void SignIn(signin::ConsentLevel consent_level) {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail, consent_level);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  // The Config depends on several features. Reset the config after setting
  // new features.
  void ResetFeaturesAndConfig(
      const std::vector<FeatureRef>& enabled_features,
      const std::vector<FeatureRef>& disabled_features) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    compose::ResetConfigForTesting();
  }

  CustomMockOptimizationGuideKeyedService& opt_guide() { return *opt_guide_; }

 protected:
  void SetLanguage(std::string lang) {
    translate_manager_->GetLanguageState()->SetSourceLanguage(lang);
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
  std::unique_ptr<translate::testing::MockTranslateClient>
      mock_translate_client_;
  std::unique_ptr<translate::testing::MockTranslateRanker>
      mock_translate_ranker_;
  std::unique_ptr<TestLanguageModel> language_model_;
  std::unique_ptr<translate::TranslateManager> translate_manager_;

  std::unique_ptr<ComposeEnabling> compose_enabling_;
  ComposeEnabling::ScopedOverride scoped_skip_user_check_;
  ComposeEnabling::ScopedOverride scoped_country_override_;
};

TEST_F(ComposeEnablingTest, EverythingDisabledTest) {
  ResetFeaturesAndConfig({}, {compose::features::kEnableCompose});
  // We intentionally don't call sign in to make our state not signed in.
  EXPECT_NE(compose_enabling_->IsEnabled(), base::ok());
}

TEST_F(ComposeEnablingTest, FeatureNotEnabledTest) {
  ResetFeaturesAndConfig({}, {compose::features::kEnableCompose});
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);

  CheckIsEnabledError(compose_enabling_.get(),
                      compose::ComposeShowStatus::kComposeFeatureFlagDisabled);
}

TEST_F(ComposeEnablingTest, NotSignedInTest) {
  base::HistogramTester histogram_tester;
  // Intentionally skip the signin step.
  CheckIsEnabledError(compose_enabling_.get(),
                      compose::ComposeShowStatus::kSignedOut);

  std::string autocomplete_attribute;
  // Check that the proactive nudge does not show.
  auto should_trigger = compose_enabling_->ShouldTriggerNoStatePopup(
      autocomplete_attribute, /*allows_writing_suggestions=*/true, GetProfile(),
      GetProfile()->GetPrefs(), translate_manager_.get(), GetOrigin(),
      GetOrigin(), GURL(kExampleURL), /*is_msbb_enabled*/ true);

  EXPECT_EQ(should_trigger.error(), compose::ComposeShowStatus::kSignedOut);
}

TEST_F(ComposeEnablingTest, SignedInErrorTest) {
  // Sign in, with error.
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSync);
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  CheckIsEnabledError(compose_enabling_.get(),
                      compose::ComposeShowStatus::kSignedOut);
}

TEST_F(ComposeEnablingTest, ComposeEligibleTest) {
  scoped_feature_list_.Reset();
  // Turn on the enable switch and off the eligible switch.
  scoped_feature_list_.InitWithFeatures({compose::features::kEnableCompose},
                                        {compose::features::kComposeEligible});
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);

  // The ComposeEligible switch should win, and disable the feature.
  CheckIsEnabledError(compose_enabling_.get(),
                      compose::ComposeShowStatus::kNotComposeEligible);
}

TEST_F(ComposeEnablingTest, EverythingEnabledTest) {
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  EXPECT_EQ(compose_enabling_->IsEnabled(), base::ok());
}

TEST_F(ComposeEnablingTest, UserNotAllowedTest) {
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // Cause per-user check to fail.
  scoped_skip_user_check_.reset();

  EXPECT_THAT(
      compose_enabling_->IsEnabled(),
      ErrorIs(compose::ComposeShowStatus::kUserNotAllowedByOptimizationGuide));
}

TEST_F(ComposeEnablingTest, StaticMethodEverythingDisabledTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures({},
                                        {compose::features::kEnableCompose});
  // We intentionally don't call sign in to make our state not signed in.
  EXPECT_FALSE(ComposeEnabling::IsEnabledForProfile(GetProfile()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuDisabledTest) {
  // We intentionally disable the feature.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableComposeSavedStateNudge},
      {compose::features::kEnableCompose});

  EXPECT_FALSE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(),
      /*rfh=*/GetRenderFrameHost(), context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuLanguageTest) {
  // Disable the language bypass.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose},
      {compose::features::kEnableComposeLanguageBypassForContextMenu});
  // Enable all base requirements.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  // Set the mock to return a language we support (English).
  SetLanguage("en");
  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
  EXPECT_TRUE(
      compose_enabling_->IsPageLanguageSupported(translate_manager_.get()));

  // Set the mock to return a language we don't support (Esperanto).
  SetLanguage("eo");
  EXPECT_FALSE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
  EXPECT_FALSE(
      compose_enabling_->IsPageLanguageSupported(translate_manager_.get()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuLanguageBypassTest) {
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  // Set the mock to return a language we don't support (Esperanto).
  SetLanguage("eo");
  // Although the language is unsupported, ShouldTrigger should return true as
  // the bypass is enabled.
  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
  EXPECT_FALSE(
      compose_enabling_->IsPageLanguageSupported(translate_manager_.get()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuEmptyLanguageTest) {
  // Disable the language bypass.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose},
      {compose::features::kEnableComposeLanguageBypassForContextMenu});
  // Enable all base requirements.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  // Set the mock to return the empty string, simluating that translate doesn't
  // have the answer yet.
  SetLanguage("");
  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
  EXPECT_TRUE(
      compose_enabling_->IsPageLanguageSupported(translate_manager_.get()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuUndeterminedLangugeTest) {
  // Disable the language bypass.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose},
      {compose::features::kEnableComposeLanguageBypassForContextMenu});
  // Enable all base requirements.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  // Set the mock to return "und", simluating that translate could not determine
  // the page language.
  SetLanguage("und");
  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
  EXPECT_TRUE(
      compose_enabling_->IsPageLanguageSupported(translate_manager_.get()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuFieldTypeTest) {
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  // Set ContextMenuParams to non-contenteditable and non-textarea, which we do
  // not support.
  context_menu_params_.is_content_editable_for_autofill = false;
  context_menu_params_.form_control_type =
      blink::mojom::FormControlType::kInputButton;

  EXPECT_FALSE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuIncorrectSchemeTest) {
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  // Get the rfh for the tab with the incorrect Scheme.
  auto* rfh =
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetPrimaryMainFrame();

  EXPECT_FALSE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(), rfh, context_menu_params_));
}

TEST_F(ComposeEnablingTest,
       ShouldTriggerContextMenuAllEnabledContentEditableTest) {
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuAllEnabledTextAreaTest) {
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  // Set ContextMenuParams to textarea, which we support.
  context_menu_params_.is_content_editable_for_autofill = false;
  context_menu_params_.form_control_type =
      blink::mojom::FormControlType::kTextArea;

  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupDefaultTest) {
  // Use the default feature values set in SetUp.
  std::string autocomplete_attribute;

  // The saved state nudge is enabled by default.
  EXPECT_TRUE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));

  // The proactive nudge is disabled by default.
  EXPECT_FALSE(compose_enabling_
                   ->ShouldTriggerNoStatePopup(
                       autocomplete_attribute,
                       /*allows_writing_suggestions=*/true, GetProfile(),
                       GetProfile()->GetPrefs(), translate_manager_.get(),
                       GetOrigin(), GetOrigin(), GURL(kExampleURL),
                       /*is_msbb_enabled*/ true)
                   .has_value());
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupDisabledTest) {
  // We intentionally disable the feature.
  ResetFeaturesAndConfig({},
                         {// Disable saved state nudge.
                          compose::features::kEnableComposeSavedStateNudge});

  std::string autocomplete_attribute;

  EXPECT_FALSE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupLanguageTests) {
  ResetFeaturesAndConfig({compose::features::kEnableComposeProactiveNudge}, {});

  // Enable the feature.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();
  std::string autocomplete_attribute;

  // Check that a non-English page blocks the proactive nudge but not the
  // saved state nudge.
  SetLanguage("eo");

  EXPECT_TRUE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));

  EXPECT_FALSE(compose_enabling_
                   ->ShouldTriggerNoStatePopup(
                       autocomplete_attribute,
                       /*allows_writing_suggestions=*/true, GetProfile(),
                       GetProfile()->GetPrefs(), translate_manager_.get(),
                       GetOrigin(), GetOrigin(), GURL(kExampleURL),
                       /*is_msbb_enabled*/ true)
                   .has_value());

  // Check that both nudges are allowed with English.
  SetLanguage("en");

  EXPECT_TRUE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));

  EXPECT_TRUE(compose_enabling_
                  ->ShouldTriggerNoStatePopup(
                      autocomplete_attribute,
                      /*allows_writing_suggestions=*/true, GetProfile(),
                      GetProfile()->GetPrefs(), translate_manager_.get(),
                      GetOrigin(), GetOrigin(), GURL(kExampleURL),
                      /*is_msbb_enabled*/ true)
                  .has_value());

  // Check that both nudges are allowed with "und".
  SetLanguage("und");

  EXPECT_TRUE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));

  EXPECT_TRUE(compose_enabling_
                  ->ShouldTriggerNoStatePopup(
                      autocomplete_attribute,
                      /*allows_writing_suggestions=*/true, GetProfile(),
                      GetProfile()->GetPrefs(), translate_manager_.get(),
                      GetOrigin(), GetOrigin(), GURL(kExampleURL),
                      /*is_msbb_enabled*/ true)
                  .has_value());
}

TEST_F(ComposeEnablingTest, ShouldNotTriggerProactivePopupAutocompleteOffTest) {
  ResetFeaturesAndConfig({compose::features::kEnableComposeProactiveNudge}, {});
  base::HistogramTester histogram_tester;
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();
  // Autocomplete is set to off for this page.
  std::string autocomplete_attribute("off");

  // The autocomplete attribute is ignored with saved state.
  EXPECT_TRUE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));

  // The autocomplete attribute is checked for the proactive nudge.
  auto should_trigger = compose_enabling_->ShouldTriggerNoStatePopup(
      autocomplete_attribute, /*allows_writing_suggestions=*/true, GetProfile(),
      GetProfile()->GetPrefs(), translate_manager_.get(), GetOrigin(),
      GetOrigin(), GURL(kExampleURL),
      /*is_msbb_enabled*/ true);

  EXPECT_EQ(should_trigger.error(),
            compose::ComposeShowStatus::kAutocompleteOff);
}

TEST_F(ComposeEnablingTest, ShouldNotTriggerProactivePopupIfMSBBDisabled) {
  ResetFeaturesAndConfig({compose::features::kEnableComposeProactiveNudge}, {});
  base::HistogramTester histogram_tester;
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  std::string autocomplete_attribute;

  // The proactive nudge does not show when msbb is disabled.
  auto should_trigger = compose_enabling_->ShouldTriggerNoStatePopup(
      autocomplete_attribute, /*allows_writing_suggestions=*/true, GetProfile(),
      GetProfile()->GetPrefs(), translate_manager_.get(), GetOrigin(),
      GetOrigin(), GURL(kExampleURL),
      /*is_msbb_enabled=*/false);
  ASSERT_EQ(should_trigger.error(),
            compose::ComposeShowStatus::kProactiveNudgeDisabledByMSBB);

  // The proactive nudge shows when msbb is enabled.
  EXPECT_TRUE(compose_enabling_
                  ->ShouldTriggerNoStatePopup(
                      autocomplete_attribute,
                      /*allows_writing_suggestions=*/true, GetProfile(),
                      GetProfile()->GetPrefs(), translate_manager_.get(),
                      GetOrigin(), GetOrigin(), GURL(kExampleURL),
                      /*is_msbb_enabled=*/true)
                  .has_value());
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupWithSavedStateTest) {
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();
  std::string autocomplete_attribute;

  // test all variants of: popup with, popup without state.
  std::vector<std::pair<bool, bool>> tests = {
      {true, true}, {true, false}, {false, true}, {false, false}};
  for (auto [saved_state_nudge, proactive_nudge] : tests) {
    compose::Config& config = compose::GetMutableConfigForTesting();
    config.saved_state_nudge_enabled = saved_state_nudge;
    config.proactive_nudge_enabled = proactive_nudge;

    EXPECT_EQ(
        saved_state_nudge,
        compose_enabling_->ShouldTriggerSavedStatePopup(
            autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));

    EXPECT_EQ(proactive_nudge,
              compose_enabling_
                  ->ShouldTriggerNoStatePopup(
                      autocomplete_attribute,
                      /*allows_writing_suggestions=*/true, GetProfile(),
                      GetProfile()->GetPrefs(), translate_manager_.get(),
                      GetOrigin(), GetOrigin(), GURL(kExampleURL),
                      /*is_msbb_enabled*/ true)
                  .has_value());
  }
}

TEST_F(ComposeEnablingTest, ComposeSavedStateNotificationEnabledByDefault) {
  std::string autocomplete_attribute;

  EXPECT_TRUE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kComposeDialogLostFocus));
}

TEST_F(ComposeEnablingTest, SavedStateNotificationWithSavedStateNudgeDisabled) {
  ResetFeaturesAndConfig({},
                         {compose::features::kEnableComposeSavedStateNudge});
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();
  std::string autocomplete_attribute;

  // Saved State Notification does not trigger if saved state nudge is disabled.
  EXPECT_FALSE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kComposeDialogLostFocus));
}

TEST_F(ComposeEnablingTest,
       ShouldTriggerPopupSavedStateNotificationDisabledTest) {
  // Disable the notification flag.
  ResetFeaturesAndConfig(
      {compose::features::kEnableComposeProactiveNudge},
      {compose::features::kEnableComposeSavedStateNotification});

  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();
  std::string autocomplete_attribute;

  // Nudge still works, even if Saved State Notification is disabled.
  EXPECT_TRUE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));

  // Saved state notification is disabled.
  EXPECT_FALSE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kComposeDialogLostFocus));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupIncorrectSchemeTest) {
  base::HistogramTester histogram_tester;
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.saved_state_nudge_enabled = true;
  config.proactive_nudge_enabled = true;
  std::string autocomplete_attribute;

  // Use URL with incorrect scheme is checked when no previous state.
  auto should_trigger = compose_enabling_->ShouldTriggerNoStatePopup(
      autocomplete_attribute, /*allows_writing_suggestions=*/true, GetProfile(),
      GetProfile()->GetPrefs(), translate_manager_.get(), GetOrigin(),
      url::Origin(), GURL(kExampleBadURL),
      /*is_msbb_enabled*/ true);
  ASSERT_EQ(should_trigger.error(),
            compose::ComposeShowStatus::kIncorrectScheme);

  // Use URL with incorrect scheme is not checked when there is previous state.
  EXPECT_TRUE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupCrossOrigin) {
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();
  compose::Config& config = compose::GetMutableConfigForTesting();
  config.proactive_nudge_enabled = true;
  std::string autocomplete_attribute;

  EXPECT_FALSE(compose_enabling_
                   ->ShouldTriggerNoStatePopup(
                       autocomplete_attribute,
                       /*allows_writing_suggestions=*/true, GetProfile(),
                       GetProfile()->GetPrefs(), translate_manager_.get(),
                       GetOrigin(), url::Origin(), GURL(kExampleURL),
                       /*is_msbb_enabled*/ true)
                   .has_value());
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuCrossOrigin) {
  base::HistogramTester histogram_tester;
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  context_menu_params_.frame_origin = url::Origin();
  EXPECT_FALSE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
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

TEST_F(ComposeEnablingTest, ShouldTriggerDisableComposeByPolicyTest) {
  ResetFeaturesAndConfig({compose::features::kEnableComposeProactiveNudge}, {});
  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  // Set ContextMenuParams to textarea, which we support.
  context_menu_params_.is_content_editable_for_autofill = false;
  context_menu_params_.form_control_type =
      blink::mojom::FormControlType::kTextArea;

  std::string autocomplete_attribute;
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
      GetProfile(), translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));

  // Check that the proactive nudge is also disabled.
  EXPECT_FALSE(compose_enabling_
                   ->ShouldTriggerNoStatePopup(
                       autocomplete_attribute,
                       /*allows_writing_suggestions=*/true, GetProfile(),
                       GetProfile()->GetPrefs(), translate_manager_.get(),
                       GetOrigin(), GetOrigin(), GURL(kExampleURL),
                       /*is_msbb_enabled*/ true)
                   .has_value());

  // The saved state is not disabled.
  EXPECT_TRUE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));

  // Verify the metrics reflect the decision not to show the page.
  histogram_tester.ExpectUniqueSample(
      compose::kComposeShowStatus,
      compose::ComposeShowStatus::kPerUrlChecksFailed, 1);
}

TEST_F(ComposeEnablingTest, ShouldTriggerDisableNudgeByPolicy) {
  ResetFeaturesAndConfig({compose::features::kEnableComposeProactiveNudge}, {});

  // Enable everything.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();

  // Set ContextMenuParams to textarea, which we support.
  context_menu_params_.is_content_editable_for_autofill = false;
  context_menu_params_.form_control_type =
      blink::mojom::FormControlType::kTextArea;

  std::string autocomplete_attribute;

  // Set up a fake metadata to return from the mock.
  optimization_guide::OptimizationMetadata test_metadata;
  compose::ComposeHintMetadata compose_hint_metadata;
  compose_hint_metadata.set_decision(
      compose::ComposeHintDecision::COMPOSE_HINT_DECISION_DISABLE_NUDGE);
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

  // The context is not disabled.
  EXPECT_TRUE(compose_enabling_->ShouldTriggerContextMenu(
      GetProfile(), translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));

  // The saved state nudge is not disabled.
  EXPECT_TRUE(compose_enabling_->ShouldTriggerSavedStatePopup(
      autofill::AutofillSuggestionTriggerSource::kTextFieldDidChange));

  // Check that the proactive nudge is disabled.
  EXPECT_FALSE(compose_enabling_
                   ->ShouldTriggerNoStatePopup(
                       autocomplete_attribute,
                       /*allows_writing_suggestions=*/true, GetProfile(),
                       GetProfile()->GetPrefs(), translate_manager_.get(),
                       GetOrigin(), GetOrigin(), GURL(kExampleURL),
                       /*is_msbb_enabled*/ true)
                   .has_value());
  // Check that the proactive nudge is not disabled if override is set in the
  // config.
  compose::GetMutableConfigForTesting()
      .proactive_nudge_bypass_optimization_guide = true;
  EXPECT_TRUE(compose_enabling_
                  ->ShouldTriggerNoStatePopup(
                      autocomplete_attribute,
                      /*allows_writing_suggestions=*/true, GetProfile(),
                      GetProfile()->GetPrefs(), translate_manager_.get(),
                      GetOrigin(), GetOrigin(), GURL(kExampleURL),
                      /*is_msbb_enabled*/ true)
                  .has_value());
}

TEST_F(ComposeEnablingTest, ProactiveNudgeGlobalPreferenceTest) {
  ResetFeaturesAndConfig({compose::features::kEnableComposeProactiveNudge}, {});
  base::HistogramTester histogram_tester;
  // Enable the feature.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();
  std::string autocomplete_attribute;

  // Preference is enabled by default, proactive nudge should trigger.
  EXPECT_TRUE(compose_enabling_
                  ->ShouldTriggerNoStatePopup(
                      autocomplete_attribute,
                      /*allows_writing_suggestions=*/true, GetProfile(),
                      GetProfile()->GetPrefs(), translate_manager_.get(),
                      GetOrigin(), GetOrigin(), GURL(kExampleURL),
                      /*is_msbb_enabled*/ true)
                  .has_value());

  // When preference is disabled, proactive nudge should not trigger
  SetProactiveNudgePref(false);
  auto should_trigger = compose_enabling_->ShouldTriggerNoStatePopup(
      autocomplete_attribute, /*allows_writing_suggestions=*/true, GetProfile(),
      GetProfile()->GetPrefs(), translate_manager_.get(), GetOrigin(),
      GetOrigin(), GURL(kExampleURL),
      /*is_msbb_enabled*/ true);
  EXPECT_EQ(should_trigger.error(),
            compose::ComposeShowStatus::
                kProactiveNudgeDisabledGloballyByUserPreference);
}

TEST_F(ComposeEnablingTest, ProactiveNudgeDisabledSitesPreferenceTest) {
  ResetFeaturesAndConfig({compose::features::kEnableComposeProactiveNudge}, {});
  base::HistogramTester histogram_tester;
  // Enable the feature.
  auto scoped_compose_enabled =
      ComposeEnabling::ScopedEnableComposeForTesting();
  std::string autocomplete_attribute;

  // Preference is enabled by default, proactive nudge should trigger on default
  // origin.
  ASSERT_TRUE(compose_enabling_
                  ->ShouldTriggerNoStatePopup(
                      autocomplete_attribute,
                      /*allows_writing_suggestions=*/true, GetProfile(),
                      GetProfile()->GetPrefs(), translate_manager_.get(),
                      GetOrigin(), GetOrigin(), GURL(kExampleURL),
                      /*is_msbb_enabled*/ true)
                  .has_value());

  // When origin is added to disabled sites list, proactive nudge should not
  // trigger.
  AddDomainToProactiveNudgeDisabledSitesPref();
  auto should_trigger = compose_enabling_->ShouldTriggerNoStatePopup(
      autocomplete_attribute, /*allows_writing_suggestions=*/true, GetProfile(),
      GetProfile()->GetPrefs(), translate_manager_.get(), GetOrigin(),
      GetOrigin(), GURL(kExampleURL),
      /*is_msbb_enabled*/ true);
  EXPECT_EQ(should_trigger.error(),
            compose::ComposeShowStatus::
                kProactiveNudgeDisabledForSiteByUserPreference);
}

TEST_F(ComposeEnablingTest, ClientCountryNotInFinchCountryList) {
  // Sets a country list via Finch that does not include "us".
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      compose::features::kEnableCompose, {{"enabled_countries", "a, b, c"}});
  compose::ResetConfigForTesting();

  EXPECT_THAT(compose_enabling_->IsEnabled(),
              ErrorIs(compose::ComposeShowStatus::kComposeNotEnabledInCountry));
}

TEST_F(ComposeEnablingTest, ClientCountryUndefined) {
  // Replace the client country override with an undefined country.
  scoped_country_override_.reset();
  scoped_country_override_ = ComposeEnabling::OverrideCountryForTesting("");

  EXPECT_THAT(compose_enabling_->IsEnabled(),
              ErrorIs(compose::ComposeShowStatus::kUndefinedCountry));
}

TEST_F(ComposeEnablingTest, AnyAndAllCountriesAllowed) {
  // Replace the client country override with an undefined country.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      compose::features::kEnableCompose, {{"enabled_countries", "*"}});

  EXPECT_NE(compose_enabling_->IsEnabled(), base::ok());

  scoped_country_override_.reset();
  EXPECT_NE(compose_enabling_->IsEnabled(), base::ok());

  scoped_country_override_ = ComposeEnabling::OverrideCountryForTesting("");
  EXPECT_NE(compose_enabling_->IsEnabled(), base::ok());
}
