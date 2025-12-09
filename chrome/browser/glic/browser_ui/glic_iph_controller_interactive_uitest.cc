// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/browser_ui/glic_iph_controller.h"
#include "chrome/browser/glic/fre/glic_fre_dialog_view.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test_common.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

class FreWebUiStateObserver
    : public ui::test::StateObserver<mojom::FreWebUiState> {
 public:
  explicit FreWebUiStateObserver(GlicFreController& controller)
      : subscription_(controller.AddWebUiStateChangedCallback(
            base::BindRepeating(&FreWebUiStateObserver::OnWebUiStateChanged,
                                base::Unretained(this)))) {}

  void OnWebUiStateChanged(mojom::FreWebUiState new_state) {
    OnStateObserverStateChanged(new_state);
  }

 private:
  base::CallbackListSubscription subscription_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(FreWebUiStateObserver, kFreWebUiState);

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
    SetFRECompletion(browser()->profile(), prefs::FreStatus::kNotStarted);
  }

  GURL Title1() const {
    GURL test_url = embedded_test_server()->GetURL("/title1.html");
    GURL::Replacements replacements;
    replacements.SetHostStr("glic.test");
    return test_url.ReplaceComponents(replacements);
  }

  GlicFreController& GetFreController() {
    return glic_service()->fre_controller();
  }

  auto WaitForAndInstrumentGlicFre() {
    MultiStep steps =
        Steps(UninstrumentWebContents(test::kGlicFreContentsElementId, false),
              UninstrumentWebContents(test::kGlicFreHostElementId, false),
              ObserveState(test::internal::kGlicFreShowingDialogState,
                           std::ref(GetFreController())),
              InAnyContext(Steps(
                  InstrumentNonTabWebView(
                      test::kGlicFreHostElementId,
                      GlicFreDialogView::kWebViewElementIdForTesting),
                  InstrumentInnerWebContents(test::kGlicFreContentsElementId,
                                             test::kGlicFreHostElementId, 0),
                  WaitForWebContentsReady(test::kGlicFreContentsElementId))),
              WaitForState(test::internal::kGlicFreShowingDialogState, true),
              StopObservingState(test::internal::kGlicFreShowingDialogState));

    AddDescriptionPrefix(steps, "WaitForAndInstrumentGlicFre");
    return steps;
  }

  auto ShowPromoForTest() {
    return Do([&]() {
      browser()->GetFeatures().glic_iph_controller()->MaybeShowPromoForTest();
    });
  }

  auto WaitForGlicIph(base::test::FeatureRef feature) {
    MultiStep steps = Steps(InstrumentTab(kFirstTab),
                            NavigateWebContents(kFirstTab, Title1()),
                            WaitForWebContentsReady(kFirstTab),
                            ShowPromoForTest(), WaitForPromo(*feature));
    return steps;
  }

  auto ExpectWarmedFre(bool warm) {
    return Do([this, warm]() {
      EXPECT_EQ(warm, GlicKeyedServiceFactory::GetGlicKeyedService(
                          browser()->profile())
                          ->fre_controller()
                          .IsWarmed());
    });
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class GlicIphControllerTestClassic : public GlicIphControllerTestBase {
 public:
  GlicIphControllerTestClassic()
      : GlicIphControllerTestBase({feature_engagement::kIPHGlicPromoFeature}) {
    // enables FRE warming to test that successful IPH will warm the FRE.
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicFreWarming},
        {feature_engagement::kIPHGlicTryItFeature});
  }
  ~GlicIphControllerTestClassic() override = default;
};

// Test that settings changes are reflected in the show state of the controller
// delegate.
IN_PROC_BROWSER_TEST_F(GlicIphControllerTestClassic, ShowPromo) {
  RunTestSequence(ExpectWarmedFre(false),
                  ObserveState(kFreWebUiState, std::ref(GetFreController())),
                  WaitForGlicIph({feature_engagement::kIPHGlicPromoFeature}),
                  WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
                  ExpectWarmedFre(true), PressDefaultPromoButton(),
                  StopObservingState(kFreWebUiState));
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
      CheckPromoRequested(feature_engagement::kIPHGlicPromoFeature, false),
      // Checks that the FRE was not pre-warmed.
      ExpectWarmedFre(false));
}

class GlicIphControllerTestTryIt : public GlicIphControllerTestBase {
 public:
  GlicIphControllerTestTryIt()
      // Enable both features to make sure that TryIt takes priority.
      : GlicIphControllerTestBase({feature_engagement::kIPHGlicPromoFeature,
                                   feature_engagement::kIPHGlicTryItFeature}) {}
  ~GlicIphControllerTestTryIt() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicIphControllerTestTryIt,
                       ShowPromoWithCtaEndsInGlicFre) {
  RunTestSequence(ObserveState(kFreWebUiState, std::ref(GetFreController())),
                  WaitForGlicIph({feature_engagement::kIPHGlicTryItFeature}),
                  PressDefaultPromoButton(),
                  WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
                  StopObservingState(kFreWebUiState));
}

IN_PROC_BROWSER_TEST_F(GlicIphControllerTestTryIt, ShowPromoWithCtaEndsInGlic) {
  SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);
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
  SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);
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
         features::kGlicMultiInstance,
         feature_engagement::kIPHGlicPromoFeature},
        {});
  }
  ~GlicIphControllerTestMultiInstance() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicIphControllerTestMultiInstance,
                       ShowPromoWithCtaEndsInGlicFre) {
  ASSERT_TRUE(GlicEnabling::IsMultiInstanceEnabledByFlags());
  RunTestSequence(ObserveState(kFreWebUiState, std::ref(GetFreController())),
                  WaitForGlicIph({feature_engagement::kIPHGlicTryItFeature}),
                  PressDefaultPromoButton(),
                  WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
                  StopObservingState(kFreWebUiState));
}

IN_PROC_BROWSER_TEST_F(GlicIphControllerTestMultiInstance,
                       ShowPromoWithCtaEndsInGlic) {
  ASSERT_TRUE(GlicEnabling::IsMultiInstanceEnabledByFlags());
  SetFRECompletion(browser()->profile(), prefs::FreStatus::kCompleted);
  RunTestSequence(WaitForGlicIph({feature_engagement::kIPHGlicTryItFeature}),
                  PressDefaultPromoButton(),
                  WaitForAndInstrumentGlic(kHostAndContents));
}

}  // namespace glic
