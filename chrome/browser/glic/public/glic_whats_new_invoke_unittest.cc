// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_whats_new_invoke.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/browser_command/browser_command_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/glic_enums.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {

class GlicWhatsNewInvokeTest : public testing::Test {
 protected:
  static InvokeFromWhatsNewPasskey GetPassKey() {
    return InvokeFromWhatsNewPasskeyProvider::GetPassKey();
  }

  explicit GlicWhatsNewInvokeTest(
      const std::vector<base::test::FeatureRefAndParams>& enabled_features = {},
      const std::vector<base::test::FeatureRef>& disabled_features = {
          features::kGlicBrowserCommandInvoke,
          features::kGlicWhatsNewQuizzes}) {
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  void SetUp() override {
    GlicEnabling::SetSystemRequirementMetForTesting(true);
    TestingProfileManager* testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);

#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)

    TestingProfile::TestingFactories testing_factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();

    profile_ = testing_profile_manager->CreateTestingProfile(
        "test-profile", std::move(testing_factories));

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&GlicWhatsNewInvokeTest::CreateService,
                                      base::Unretained(this)));

    mock_service_ = static_cast<MockGlicKeyedService*>(
        GlicKeyedServiceFactory::GetGlicKeyedService(profile_,
                                                     /*create=*/true));
    ASSERT_TRUE(mock_service_);
  }

  void TearDown() override {
    GlicEnabling::SetSystemRequirementMetForTesting(std::nullopt);
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    mock_service_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
        context, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        ContextualCueingServiceFactory::GetForProfile(profile),
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
  }

  base::test::ScopedFeatureList feature_list_;
  std::optional<GlicEnabling::ScopedBypassEnablementChecksForTesting>
      scoped_glic_bypass_{std::in_place};
  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  raw_ptr<TestingProfile> profile_ = nullptr;
  GlicProfileManager glic_profile_manager_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<MockGlicKeyedService> mock_service_ = nullptr;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_;
};

namespace {

TEST_F(GlicWhatsNewInvokeTest, InvokeFeatureDisabled_CallsToggleUI) {
  EXPECT_CALL(*mock_service_, ToggleUI(&mock_browser_window_,
                                       /*prevent_close=*/false,
                                       mojom::InvocationSource::kWhatsNew))
      .Times(1);
  EXPECT_CALL(*mock_service_, Invoke(testing::_)).Times(0);
  EXPECT_CALL(*mock_service_, InvokeWithAutoSubmit(testing::_, testing::_))
      .Times(0);

  InvokeFromWhatsNew(GetPassKey(), *mock_service_, &mock_browser_window_);
}

TEST_F(GlicWhatsNewInvokeTest, ForQuizzes_InvokeFeatureDisabled_CallsToggleUI) {
  EXPECT_CALL(*mock_service_, ToggleUI(&mock_browser_window_,
                                       /*prevent_close=*/false,
                                       mojom::InvocationSource::kWhatsNew))
      .Times(1);
  EXPECT_CALL(*mock_service_, Invoke(testing::_)).Times(0);
  EXPECT_CALL(*mock_service_, InvokeWithAutoSubmit(testing::_, testing::_))
      .Times(0);

  InvokeFromWhatsNewForQuizzes(GetPassKey(), *mock_service_,
                               &mock_browser_window_);
}

class GlicWhatsNewInvokeEnabledTest : public GlicWhatsNewInvokeTest {
 protected:
  GlicWhatsNewInvokeEnabledTest()
      : GlicWhatsNewInvokeTest(
            /*enabled_features=*/{{features::kGlicBrowserCommandInvoke, {}}},
            /*disabled_features=*/{features::kGlicWhatsNewQuizzes}) {}
};

TEST_F(GlicWhatsNewInvokeEnabledTest,
       InvokeFeatureEnabled_CallsInvokeWithoutPrompt) {
  std::optional<GlicInvokeOptions> captured_options;
  EXPECT_CALL(*mock_service_, ToggleUI(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_service_, InvokeWithAutoSubmit(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_service_, Invoke(testing::_))
      .WillOnce([&](GlicInvokeOptions options) {
        captured_options = std::move(options);
        return base::WeakPtr<GlicInstance>();
      });

  InvokeFromWhatsNew(GetPassKey(), *mock_service_, &mock_browser_window_);

  ASSERT_TRUE(captured_options.has_value());
  EXPECT_EQ(captured_options->GetInvocationSource(),
            mojom::InvocationSource::kWhatsNew);
  EXPECT_EQ(captured_options->fre_override,
            mojom::FreOverride::kTrustFirstInline);
  EXPECT_TRUE(captured_options->prompts.empty());
  ASSERT_TRUE(
      std::holds_alternative<DefaultSurface>(captured_options->target.surface));
  EXPECT_EQ(std::get<DefaultSurface>(captured_options->target.surface).browser,
            &mock_browser_window_);
}

TEST_F(GlicWhatsNewInvokeEnabledTest,
       ForQuizzes_QuizzesDisabled_CallsInvokeWithoutAutoSubmit) {
  std::optional<GlicInvokeOptions> captured_options;
  EXPECT_CALL(*mock_service_, ToggleUI(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_service_, InvokeWithAutoSubmit(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_service_, Invoke(testing::_))
      .WillOnce([&](GlicInvokeOptions options) {
        captured_options = std::move(options);
        return base::WeakPtr<GlicInstance>();
      });

  InvokeFromWhatsNewForQuizzes(GetPassKey(), *mock_service_,
                               &mock_browser_window_);

  ASSERT_TRUE(captured_options.has_value());
  EXPECT_TRUE(captured_options->prompts.empty());
}

class GlicWhatsNewInvokeQuizzesTest : public GlicWhatsNewInvokeTest {
 protected:
  GlicWhatsNewInvokeQuizzesTest()
      : GlicWhatsNewInvokeTest(
            /*enabled_features=*/
            {{features::kGlicBrowserCommandInvoke, {}},
             {features::kGlicWhatsNewQuizzes, {}}},
            /*disabled_features=*/{}) {}
};

TEST_F(GlicWhatsNewInvokeQuizzesTest,
       OpenGlic_EvenWithQuizzesEnabled_CallsInvokeWithoutPromptOrAutoSubmit) {
  std::optional<GlicInvokeOptions> captured_options;
  EXPECT_CALL(*mock_service_, ToggleUI(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_service_, InvokeWithAutoSubmit(testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_service_, Invoke(testing::_))
      .WillOnce([&](GlicInvokeOptions options) {
        captured_options = std::move(options);
        return base::WeakPtr<GlicInstance>();
      });

  InvokeFromWhatsNew(GetPassKey(), *mock_service_, &mock_browser_window_);

  ASSERT_TRUE(captured_options.has_value());
  EXPECT_TRUE(captured_options->prompts.empty());
}

TEST_F(GlicWhatsNewInvokeQuizzesTest,
       ForQuizzes_QuizzesEnabled_CallsInvokeWithAutoSubmitAndQuizzesPrompt) {
  const std::string expected_prompt =
      l10n_util::GetStringUTF8(IDS_GLIC_WHATS_NEW_QUIZZES_PROMPT);
  ASSERT_FALSE(expected_prompt.empty());

  std::optional<GlicInvokeOptions> captured_options;
  EXPECT_CALL(*mock_service_, ToggleUI(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_service_, Invoke(testing::_)).Times(0);
  EXPECT_CALL(*mock_service_, InvokeWithAutoSubmit(testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options) {
        captured_options = std::move(options);
        return base::WeakPtr<GlicInstance>();
      });

  InvokeFromWhatsNewForQuizzes(GetPassKey(), *mock_service_,
                               &mock_browser_window_);

  ASSERT_TRUE(captured_options.has_value());
  EXPECT_EQ(captured_options->GetInvocationSource(),
            mojom::InvocationSource::kWhatsNew);
  EXPECT_EQ(captured_options->fre_override,
            mojom::FreOverride::kTrustFirstInline);
  EXPECT_THAT(captured_options->prompts, testing::ElementsAre(expected_prompt));
  ASSERT_TRUE(
      std::holds_alternative<DefaultSurface>(captured_options->target.surface));
  EXPECT_EQ(std::get<DefaultSurface>(captured_options->target.surface).browser,
            &mock_browser_window_);
}

TEST_F(GlicWhatsNewInvokeQuizzesTest,
       BrowserCommandHandlerWithoutPasskey_CallsToggleUI) {
  EXPECT_CALL(*mock_service_, ToggleUI(nullptr, /*prevent_close=*/false,
                                       mojom::InvocationSource::kWhatsNew))
      .Times(2);
  EXPECT_CALL(*mock_service_, Invoke(testing::_)).Times(0);
  EXPECT_CALL(*mock_service_, InvokeWithAutoSubmit(testing::_, testing::_))
      .Times(0);

  BrowserCommandHandler handler(
      mojo::PendingReceiver<browser_command::mojom::CommandHandler>(), profile_,
      {browser_command::mojom::Command::kOpenGlic,
       browser_command::mojom::Command::kOpenGlicForQuizzes},
      /*web_contents=*/nullptr);
  handler.ExecuteCommand(browser_command::mojom::Command::kOpenGlic,
                         browser_command::mojom::ClickInfo::New(),
                         base::DoNothing());
  handler.ExecuteCommand(browser_command::mojom::Command::kOpenGlicForQuizzes,
                         browser_command::mojom::ClickInfo::New(),
                         base::DoNothing());
}

TEST_F(GlicWhatsNewInvokeQuizzesTest,
       BrowserCommandHandlerWithPasskey_RoutesOpenGlicAndOpenGlicForQuizzes) {
  const std::string expected_prompt =
      l10n_util::GetStringUTF8(IDS_GLIC_WHATS_NEW_QUIZZES_PROMPT);
  ASSERT_FALSE(expected_prompt.empty());

  std::optional<GlicInvokeOptions> open_glic_options;
  std::optional<GlicInvokeOptions> quizzes_options;
  EXPECT_CALL(*mock_service_, ToggleUI(testing::_, testing::_, testing::_))
      .Times(0);
  EXPECT_CALL(*mock_service_, Invoke(testing::_))
      .WillOnce([&](GlicInvokeOptions options) {
        open_glic_options = std::move(options);
        return base::WeakPtr<GlicInstance>();
      });
  EXPECT_CALL(*mock_service_, InvokeWithAutoSubmit(testing::_, testing::_))
      .WillOnce([&](InvokeWithAutoSubmitPasskey, GlicInvokeOptions options) {
        quizzes_options = std::move(options);
        return base::WeakPtr<GlicInstance>();
      });

  BrowserCommandHandler handler(
      mojo::PendingReceiver<browser_command::mojom::CommandHandler>(), profile_,
      {browser_command::mojom::Command::kOpenGlic,
       browser_command::mojom::Command::kOpenGlicForQuizzes},
      /*web_contents=*/nullptr, GetPassKey());
  handler.ExecuteCommand(browser_command::mojom::Command::kOpenGlic,
                         browser_command::mojom::ClickInfo::New(),
                         base::DoNothing());
  handler.ExecuteCommand(browser_command::mojom::Command::kOpenGlicForQuizzes,
                         browser_command::mojom::ClickInfo::New(),
                         base::DoNothing());

  ASSERT_TRUE(open_glic_options.has_value());
  EXPECT_TRUE(open_glic_options->prompts.empty());
  ASSERT_TRUE(quizzes_options.has_value());
  EXPECT_THAT(quizzes_options->prompts, testing::ElementsAre(expected_prompt));
}

class GlicWhatsNewAnchoredProfileTest : public GlicWhatsNewInvokeTest {
 protected:
  GlicWhatsNewAnchoredProfileTest()
      : GlicWhatsNewInvokeTest(
            /*enabled_features=*/
            {{features::kGlic, {}},
             {features::kGlicAnchorEntryPointForOnboardedUsers, {}},
             {features::kGlicBrowserCommandInvoke, {}},
             {features::kGlicWhatsNewQuizzes, {}}},
            /*disabled_features=*/{features::kGlicCountryFiltering,
                                   features::kGlicLocaleFiltering,
                                   features::kGlicTieredRollout,
                                   features::kGlicUserStatusCheck}) {}
};

TEST_F(GlicWhatsNewAnchoredProfileTest,
       AnchoredProfileNotEnabled_InvokeFeatureEnabled_CallsToggleUI) {
  SigninWithPrimaryAccount(profile_);
  SetGlicCapability(profile_, false);
  SetFRECompletion(profile_, prefs::FreStatus::kCompleted);
  scoped_glic_bypass_.reset();

  ASSERT_TRUE(GlicEnabling::ShouldShowGlicButton(profile_));
  ASSERT_FALSE(GlicEnabling::IsEnabledForProfile(profile_));

  EXPECT_CALL(*mock_service_, ToggleUI(&mock_browser_window_,
                                       /*prevent_close=*/false,
                                       mojom::InvocationSource::kWhatsNew))
      .Times(2);
  EXPECT_CALL(*mock_service_, Invoke(testing::_)).Times(0);
  EXPECT_CALL(*mock_service_, InvokeWithAutoSubmit(testing::_, testing::_))
      .Times(0);

  InvokeFromWhatsNew(GetPassKey(), *mock_service_, &mock_browser_window_);
  InvokeFromWhatsNewForQuizzes(GetPassKey(), *mock_service_,
                               &mock_browser_window_);
}

}  // namespace
}  // namespace glic
