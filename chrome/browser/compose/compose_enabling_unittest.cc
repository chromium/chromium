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
#include "chrome/test/base/browser_with_test_window_test.h"
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

class ComposeEnablingTest : public BrowserWithTestWindowTest {
 public:
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

    AddTab(browser(), GURL("http://foo/1"));
    context_menu_params_.is_content_editable_for_autofill = true;
    context_menu_params_.frame_origin = GetOrigin();
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

  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;

  content::ContextMenuParams context_menu_params_;

  translate::testing::MockTranslateDriver translate_driver_;
  std::unique_ptr<MockTranslateClient> mock_translate_client_;
  std::unique_ptr<testing::NiceMock<MockTranslateManager>>
      mock_translate_manager_;

  testing::NiceMock<MockTranslateLanguageProvider>
      mock_translate_language_provider_;
};

TEST_F(ComposeEnablingTest, EverythingDisabledTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(compose::features::kEnableCompose);
  // We intentionally don't call sign in to make our state not signed in.
  SetMsbbState(false);
  EXPECT_FALSE(compose_enabling.IsEnabled(
      GetProfile(), identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, FeatureNotEnabledTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Ensure feature flag is off.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(compose::features::kEnableCompose);
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // Turn on MSBB.
  SetMsbbState(true);

  EXPECT_FALSE(compose_enabling.IsEnabled(
      GetProfile(), identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, MsbbDisabledTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // MSBB turned off.
  SetMsbbState(false);
  EXPECT_FALSE(compose_enabling.IsEnabled(
      GetProfile(), identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, NotSignedInTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Turn on MSBB.
  SetMsbbState(true);
  EXPECT_FALSE(compose_enabling.IsEnabled(
      GetProfile(), identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, EverythingEnabledTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Sign in, with sync turned on.
  SignIn(signin::ConsentLevel::kSync);
  // Turn on MSBB.
  SetMsbbState(true);
  EXPECT_TRUE(compose_enabling.IsEnabled(
      GetProfile(), identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, AlternateFlagEnabledTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
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
      GetProfile(), identity_test_env_.identity_manager()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuDisabledTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);

  // We intentionally disable the feature.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(compose::features::kEnableCompose);

  // Set the language to something we support. "en" is English.
  SetLanguage("en");

  EXPECT_FALSE(compose_enabling.ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuLanguageTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  SetLanguage("eo");

  EXPECT_FALSE(compose_enabling.ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuLanguageBypassTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose,
       compose::features::kEnableComposeLanguageBypass},
      {});
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the mock language to something we don't support. "eo" is Esperanto.
  SetLanguage("eo");

  // Although the language is unsupported, ShouldTrigger should return true as
  // the bypass is enabled.
  EXPECT_TRUE(compose_enabling.ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuEmptyLangugeTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the language to the empty string - translate doesn't have the answer
  // yet.
  SetLanguage("");

  EXPECT_TRUE(compose_enabling.ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuUndeterminedLangugeTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the language to the "und" for a page where translate could not
  // determine the language.
  SetLanguage("und");

  EXPECT_TRUE(compose_enabling.ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuFieldTypeTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Set ContextMenuParams to non-contenteditable and non-textarea, which we do
  // not support.
  context_menu_params_.is_content_editable_for_autofill = false;
  context_menu_params_.form_control_type =
      blink::mojom::FormControlType::kInputButton;

  // Set the language to something we support. Not expected to be called.
  SetLanguage("en");

  EXPECT_FALSE(compose_enabling.ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest,
       ShouldTriggerContextMenuAllEnabledContentEditableTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the language to something we support.
  SetLanguage("en");

  EXPECT_TRUE(compose_enabling.ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuAllEnabledTextAreaTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the language to something we support.
  SetLanguage("en");

  // Set ContextMenuParams to textarea, which we support.
  context_menu_params_.is_content_editable_for_autofill = false;
  context_menu_params_.form_control_type =
      blink::mojom::FormControlType::kTextArea;

  EXPECT_TRUE(compose_enabling.ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupDisabledTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);

  // We intentionally disable the feature.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(compose::features::kEnableCompose);

  std::string autocomplete_attribute;
  bool has_saved_state = false;

  // Set the language to something we support. "en" is English.
  SetLanguage("en");

  EXPECT_FALSE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), GetOrigin()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupLanguageTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable the feature.
  compose_enabling.SetEnabledForTesting();
  std::string autocomplete_attribute;
  bool has_saved_state = false;

  // Set the mock language to something we don't support. "eo" is Esperanto.
  SetLanguage("eo");

  EXPECT_FALSE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), GetOrigin()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupLanguageBypassTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose,
       compose::features::kEnableComposeNudge,
       compose::features::kEnableComposeLanguageBypass},
      {});

  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable the feature.
  compose_enabling.SetEnabledForTesting();
  std::string autocomplete_attribute;
  bool has_saved_state = false;

  // Set the mock language to something we don't support. "eo" is Esperanto.
  // Not expected to be called.
  SetLanguage("eo");

  // Although the language is unsupported, ShouldTrigger should return true as
  // the bypass is enabled.
  EXPECT_TRUE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), GetOrigin()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupAutocompleteTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();
  // Autocomplete is set to off for this page.
  std::string autocomplete_attribute("off");
  bool has_saved_state = false;

  // Set the language to something we support.
  SetLanguage("en");

  EXPECT_FALSE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), GetOrigin()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupSavedStateTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything..
  compose_enabling.SetEnabledForTesting();
  std::string autocomplete_attribute;
  // We have saved state.
  bool has_saved_state = true;

  // Set the language to something we support.
  SetLanguage("en");

  EXPECT_FALSE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), GetOrigin()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupAllEnabledTest) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();
  std::string autocomplete_attribute;
  bool has_saved_state = false;

  SetLanguage("en");

  EXPECT_TRUE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), GetOrigin()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupNudgeDisabledTest) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {compose::features::kEnableCompose},
      {compose::features::kEnableComposeNudge});

  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything at the profile level.
  compose_enabling.SetEnabledForTesting();

  // Set the language to something we support.
  SetLanguage("en");
  EXPECT_FALSE(compose_enabling.ShouldTriggerPopup(
      "", GetProfile(), mock_translate_manager_.get(),
      /* has_saved_state= */ false, GetOrigin(), GetOrigin()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerPopupCrossOrigin) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();
  std::string autocomplete_attribute;
  bool has_saved_state = false;

  // Set the language to something we support.
  SetLanguage("en");
  EXPECT_FALSE(compose_enabling.ShouldTriggerPopup(
      autocomplete_attribute, GetProfile(), mock_translate_manager_.get(),
      has_saved_state, GetOrigin(), url::Origin()));
}

TEST_F(ComposeEnablingTest, ShouldTriggerContextMenuCrossOrigin) {
  ComposeEnabling compose_enabling(&mock_translate_language_provider_);
  // Enable everything.
  compose_enabling.SetEnabledForTesting();

  // Set the language to something we support.
  SetLanguage("en");

  context_menu_params_.frame_origin = url::Origin();
  EXPECT_FALSE(compose_enabling.ShouldTriggerContextMenu(
      GetProfile(), mock_translate_manager_.get(), /*rfh=*/GetRenderFrameHost(),
      context_menu_params_));
}
