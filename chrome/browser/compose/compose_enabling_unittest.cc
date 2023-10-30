// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/compose/core/browser/compose_features.h"  // nogncheck - https://crbug.com/1125897
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

}  // namespace

class ComposeEnablingTest : public testing::Test {
 public:
  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    test_profile_ = profile_manager_->CreateTestingProfile("test");
    scoped_feature_list_.InitWithFeatures(
        {compose::features::kEnableCompose,
         compose::features::kEnableComposeNudge},
        {});
    context_menu_params_.is_content_editable_for_autofill = true;
  }

  void TearDown() override {
    test_profile_ = nullptr;
    profile_manager_.reset();
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  void SignIn(signin::ConsentLevel consent_level) {
    identity_test_env_.MakePrimaryAccountAvailable(kEmail, consent_level);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  void SetMsbbState(bool new_state) {
    PrefService* prefs = test_profile_->GetPrefs();
    prefs->SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        new_state);
  }

  translate::TranslateDriver* translate_driver() { return &translate_driver_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TestingProfile> test_profile_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<ComposeEnabling> compose_enabling_;
  content::ContextMenuParams context_menu_params_;

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  translate::testing::MockTranslateDriver translate_driver_;
};

TEST_F(ComposeEnablingTest, EverythingDisabledTest) {
  MockTranslateLanguageProvider mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(compose::features::kEnableCompose);
  // We intentionally don't call sign in to make our state not signed in.
  SetMsbbState(false);
  EXPECT_FALSE(compose_enabling.IsEnabled(
      test_profile_, identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, FeatureNotEnabledTest) {
  MockTranslateLanguageProvider mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Ensure feature flag is off.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(compose::features::kEnableCompose);
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // Turn on MSBB.
  SetMsbbState(true);

  EXPECT_FALSE(compose_enabling.IsEnabled(
      test_profile_, identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, MsbbDisabledTest) {
  MockTranslateLanguageProvider mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // MSBB turned off.
  SetMsbbState(false);
  EXPECT_FALSE(compose_enabling.IsEnabled(
      test_profile_, identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, NotSignedInTest) {
  MockTranslateLanguageProvider mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Turn on MSBB.
  SetMsbbState(true);
  EXPECT_FALSE(compose_enabling.IsEnabled(
      test_profile_, identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, EverythingEnabledTest) {
  MockTranslateLanguageProvider mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // Turn on MSBB.
  SetMsbbState(true);
  EXPECT_TRUE(compose_enabling.IsEnabled(
      test_profile_, identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, AlternateFlagEnabledTest) {
  MockTranslateLanguageProvider mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Ensure alternate feature flag is on and normal feature flag is off.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /* enabled features */
      {compose::features::kFillMultiLine,
       compose::features::kEnableComposeNudge},
      /* disabled features */
      {compose::features::kEnableCompose});
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // Turn on MSBB.
  SetMsbbState(true);
  EXPECT_TRUE(compose_enabling.IsEnabled(
      test_profile_, identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuDisabledTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);

  // We intentionally disable the feature.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(compose::features::kEnableCompose);

  // Set the language to something we support. "en" is English.
  translate::testing::MockTranslateClient mock_translate_client(
      translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  ON_CALL(mock_translate_language_provider,
          GetSourceLanguage(&mock_translate_manager))
      .WillByDefault(Return(std::string("en")));

  EXPECT_FALSE(compose_enabling.ShouldTriggerContextMenu(
      test_profile_, &mock_translate_manager, /*rfh=*/nullptr,
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuLanguageTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the mock language to something we don't support. "eo" is Esperanto.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  EXPECT_CALL(mock_translate_language_provider,
              GetSourceLanguage(&mock_translate_manager))
      .WillOnce(Return(std::string("eo")));

  EXPECT_FALSE(compose_enabling.ShouldTriggerContextMenu(
      test_profile_, &mock_translate_manager, /*rfh=*/nullptr,
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuLanguageBypassTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose,
       compose::features::kEnableComposeLanguageBypass},
      {});
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the mock language to something we don't support. "eo" is Esperanto.
  // Not expected to be called.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  ON_CALL(mock_translate_language_provider, GetSourceLanguage(_))
      .WillByDefault(Return(std::string("eo")));

  // Although the language is unsupported, ShouldTrigger should return true as
  // the bypass is enabled.
  EXPECT_TRUE(compose_enabling.ShouldTriggerContextMenu(
      test_profile_, &mock_translate_manager, /*rfh=*/nullptr,
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuEmptyLangugeTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the language to the empty string - translate doesn't have the answer
  // yet.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  EXPECT_CALL(mock_translate_language_provider,
              GetSourceLanguage(&mock_translate_manager))
      .WillOnce(Return(std::string()));

  EXPECT_TRUE(compose_enabling.ShouldTriggerContextMenu(
      test_profile_, &mock_translate_manager, /*rfh=*/nullptr,
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuUndeterminedLangugeTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the language to the "und" for a page where translate could not
  // determine the language.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  EXPECT_CALL(mock_translate_language_provider,
              GetSourceLanguage(&mock_translate_manager))
      .WillOnce(Return(std::string("und")));

  EXPECT_TRUE(compose_enabling.ShouldTriggerContextMenu(
      test_profile_, &mock_translate_manager, /*rfh=*/nullptr,
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuFieldTypeTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Set ContextMenuParams to non-contenteditable and non-textarea, which we do
  // not support.
  context_menu_params_.is_content_editable_for_autofill = false;
  context_menu_params_.form_control_type =
      blink::mojom::FormControlType::kInputButton;

  // Set the language to something we support. Not expected to be called.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  ON_CALL(mock_translate_language_provider, GetSourceLanguage(_))
      .WillByDefault(Return(std::string("en")));

  EXPECT_FALSE(compose_enabling.ShouldTriggerContextMenu(
      test_profile_, &mock_translate_manager, /*rfh=*/nullptr,
      context_menu_params_));
}

TEST_F(ComposeEnablingTest,
       ShouldTriggerContextMenuAllEnabledContentEditableTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the language to something we support.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  EXPECT_CALL(mock_translate_language_provider,
              GetSourceLanguage(&mock_translate_manager))
      .WillOnce(Return(std::string("en")));

  EXPECT_TRUE(compose_enabling.ShouldTriggerContextMenu(
      test_profile_, &mock_translate_manager, /*rfh=*/nullptr,
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuAllEnabledTextAreaTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the language to something we support.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  EXPECT_CALL(mock_translate_language_provider,
              GetSourceLanguage(&mock_translate_manager))
      .WillOnce(Return(std::string("en")));

  // Set ContextMenuParams to textarea, which we support.
  context_menu_params_.is_content_editable_for_autofill = false;
  context_menu_params_.form_control_type =
      blink::mojom::FormControlType::kTextArea;

  EXPECT_TRUE(compose_enabling.ShouldTriggerContextMenu(
      test_profile_, &mock_translate_manager, /*rfh=*/nullptr,
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupDisabledTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);

  // We intentionally disable the feature.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(compose::features::kEnableCompose);

  std::string autocomplete_attribute;
  bool has_saved_state = false;

  // Set the language to something we support. "en" is English.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  ON_CALL(mock_translate_language_provider,
          GetSourceLanguage(&mock_translate_manager))
      .WillByDefault(Return(std::string("en")));

  EXPECT_FALSE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, test_profile_, &mock_translate_manager,
      has_saved_state));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupLanguageTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable the feature.
  compose_enabling.SetEnabledForTesting();
  std::string autocomplete_attribute;
  bool has_saved_state = false;

  // Set the mock language to something we don't support. "eo" is Esperanto.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  EXPECT_CALL(mock_translate_language_provider,
              GetSourceLanguage(&mock_translate_manager))
      .WillOnce(Return(std::string("eo")));

  EXPECT_FALSE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, test_profile_, &mock_translate_manager,
      has_saved_state));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupLanguageBypassTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose,
       compose::features::kEnableComposeNudge,
       compose::features::kEnableComposeLanguageBypass},
      {});

  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable the feature.
  compose_enabling.SetEnabledForTesting();
  std::string autocomplete_attribute;
  bool has_saved_state = false;

  // Set the mock language to something we don't support. "eo" is Esperanto.
  // Not expected to be called.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  ON_CALL(mock_translate_language_provider, GetSourceLanguage(_))
      .WillByDefault(Return(std::string("eo")));

  // Although the language is unsupported, ShouldTrigger should return true as
  // the bypass is enabled.
  EXPECT_TRUE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, test_profile_, &mock_translate_manager,
      has_saved_state));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupAutocompleteTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();
  // Autocomplete is set to off for this page.
  std::string autocomplete_attribute("off");
  bool has_saved_state = false;

  // Set the language to something we support.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  ON_CALL(mock_translate_language_provider,
          GetSourceLanguage(&mock_translate_manager))
      .WillByDefault(Return(std::string("en")));

  EXPECT_FALSE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, test_profile_, &mock_translate_manager,
      has_saved_state));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupSavedStateTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable everything..
  compose_enabling.SetEnabledForTesting();
  std::string autocomplete_attribute;
  // We have saved state.
  bool has_saved_state = true;

  // Set the language to something we support.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  ON_CALL(mock_translate_language_provider,
          GetSourceLanguage(&mock_translate_manager))
      .WillByDefault(Return(std::string("en")));

  EXPECT_FALSE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, test_profile_, &mock_translate_manager,
      has_saved_state));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupAllEnabledTest) {
  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();
  std::string autocomplete_attribute;
  bool has_saved_state = false;

  // Set the language to something we support.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);
  EXPECT_CALL(mock_translate_language_provider,
              GetSourceLanguage(&mock_translate_manager))
      .WillOnce(Return(std::string("en")));

  EXPECT_TRUE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, test_profile_, &mock_translate_manager,
      has_saved_state));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupNudgeDisabledTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose},
      {compose::features::kEnableComposeNudge});

  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider;
  ComposeEnabling compose_enabling(&mock_translate_language_provider);
  // Enable everything at the profile level.
  compose_enabling.SetEnabledForTesting();

  // Set the language to something we support.
  MockTranslateClient mock_translate_client(translate_driver(), nullptr);
  testing::NiceMock<MockTranslateManager> mock_translate_manager(
      &mock_translate_client);

  EXPECT_FALSE(compose_enabling.ShouldTriggerPopup(
      "", test_profile_, &mock_translate_manager,
      /* has_saved_state= */ false));
}
