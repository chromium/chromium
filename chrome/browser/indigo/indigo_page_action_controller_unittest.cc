// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_page_action_controller.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/indigo_component_installer.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/indigo/indigo_image_replacement_manager.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"
#include "chrome/browser/indigo/proto/indigo_prompts.pb.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/skills/mocks/mock_skills_service.h"
#include "components/skills/public/skill.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/image/image_skia.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace indigo {
namespace {

using ::optimization_guide::OptimizationGuideDecision;
using ::testing::_;

// Matcher to verify that GlicInvokeOptions has a specific prompt.
auto HasGlicPrompt(std::string_view prompt) {
  return ::testing::Field("prompts", &glic::GlicInvokeOptions::prompts,
                          ::testing::ElementsAre(prompt));
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class MockSigninUiDelegate : public signin_ui_util::SigninUiDelegate {
 public:
  MOCK_METHOD(void,
              ShowSigninUI,
              (Profile*,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction),
              (override));
  MOCK_METHOD(void,
              ShowReauthUI,
              (Profile*,
               const std::string&,
               bool,
               signin_metrics::AccessPoint,
               signin_metrics::PromoAction),
              (override));
};
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kSignOutSupportedOnPlatform = false;
#else
constexpr bool kSignOutSupportedOnPlatform = true;
#endif  // BUILDFLAG(IS_CHROMEOS)

struct CreateControllerOptions {
  bool expect_register_optimization_types = true;
};

class IndigoPageActionControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIndigo,
        {{"indigo_onboarding_url", "https://example.com/onboard"}});
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        "indigo-script", "/dummy/path");
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
    // SetUpGlobalFeaturesForTesting is required to initialize
    // GlicGlobalEnabling which is checked by GlicEnabling.
    testing_profile_manager_ =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(true);
  }

  void TearDown() override {
    controller_.reset();
    page_action_controller_.reset();
    tab_interface_.reset();
    mock_optimization_guide_ = nullptr;
    mock_glic_keyed_service_ = nullptr;
    mock_skills_service_ = nullptr;
    identity_test_env_adaptor_.reset();
    profile_.reset();
    testing_profile_manager_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
  }

  void CreateController(CreateControllerOptions options = {}) {
    CHECK(!controller_);

    if (!profile_) {
      TestingProfile::Builder builder;
      builder.AddTestingFactory(
          OptimizationGuideKeyedServiceFactory::GetInstance(),
          base::BindRepeating([](content::BrowserContext* context)
                                  -> std::unique_ptr<KeyedService> {
            return std::make_unique<
                testing::NiceMock<MockOptimizationGuideKeyedService>>();
          }));
      builder.AddTestingFactory(
          skills::SkillsServiceFactory::GetInstance(),
          base::BindRepeating([](content::BrowserContext* context)
                                  -> std::unique_ptr<KeyedService> {
            return std::make_unique<
                testing::NiceMock<skills::MockSkillsService>>();
          }));
      builder.AddTestingFactory(
          glic::GlicKeyedServiceFactory::GetInstance(),
          base::BindRepeating(
              [](ProfileManager* profile_manager,
                 glic::GlicProfileManager* glic_profile_manager,
                 content::BrowserContext* context)
                  -> std::unique_ptr<KeyedService> {
                return std::make_unique<
                    testing::NiceMock<glic::MockGlicKeyedService>>(
                    context,
                    IdentityManagerFactory::GetForProfile(
                        Profile::FromBrowserContext(context)),
                    profile_manager, glic_profile_manager, nullptr, nullptr);
              },
              testing_profile_manager_->profile_manager(),
              glic::GlicProfileManager::GetInstance()));
      profile_ = IdentityTestEnvironmentProfileAdaptor::
          CreateProfileForIdentityTestEnvironment(builder);

      identity_test_env_adaptor_ =
          std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
              profile_.get());
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("user@example.com",
                                        signin::ConsentLevel::kSignin);

      mock_optimization_guide_ =
          static_cast<testing::NiceMock<MockOptimizationGuideKeyedService>*>(
              OptimizationGuideKeyedServiceFactory::GetForProfile(
                  profile_.get()));

      mock_skills_service_ =
          static_cast<testing::NiceMock<skills::MockSkillsService>*>(
              skills::SkillsServiceFactory::GetForProfile(profile_.get()));
      CHECK(mock_skills_service_);

      SetModelExecutionCapability(true);

      mock_glic_keyed_service_ =
          static_cast<testing::NiceMock<glic::MockGlicKeyedService>*>(
              glic::GlicKeyedServiceFactory::GetGlicKeyedService(
                  profile_.get(), /*create=*/true));
      CHECK(mock_glic_keyed_service_);
    }

    tab_interface_ =
        std::make_unique<page_actions::FakeTabInterface>(profile_.get());
    tabs::TabLookupFromWebContents::CreateForWebContents(
        tab_interface_->GetContents(), tab_interface_.get());
    ON_CALL(*tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));

    page_action_controller_ =
        std::make_unique<page_actions::MockPageActionController>();

    // The service is only created when
    // `optimization_guide::features::IsOptimizationHintsEnabled()` returns
    // true.
    if (mock_optimization_guide_) {
      if (options.expect_register_optimization_types) {
        EXPECT_CALL(*mock_optimization_guide_,
                    RegisterOptimizationTypes(testing::ElementsAre(
                        optimization_guide::proto::OptimizationType::INDIGO)));
      } else {
        EXPECT_CALL(*mock_optimization_guide_,
                    RegisterOptimizationTypes(::testing::Contains(
                        optimization_guide::proto::OptimizationType::INDIGO)))
            .Times(0);
      }
    } else {
      EXPECT_FALSE(options.expect_register_optimization_types)
          << "Cannot expect registration when OptimizationGuideKeyedService "
             "was not created";
    }
    controller_ = std::make_unique<IndigoPageActionController>(
        *tab_interface_, *page_action_controller_);
  }

  void ExpectOptimizationGuideDecision(const GURL& url,
                                       OptimizationGuideDecision decision) {
    EXPECT_CALL(
        *mock_optimization_guide_,
        CanApplyOptimization(
            url, optimization_guide::proto::OptimizationType::INDIGO,
            testing::An<
                optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            decision, optimization_guide::OptimizationMetadata()));
  }

  void SetupEligibleAndOnboarded() {
    profile_->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, true);
    IndigoServiceFactory::GetForProfile(profile_.get())
        ->SetRemoteEligibilityFetcherForTesting(base::BindRepeating(
            [](IndigoService::RemoteEligibilityCallback callback) {
              std::move(callback).Run(
                  RemoteEligibility{.is_service_supported_for_account = true,
                                    .has_user_image = true});
            }));
  }

  void SetModelExecutionCapability(bool can_use_model_execution_features) {
    signin::IdentityManager* identity_manager =
        identity_test_env_adaptor_->identity_test_env()->identity_manager();
    AccountInfo account_info =
        identity_manager->FindExtendedAccountInfoByAccountId(
            identity_manager->GetPrimaryAccountId(
                signin::ConsentLevel::kSignin));
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(
        can_use_model_execution_features);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
  }

  void SetupComponentWithPrompts(
      const base::FilePath& temp_dir_path,
      const std::vector<std::pair<std::string, std::string>>& prompts) {
    chrome::aix::indigo::IndigoPrompts proto;
    for (const auto& [key, prompt_text] : prompts) {
      auto* prompt = proto.add_prompts();
      prompt->set_key(key);
      prompt->set_prompt(prompt_text);
    }

    base::FilePath prompts_path =
        temp_dir_path.Append(FILE_PATH_LITERAL("indigo_prompts.bin"));
    std::string serialized;
    ASSERT_TRUE(proto.SerializeToString(&serialized));
    ASSERT_TRUE(base::WriteFile(prompts_path, serialized));

    base::test::TestFuture<void> prompts_loaded_future;
    IndigoServiceFactory::GetForProfile(profile_.get())
        ->SetPromptsLoadedCallbackForTesting(
            prompts_loaded_future.GetCallback());

    component_updater::IndigoComponentInstallerPolicy policy;
    policy.ComponentReady(base::Version("1.0"), temp_dir_path,
                          base::DictValue());

    EXPECT_TRUE(prompts_loaded_future.Wait());
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
#if BUILDFLAG(IS_CHROMEOS)
  // Needed because TestWebContents ends up creating BTM classes which depend
  // on this on ChromeOS.
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  raw_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<testing::NiceMock<glic::MockGlicKeyedService>>
      mock_glic_keyed_service_ = nullptr;
  raw_ptr<testing::NiceMock<skills::MockSkillsService>> mock_skills_service_ =
      nullptr;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<testing::NiceMock<MockOptimizationGuideKeyedService>>
      mock_optimization_guide_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<page_actions::FakeTabInterface> tab_interface_;
  std::unique_ptr<page_actions::MockPageActionController>
      page_action_controller_;
  std::unique_ptr<IndigoPageActionController> controller_;
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(IndigoPageActionControllerTest, ShowsWhenOptimizationGuideReturnsTrue) {
  CreateController();

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
}

TEST_F(IndigoPageActionControllerTest, HidesWhenOptimizationGuideReturnsFalse) {
  CreateController();

  // First, simulate a navigation where the decision is true so it gets shown.
  GURL url1("https://example.com");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);

  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();

  // Now expect Hide when navigating to a page where the decision is false.
  EXPECT_CALL(*page_action_controller_, Hide(kActionIndigo));

  GURL url2("https://example2.com");
  ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kFalse);

  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      url2, tab_interface_->GetContents());
  navigation2->Commit();
}

TEST_F(IndigoPageActionControllerTest, UpdatesOnSameDocumentNavigation) {
  CreateController();

  // First, simulate a navigation where the decision is true so it gets shown.
  GURL url1("https://example.com/page1");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);

  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();

  // Now expect Hide when performing a same-document navigation to a page where
  // the decision is false.
  EXPECT_CALL(*page_action_controller_, Hide(kActionIndigo));

  GURL url2("https://example.com/page2");
  ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kFalse);

  auto navigation2 = content::NavigationSimulator::CreateRendererInitiated(
      url2, tab_interface_->GetContents()->GetPrimaryMainFrame());
  navigation2->CommitSameDocument();
}

TEST_F(IndigoPageActionControllerTest, IgnoresFragmentOnlyNavigation) {
  CreateController();

  GURL url1("https://example.com/page");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);

  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();

  testing::Mock::VerifyAndClearExpectations(mock_optimization_guide_);
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Now perform a same-document navigation that only changes the fragment.
  // We expect NO calls to mock_optimization_guide_ or page_action_controller_.
  EXPECT_CALL(
      *mock_optimization_guide_,
      CanApplyOptimization(
          _, _,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);
  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);
  EXPECT_CALL(*page_action_controller_, Hide(_)).Times(0);

  GURL url2("https://example.com/page#fragment");
  auto navigation2 = content::NavigationSimulator::CreateRendererInitiated(
      url2, tab_interface_->GetContents()->GetPrimaryMainFrame());
  navigation2->CommitSameDocument();
}

TEST_F(IndigoPageActionControllerTest, QueriesOptimizationGuideOnReload) {
  CreateController();

  GURL url("https://example.com");

  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation1->Commit();

  testing::Mock::VerifyAndClearExpectations(mock_optimization_guide_);
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Reloading the same URL should query optimization guide again.
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));

  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation2->Commit();
}

TEST_F(IndigoPageActionControllerTest,
       HidesWhenOptimizationHintsFeatureIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      optimization_guide::features::kOptimizationHints);

  CreateController({.expect_register_optimization_types = false});

  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);
  GURL url("https://example.com");
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
}

TEST_F(IndigoPageActionControllerTest, HidesWhenNotSignedIn) {
  if constexpr (!kSignOutSupportedOnPlatform) {
    GTEST_SKIP() << "Sign out is not supported on this platform.";
  }

  CreateController();
  identity_test_env_adaptor_->identity_test_env()->ClearPrimaryAccount();

  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
}

TEST_F(IndigoPageActionControllerTest, HidesWhenCapabilityIsFalse) {
  CreateController();
  SetModelExecutionCapability(false);

  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
}

TEST_F(IndigoPageActionControllerTest, ShowsWhenCapabilityIsTrue) {
  CreateController();
  SetModelExecutionCapability(true);

  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
}

TEST_F(IndigoPageActionControllerTest, UpdatesWhenCapabilityChanges) {
  CreateController();

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  SetModelExecutionCapability(true);

  // Show the action initially.
  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Hide when capability becomes false.
  EXPECT_CALL(*page_action_controller_, Hide(kActionIndigo));
  SetModelExecutionCapability(false);
  ::testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Show again when capability becomes true.
  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));
  SetModelExecutionCapability(true);
}

TEST_F(IndigoPageActionControllerTest, UpdatesWhenAccountChanges) {
  if constexpr (!kSignOutSupportedOnPlatform) {
    GTEST_SKIP() << "Sign out is not supported on this platform.";
  }

  CreateController();

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  SetModelExecutionCapability(true);

  // Show the action initially.
  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Hide when signing out.
  EXPECT_CALL(*page_action_controller_, Hide(kActionIndigo));
  identity_test_env_adaptor_->identity_test_env()->ClearPrimaryAccount();
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Show again when signing back in.
  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));
  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "user2@example.com", signin::ConsentLevel::kSignin);
  SetModelExecutionCapability(true);
}

TEST_F(IndigoPageActionControllerTest, ShowsAnchoredMessageThenSuggestionChip) {
  CreateController();

  // First navigation: show an anchored message.
  {
    GURL url("https://example.com/1");
    ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

    EXPECT_CALL(
        *page_action_controller_,
        SetAnchoredMessageText(
            kActionIndigo, l10n_util::GetStringUTF16(
                               IDS_INDIGO_ENTRYPOINT_ANCHORED_MESSAGE_TEXT)));
    gfx::ImageSkia* expected_skia =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_GLIC_BUTTON_ALT_ICON);
    EXPECT_CALL(
        *page_action_controller_,
        SetAnchoredMessageIcon(
            kActionIndigo,
            testing::Truly([expected_skia](const ui::ImageModel& model) {
              return expected_skia
                         ? (model.IsImage() &&
                            model.GetImage().AsImageSkia().BackedBySameObjectAs(
                                *expected_skia))
                         : model.IsEmpty();
            })));
    EXPECT_CALL(
        *page_action_controller_,
        ShowAnchoredMessage(
            kActionIndigo,
            page_actions::AnchoredMessageConfig{
                .priority =
                    page_actions::PageActionPriorityCategory::kContextualCue}))
        .WillOnce(testing::InvokeWithoutArgs([&]() {
          page_actions::PageActionState state;
          state.action_id = kActionIndigo;
          state.anchored_message_showing = true;
          controller_->OnPageActionAnchoredMessageShown(state);
        }));
    EXPECT_CALL(*page_action_controller_, ShowSuggestionChip(_, _)).Times(0);

    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, tab_interface_->GetContents());
    navigation->Commit();
  }

  // Second navigation (soon after): show a suggestion chip instead of an
  // anchored message.
  {
    GURL url("https://example.com/2");
    ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

    EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _)).Times(0);
    EXPECT_CALL(*page_action_controller_, ShowSuggestionChip(kActionIndigo, _));

    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        url, tab_interface_->GetContents());
    navigation->Commit();
  }
}

TEST_F(IndigoPageActionControllerTest, InvokeActionTriggersEligibilityCheck) {
  CreateController();

  base::test::TestFuture<void> fetcher_called;
  IndigoServiceFactory::GetForProfile(profile_.get())
      ->SetRemoteEligibilityFetcherForTesting(base::BindLambdaForTesting(
          [&](IndigoService::RemoteEligibilityCallback callback) {
            fetcher_called.SetValue();
            std::move(callback).Run(RemoteEligibility{});
          }));

  controller_->InvokeAction(EntryPoint::kSuggestionChip);
  EXPECT_TRUE(fetcher_called.Wait());
}

TEST_F(IndigoPageActionControllerTest, OnboardingSuccessTriggersContinuation) {
  CreateController();

  // Explicitly set the initial state for onboarding preference.
  profile_->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, false);

  base::UserActionTester user_action_tester;

  OnboardingResult result;
  result.acknowledge_chrome_disclaimer = true;

  base::test::TestFuture<void> fetcher_called;
  IndigoServiceFactory::GetForProfile(profile_.get())
      ->SetRemoteEligibilityFetcherForTesting(base::BindLambdaForTesting(
          [&](IndigoService::RemoteEligibilityCallback callback) {
            fetcher_called.SetValue();
            std::move(callback).Run(RemoteEligibility{});
          }));

  // Initial state: eligible but needs onboarding. This should show the dialog.
  CombinedEligibility eligibility;
  eligibility.local_eligibility = LocalEligibility::kEligible;
  eligibility.remote_eligibility = RemoteEligibility{
      .is_service_supported_for_account = true, .has_user_image = false};
  eligibility.has_onboarded_pref = false;

  base::OnceCallback<void(const OnboardingResult&)> captured_callback;
  IndigoPageActionController::TestApi(controller_.get())
      .SetOnboardingDialogFactory(base::BindLambdaForTesting(
          [&](tabs::TabInterface& tab, const GURL& url,
              base::OnceCallback<void(const OnboardingResult&)> callback)
              -> std::unique_ptr<IndigoOnboardingDialog> {
            captured_callback = std::move(callback);
            return nullptr;
          }));

  IndigoPageActionController::TestApi(controller_.get())
      .CheckEligibilityForOnboarding(eligibility);

  ASSERT_TRUE(!captured_callback.is_null());
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Trigger"), 1);

  // Now simulate the dialog closing with success.
  std::move(captured_callback).Run(result);

  // Closing with success set the pref and trigger a re-fetch for continuation.
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kIndigoHasOnboarded));
  EXPECT_TRUE(fetcher_called.Wait());
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Complete"), 1);
}

TEST_F(IndigoPageActionControllerTest, OnboardingCancelledDoesNotTrigger) {
  CreateController();

  // Explicitly set the initial state for onboarding preference.
  profile_->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, false);

  base::UserActionTester user_action_tester;

  OnboardingResult result;
  result.acknowledge_chrome_disclaimer = false;

  base::test::TestFuture<void> fetcher_called;
  IndigoServiceFactory::GetForProfile(profile_.get())
      ->SetRemoteEligibilityFetcherForTesting(base::BindLambdaForTesting(
          [&](IndigoService::RemoteEligibilityCallback callback) {
            fetcher_called.SetValue();
            std::move(callback).Run(RemoteEligibility{});
          }));

  IndigoPageActionController::TestApi(controller_.get())
      .CheckOnboardingResult(OnboardingDisposition::kDefault, result);

  EXPECT_FALSE(fetcher_called.IsReady());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kIndigoHasOnboarded));
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Complete"), 0);
}

TEST_F(IndigoPageActionControllerTest,
       OnReplaceOriginalPhotoTriggersOnboardingWithParam) {
  CreateController();

  profile_->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, true);

  base::UserActionTester user_action_tester;

  GURL captured_url;
  base::OnceCallback<void(const OnboardingResult&)> captured_callback;
  IndigoPageActionController::TestApi(controller_.get())
      .SetOnboardingDialogFactory(base::BindLambdaForTesting(
          [&](tabs::TabInterface& tab, const GURL& url,
              base::OnceCallback<void(const OnboardingResult&)> callback)
              -> std::unique_ptr<IndigoOnboardingDialog> {
            captured_url = url;
            captured_callback = std::move(callback);
            return nullptr;
          }));

  controller_->OnReplaceOriginalPhoto(nullptr);

  EXPECT_EQ(captured_url, GURL("https://example.com/onboard?toyri=1"));
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.ReplaceImage.Trigger"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Trigger"), 0);

  // Simulate success.
  OnboardingResult result;
  result.acknowledge_chrome_disclaimer = true;
  std::move(captured_callback).Run(result);

  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.ReplaceImage.Complete"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.Onboarding.Complete"), 0);
}

TEST_F(IndigoPageActionControllerTest, OnCloseResetsReplacements) {
  CreateController();

  GURL url("https://example.com");
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  content::WebContents* web_contents = tab_interface_->GetContents();
  auto* manager = IndigoImageReplacementManager::GetOrCreateForPage(
      web_contents->GetPrimaryPage());

  base::test::TestFuture<void> disconnect_future;

  class FakeImageReplacement : public blink::mojom::ImageReplacement {
   public:
    FakeImageReplacement() = default;
    void StartReplacement(
        mojo::PendingRemote<blink::mojom::ImageReplacementHost> host,
        std::optional<int32_t> tracked_element_feature_id) override {
      // Do nothing.
    }
    void RenderReplacement() override {}
  };

  FakeImageReplacement fake_replacement;
  mojo::Receiver<blink::mojom::ImageReplacement> receiver(&fake_replacement);

  manager->RegisterImageReplacement(receiver.BindNewPipeAndPassRemote(),
                                    /*is_primary=*/true);
  receiver.set_disconnect_handler(disconnect_future.GetCallback());

  controller_->OnClose(nullptr);

  EXPECT_TRUE(disconnect_future.Wait());
}

TEST_F(IndigoPageActionControllerTest,
       InvokeActionOpensGlicForAnchoredMessage) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("test prompt")))
      .WillOnce(::testing::Return(base::WeakPtr<glic::GlicInstance>()));

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _));

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  controller_->InvokeAction(EntryPoint::kAnchoredMessage);
}

TEST_F(IndigoPageActionControllerTest, InvokeActionOpensGlicForSuggestionChip) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "test prompt"}});

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("test prompt")))
      .WillOnce(::testing::Return(base::WeakPtr<glic::GlicInstance>()));

  {
    GURL url1("https://example.com/1");
    ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);
    EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _))
        .WillOnce(testing::InvokeWithoutArgs([&]() {
          page_actions::PageActionState state;
          state.action_id = kActionIndigo;
          state.anchored_message_showing = true;
          controller_->OnPageActionAnchoredMessageShown(state);
        }));
    auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
        url1, tab_interface_->GetContents());
    navigation1->Commit();
  }

  {
    GURL url2("https://example.com/2");
    ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kTrue);
    EXPECT_CALL(*page_action_controller_, ShowSuggestionChip(kActionIndigo, _));
    auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
        url2, tab_interface_->GetContents());
    navigation2->Commit();
  }

  controller_->InvokeAction(EntryPoint::kSuggestionChip);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
TEST_F(IndigoPageActionControllerTest, InvokeActionTriggerReauthWhenPaused) {
  CreateController();

  // Set the account in paused state.
  identity_test_env_adaptor_->identity_test_env()
      ->SetInvalidRefreshTokenForPrimaryAccount();

  base::HistogramTester histogram_tester;

  // We expect ShowReauthUI to be called on the mock delegate.
  testing::StrictMock<MockSigninUiDelegate> mock_signin_ui_delegate;
  base::AutoReset<signin_ui_util::SigninUiDelegate*> delegate_auto_reset =
      signin_ui_util::SetSigninUiDelegateForTesting(&mock_signin_ui_delegate);

  EXPECT_CALL(
      mock_signin_ui_delegate,
      ShowReauthUI(profile_.get(), "user@example.com",
                   /*enable_sync=*/false, signin_metrics::AccessPoint::kIndigo,
                   signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO));

  controller_->InvokeAction(EntryPoint::kSuggestionChip);

  histogram_tester.ExpectUniqueSample(
      "Indigo.Transformation.Result",
      static_cast<int>(
          IndigoTransformationResult::kRefreshTokenInPersistentErrorState),
      1);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

TEST_F(IndigoPageActionControllerTest, InvokeActionOpensGlicWithProtoPrompt) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CreateController();
  SetupEligibleAndOnboarded();
  SetupComponentWithPrompts(temp_dir.GetPath(), {{"v5", "proto test prompt"}});

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt_key", "v5"}});

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("proto test prompt")))
      .WillOnce(::testing::Return(base::WeakPtr<glic::GlicInstance>()));

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _));

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  controller_->InvokeAction(EntryPoint::kAnchoredMessage);
}

TEST_F(IndigoPageActionControllerTest,
       InvokeActionOpensGlicWithOverridePromptPrecedence) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CreateController();
  SetupEligibleAndOnboarded();
  SetupComponentWithPrompts(temp_dir.GetPath(), {{"v5", "proto test prompt"}});

  base::test::ScopedFeatureList local_feature_list;
  // Both override and key are set. Override should win.
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt", "override prompt"},
                                  {"indigo_glic_prompt_key", "v5"}});

  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("override prompt")))
      .WillOnce(::testing::Return(base::WeakPtr<glic::GlicInstance>()));

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _));

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  controller_->InvokeAction(EntryPoint::kAnchoredMessage);
}

TEST_F(IndigoPageActionControllerTest,
       InvokeActionDoesNotOpenGlicIfPromptNotFound) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  CreateController();
  SetupEligibleAndOnboarded();
  SetupComponentWithPrompts(temp_dir.GetPath(), {});

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_prompt_key", "v5"}});

  // Expect NO call to InvokeWithAutoSubmit.
  EXPECT_CALL(*mock_glic_keyed_service_, InvokeWithAutoSubmit(_, _)).Times(0);

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _));

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  controller_->InvokeAction(EntryPoint::kAnchoredMessage);
}

TEST_F(IndigoPageActionControllerTest, InvokeActionOpensGlicWithSkill) {
  CreateController();
  SetupEligibleAndOnboarded();

  base::test::ScopedFeatureList local_feature_list;
  local_feature_list.InitAndEnableFeatureWithParameters(
      features::kIndigoOpenGlic, {{"indigo_glic_skill_id", "test_skill_id"}});

  // Mock the skill lookup
  skills::Skill mock_skill;
  mock_skill.id = "test_skill_id";
  mock_skill.prompt = "skill test prompt";
  mock_skill.source = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;

  EXPECT_CALL(*mock_skills_service_, GetSkillById("test_skill_id"))
      .WillOnce(testing::Return(&mock_skill));

  // Verify Glic is called with the skill prompt
  EXPECT_CALL(*mock_glic_keyed_service_,
              InvokeWithAutoSubmit(_, HasGlicPrompt("skill test prompt")))
      .WillOnce(::testing::Return(base::WeakPtr<glic::GlicInstance>()));

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);
  EXPECT_CALL(*page_action_controller_, ShowAnchoredMessage(_, _));

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  controller_->InvokeAction(EntryPoint::kAnchoredMessage);
}

TEST_F(IndigoPageActionControllerTest,
       InvokeActionSuggestionChipRecordsMetrics) {
  CreateController();
  base::UserActionTester user_action_tester;
  controller_->InvokeAction(EntryPoint::kSuggestionChip);
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.PageAction.Click"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Indigo.PageAction.SuggestionChip.Click"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Indigo.PageAction.AnchoredMessage.Click"),
            0);
}

TEST_F(IndigoPageActionControllerTest,
       InvokeActionAnchoredMessageRecordsMetrics) {
  CreateController();
  base::UserActionTester user_action_tester;
  controller_->InvokeAction(EntryPoint::kAnchoredMessage);
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.PageAction.Click"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Indigo.PageAction.SuggestionChip.Click"),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Indigo.PageAction.AnchoredMessage.Click"),
            1);
}

TEST_F(IndigoPageActionControllerTest, InvokeActionErrorToastRecordsMetrics) {
  CreateController();
  base::UserActionTester user_action_tester;
  controller_->InvokeAction(EntryPoint::kErrorToast);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Indigo.PageAction.SuggestionChip.Click"),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Indigo.PageAction.AnchoredMessage.Click"),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount("Indigo.ErrorToast.Retry.Click"),
            1);
}

TEST_F(IndigoPageActionControllerTest, OnPageActionAnchoredMessageShown) {
  CreateController();

  auto* service = IndigoServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service->CanShowAnchoredMessage());

  base::UserActionTester user_action_tester;
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Indigo.PageAction.ShowAnchoredMessage"),
            0);

  // Navigate to trigger UpdateEntryPointsState and attempt to show the anchored
  // message.
  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();

  // Simply attempting to show the anchored message (via UpdateEntryPointsState
  // during navigation) should NOT be enough to record the user action or
  // update the service's state.
  EXPECT_TRUE(service->CanShowAnchoredMessage());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Indigo.PageAction.ShowAnchoredMessage"),
            0);

  // Trigger the observer event, simulating the anchored message actually
  // showing.
  page_actions::PageActionState state;
  state.action_id = kActionIndigo;
  state.anchored_message_showing = true;
  controller_->OnPageActionAnchoredMessageShown(state);

  // Verify that the service was notified and the action was recorded.
  EXPECT_FALSE(service->CanShowAnchoredMessage());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Indigo.PageAction.ShowAnchoredMessage"),
            1);
}

}  // namespace
}  // namespace indigo
