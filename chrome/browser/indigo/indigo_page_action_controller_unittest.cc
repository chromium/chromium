// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_page_action_controller.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/indigo/onboarding/indigo_onboarding_dialog.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace indigo {
namespace {

using ::optimization_guide::OptimizationGuideDecision;
using ::testing::_;

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
    feature_list_.InitAndEnableFeature(features::kIndigo);
  }

  void TearDown() override {
    controller_.reset();
    page_action_controller_.reset();
    tab_interface_.reset();
    mock_optimization_guide_ = nullptr;
    identity_test_env_adaptor_.reset();
    profile_.reset();
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

      SetModelExecutionCapability(true);
    }

    tab_interface_ =
        std::make_unique<page_actions::FakeTabInterface>(profile_.get());
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

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
#if BUILDFLAG(IS_CHROMEOS)
  // Needed because TestWebContents ends up creating BTM classes which depend
  // on this on ChromeOS.
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
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
        ShowAnchoredMessage(
            kActionIndigo,
            page_actions::AnchoredMessageConfig{
                .priority =
                    page_actions::PageActionPriorityCategory::kContextualCue}));
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

  controller_->InvokeAction();
  EXPECT_TRUE(fetcher_called.Wait());
}

TEST_F(IndigoPageActionControllerTest, OnboardingSuccessTriggersContinuation) {
  CreateController();

  // Explicitly set the initial state for onboarding preference.
  profile_->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, false);

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

  // Now simulate the dialog closing with success.
  std::move(captured_callback).Run(result);

  // Closing with success set the pref and trigger a re-fetch for continuation.
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kIndigoHasOnboarded));
  EXPECT_TRUE(fetcher_called.Wait());
}

TEST_F(IndigoPageActionControllerTest, OnboardingCancelledDoesNotTrigger) {
  CreateController();

  // Explicitly set the initial state for onboarding preference.
  profile_->GetPrefs()->SetBoolean(prefs::kIndigoHasOnboarded, false);

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
      .CheckOnboardingResult(result);

  EXPECT_FALSE(fetcher_called.IsReady());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kIndigoHasOnboarded));
}

}  // namespace
}  // namespace indigo
