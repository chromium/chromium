// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_iph_controller.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/scoped_glic_provider_override.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test_common.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

namespace glic {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);


}  // namespace

using TestBase = test::InteractiveGlicFeaturePromoTest;

class GlicIphControllerTestBase : public TestBase {
 public:
  const InteractiveBrowserTestApi::DeepQuery kMockFreClientNoThanksButton = {
      "#noThanks"};
  const InteractiveBrowserTestApi::DeepQuery kMockFreClientContinueButton = {
      "#continue"};

  explicit GlicIphControllerTestBase(
      std::vector<base::test::FeatureRef> features)
      : TestBase(base::FieldTrialParams(),
                 GlicTestEnvironmentConfig(),
                 UseDefaultTrackerAllowingPromos(features),
                 ClockMode::kUseDefaultClock) {}
  ~GlicIphControllerTestBase() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("glic.test", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory(
        GetChromeTestDataDir());
    TestBase::SetUpOnMainThread();
    SetFRECompletion(browser()->GetProfile(), prefs::FreStatus::kNotStarted);
  }

  GURL Title1() const {
    GURL test_url = embedded_test_server()->GetURL("/title1.html");
    GURL::Replacements replacements;
    replacements.SetHostStr("glic.test");
    return test_url.ReplaceComponents(replacements);
  }


  auto ShowPromoForTest() {
    return Do([&]() {
      glic::GlicIphController::From(browser())->MaybeShowPromoForTest();
    });
  }

  auto WaitForGlicIph(base::test::FeatureRef feature) {
    MultiStep steps = Steps(InstrumentTab(kFirstTab),
                            NavigateWebContents(kFirstTab, Title1()),
                            WaitForWebContentsReady(kFirstTab),
                            ShowPromoForTest(), WaitForPromo(*feature));
    return steps;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicIphControllerTestClassic : public GlicIphControllerTestBase {
 public:
  GlicIphControllerTestClassic()
      : GlicIphControllerTestBase({feature_engagement::kIPHGlicPromoFeature}) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{feature_engagement::kIPHGlicTryItFeature});
  }
  ~GlicIphControllerTestClassic() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicIphControllerTestClassic,
                       ShowsConsumerPromoStrings) {
  RunTestSequence(
      WaitForGlicIph({feature_engagement::kIPHGlicPromoFeature}),
      CheckViewProperty(user_education::HelpBubbleView::kTitleTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_GLIC_PROMO_TITLE)),
      CheckViewProperty(user_education::HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_GLIC_PROMO_BODY)));
}

// Confirms that the promo is not shown if the user's profile has a signed-in
// account that needs to be re-authenticated.
IN_PROC_BROWSER_TEST_F(GlicIphControllerTestClassic,
                       ShowPromoBlockedByAuthError) {
  RunTestSequence(
      // Prepares the browser to show the IPH.
      InstrumentTab(kFirstTab), NavigateWebContents(kFirstTab, Title1()),
      WaitForWebContentsReady(kFirstTab),
      // Disable model execution capabilities for the profile's account.
      Do([this]() { glic_test_service().SetModelExecutionCapability(false); }),
      // Tries to trigger the showing of the IPH.
      ShowPromoForTest(),
      // Checks that the showing of the IPH was not actually requested to the
      // user education system.
      CheckPromoRequested(feature_engagement::kIPHGlicPromoFeature, false));
}

class GlicIphControllerTestTryIt : public GlicIphControllerTestBase {
 public:
  GlicIphControllerTestTryIt()
      : GlicIphControllerTestBase({feature_engagement::kIPHGlicTryItFeature}) {
  }

  ~GlicIphControllerTestTryIt() override = default;
};

// TODO(b/503834154): Write a test for IPH promo leading into trust-first FRE

IN_PROC_BROWSER_TEST_F(GlicIphControllerTestTryIt, ShowPromoWithCtaEndsInGlic) {
  SetFRECompletion(browser()->GetProfile(), prefs::FreStatus::kCompleted);
  RunTestSequence(WaitForGlicIph({feature_engagement::kIPHGlicTryItFeature}),
                  PressDefaultPromoButton(),
                  WaitForAndInstrumentGlic(kHostAndContents));
}

class GlicIphControllerTestPromoDisabled : public GlicIphControllerTestBase {
 public:
  GlicIphControllerTestPromoDisabled()
      : GlicIphControllerTestBase({feature_engagement::kIPHGlicTryItFeature}) {
    scoped_feature_list_.InitAndDisableFeature(
        feature_engagement::kIPHGlicPromoFeature);
  }
  ~GlicIphControllerTestPromoDisabled() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicIphControllerTestPromoDisabled,
                       ShowPromoWithCtaEndsInGlic) {
  SetFRECompletion(browser()->GetProfile(), prefs::FreStatus::kCompleted);
  RunTestSequence(WaitForGlicIph({feature_engagement::kIPHGlicTryItFeature}),
                  PressDefaultPromoButton(),
                  WaitForAndInstrumentGlic(kHostAndContents));
}

class GlicIphControllerTestMultiInstance : public GlicIphControllerTestBase {
 public:
  GlicIphControllerTestMultiInstance()
      : GlicIphControllerTestBase({feature_engagement::kIPHGlicTryItFeature}) {
    scoped_feature_list_.InitWithFeatures(
        {mojom::features::kGlicMultiTab, features::kGlicMultitabUnderlines,
         feature_engagement::kIPHGlicPromoFeature},
        {});
  }
  ~GlicIphControllerTestMultiInstance() override = default;
};

// TODO(b/503834154): Write a test for IPH promo leading into trust-first FRE.

IN_PROC_BROWSER_TEST_F(GlicIphControllerTestMultiInstance,
                       ShowPromoWithCtaEndsInGlic) {
  SetFRECompletion(browser()->GetProfile(), prefs::FreStatus::kCompleted);
  RunTestSequence(WaitForGlicIph({feature_engagement::kIPHGlicTryItFeature}),
                  PressDefaultPromoButton(),
                  WaitForAndInstrumentGlic(kHostAndContents));
}

class GlicIphControllerWarmingInteractiveUiTest
    : public GlicIphControllerTestBase {
 public:
  GlicIphControllerWarmingInteractiveUiTest()
      : GlicIphControllerTestBase({feature_engagement::kIPHGlicTryItFeature}) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicWarmOnIph,
                              features::kGlicAnchorEntryPointForOnboardedUsers},
        /*disabled_features=*/{features::kGlicWarming});
  }
  ~GlicIphControllerWarmingInteractiveUiTest() override = default;

  void SetUp() override {
    ForceConnectionTypeForTesting(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
    GlicIphControllerTestBase::SetUp();
  }

  void TearDown() override {
    GlicIphControllerTestBase::TearDown();
    ForceConnectionTypeForTesting(std::nullopt);
  }

  bool IsWarmed() {
    auto* keyed_service = GlicKeyedService::Get(browser()->GetProfile());
    auto& coordinator = static_cast<GlicInstanceCoordinatorImpl&>(
        keyed_service->instance_coordinator());
    return coordinator.GetWebContentsWarmingPoolForTesting()
        .HasWarmedContainerForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicIphControllerWarmingInteractiveUiTest,
                       WarmsOnIphShownWhenFeatureEnabled) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(IsWarmed());

  RunTestSequence(WaitForGlicIph({feature_engagement::kIPHGlicTryItFeature}));

  EXPECT_TRUE(base::test::RunUntil([this]() { return IsWarmed(); }));

  histogram_tester.ExpectBucketCount("Glic.Prewarming.ChecksResult.Iph",
                                     GlicPrewarmingChecksResult::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      "Glic.WarmingPool.ContainerCreationReason",
      GlicWebContentsWarmingPool::ContainerCreationReason::kIph, 1);
}

class GlicIphControllerWarmingDisabledInteractiveUiTest
    : public GlicIphControllerTestBase {
 public:
  GlicIphControllerWarmingDisabledInteractiveUiTest()
      : GlicIphControllerTestBase({feature_engagement::kIPHGlicTryItFeature}) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicAnchorEntryPointForOnboardedUsers},
        /*disabled_features=*/{features::kGlicWarmOnIph,
                               features::kGlicWarming});
  }
  ~GlicIphControllerWarmingDisabledInteractiveUiTest() override = default;

  void SetUp() override {
    ForceConnectionTypeForTesting(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
    GlicIphControllerTestBase::SetUp();
  }

  void TearDown() override {
    GlicIphControllerTestBase::TearDown();
    ForceConnectionTypeForTesting(std::nullopt);
  }

  bool IsWarmed() {
    auto* keyed_service = GlicKeyedService::Get(browser()->GetProfile());
    auto& coordinator = static_cast<GlicInstanceCoordinatorImpl&>(
        keyed_service->instance_coordinator());
    return coordinator.GetWebContentsWarmingPoolForTesting()
        .HasWarmedContainerForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicIphControllerWarmingDisabledInteractiveUiTest,
                       DoesNotWarmOnIphShownWhenFeatureDisabled) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(IsWarmed());

  RunTestSequence(WaitForGlicIph({feature_engagement::kIPHGlicTryItFeature}));

  base::PlatformThread::Sleep(base::Milliseconds(200));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsWarmed());

  histogram_tester.ExpectTotalCount("Glic.Prewarming.ChecksResult.Iph", 0);
  histogram_tester.ExpectBucketCount(
      "Glic.WarmingPool.ContainerCreationReason",
      GlicWebContentsWarmingPool::ContainerCreationReason::kIph, 0);
}

class GlicIphControllerTestGeicClassic : public GlicIphControllerTestBase {
 public:
  GlicIphControllerTestGeicClassic()
      : GlicIphControllerTestBase({feature_engagement::kIPHGlicPromoFeature}) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{feature_engagement::kIPHGlicTryItFeature});
  }
  ~GlicIphControllerTestGeicClassic() override = default;

 private:
  ScopedGlicProviderOverride provider_override_{
      GlicProvider::kGeminiEnterprise};
};

IN_PROC_BROWSER_TEST_F(GlicIphControllerTestGeicClassic,
                       ShowsGeminiEnterprisePromoStrings) {
  RunTestSequence(
      WaitForGlicIph({feature_engagement::kIPHGlicPromoFeature}),
      CheckViewProperty(user_education::HelpBubbleView::kTitleTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_GLIC_PROMO_TITLE)),
      CheckViewProperty(user_education::HelpBubbleView::kBodyTextIdForTesting,
                        &views::Label::GetText,
                        l10n_util::GetStringUTF16(IDS_GEIC_PROMO_BODY)));
}

}  // namespace glic
