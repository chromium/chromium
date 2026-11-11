// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"

#include <memory>
#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_cui_tracker.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/service/glic_state_tracker.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/profile_metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/skills/public/skills_metrics.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/geometry/size.h"

namespace glic {
namespace {

class MockGlicSharingManager : public GlicSharingManagerInternal {
 public:
  MOCK_METHOD(base::CallbackListSubscription,
              AddFocusedTabChangedCallback,
              (FocusedTabChangedCallback callback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddFocusedTabDataChangedCallback,
              (FocusedTabDataChangedCallback callback),
              (override));
  FocusedTabData GetFocusedTabData() override {
    return FocusedTabData("no focus", nullptr);
  }
  MOCK_METHOD(base::CallbackListSubscription,
              AddFocusedBrowserChangedCallback,
              (FocusedBrowserChangedCallback callback),
              (override));
  MOCK_METHOD(BrowserWindowInterface*,
              GetFocusedBrowser,
              (),
              (const, override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddTabPinningStatusChangedCallback,
              (TabPinningStatusChangedCallback callback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddTabPinningStatusEventCallback,
              (TabPinningStatusEventCallback callback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddPinnedTabsChangedCallback,
              (PinnedTabsChangedCallback callback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddPinnedTabDataChangedCallback,
              (PinnedTabDataChangedCallback callback),
              (override));
  MOCK_METHOD(bool,
              PinTabs,
              (base::span<const tabs::TabHandle> tab_handles,
               GlicPinTrigger trigger),
              (override));
  MOCK_METHOD(void,
              SetPinTrigger,
              (tabs::TabHandle tab_handle, GlicPinTrigger trigger),
              (override));
  MOCK_METHOD(bool,
              UnpinTabs,
              (base::span<const tabs::TabHandle> tab_handles,
               GlicUnpinTrigger trigger),
              (override));
  MOCK_METHOD(void, UnpinAllTabs, (GlicUnpinTrigger trigger), (override));
  MOCK_METHOD(int32_t, GetMaxPinnedTabs, (), (const, override));
  MOCK_METHOD(int32_t, GetNumPinnedTabs, (), (const, override));
  MOCK_METHOD(int32_t,
              SetMaxPinnedTabs,
              (uint32_t max_pinned_tabs),
              (override));
  MOCK_METHOD(std::vector<raw_ptr<tabs::TabInterface>>,
              GetPinnedTabs,
              (),
              (const, override));
  MOCK_METHOD(bool,
              IsTabPinned,
              (tabs::TabHandle tab_handle),
              (const, override));
  MOCK_METHOD(bool, IsTabShared, (tabs::TabInterface * tab), (const, override));
  MOCK_METHOD(bool,
              IsTabFocused,
              (tabs::TabHandle tab_handle),
              (const, override));
  MOCK_METHOD(std::optional<GlicPinnedTabUsage>,
              GetPinnedTabUsage,
              (tabs::TabHandle tab_handle),
              (override));
  MOCK_METHOD(std::optional<GlicGetContextError>,
              CheckPreliminaryContextSharingEligibility,
              (tabs::TabHandle tab_handle),
              (const, override));
  MOCK_METHOD(void,
              GetContextFromTab,
              (tabs::TabHandle tab_handle,
               const mojom::TabContextOptions& options,
               base::OnceCallback<void(GlicGetContextResult)> callback),
              (override));
  MOCK_METHOD(void,
              GetContextForActorFromTab,
              (tabs::TabHandle tab_handle,
               const mojom::TabContextOptions& options,
               base::OnceCallback<void(GlicGetContextResult)> callback),
              (override));
  MOCK_METHOD(void,
              GetImageBytes,
              (tabs::TabHandle tab_handle,
               const std::string& document_id,
               int32_t dom_node_id,
               base::OnceCallback<void(GlicGetImageBytesResult)> callback),
              (override));
  MOCK_METHOD(void, OnConversationTurnSubmitted, (), (override));
  base::WeakPtr<GlicSharingManagerInternal> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockGlicSharingManager> weak_ptr_factory_{this};
};

}  // namespace

class GlicInstanceMetricsTest : public testing::Test {
 public:
  GlicInstanceMetricsTest() {
    EXPECT_CALL(mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));
  }

 protected:
  ShowOptions MakeNativeFreShowOptions() {
    ShowOptions show_options = ShowOptions::ForSidePanel(mock_tab_);
    show_options.fre_override = mojom::FreOverride::kNativeFre;
    return show_options;
  }

  ShowOptions ShowOptionsFor(mojom::InvocationSource source,
                             ShowOptions options) {
    options.invocation_source = source;
    return options;
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ukm::TestAutoSetUkmRecorder ukm_tester_;
  TestingProfile profile_;
  metrics::ProfileMetricsService profile_metrics_service_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  GlicInstanceMetrics metrics_{&profile_metrics_service_, &profile_};
  tabs::MockTabInterface mock_tab_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kGlicCaptureRegion};
};

class GlicInstanceMetricsTestWithPolyline : public GlicInstanceMetricsTest {
 public:
  GlicInstanceMetricsTestWithPolyline() {
    feature_list_.InitAndEnableFeature(features::kGlicRegionSelectionLine);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(GlicInstanceMetricsTest, OptinImpression) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicOptInImpressionMetrics);

  metrics_.OnOptinImpression();
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Glic.Onboarding.OptInImpression"), 0);

  metrics_.OnVisibilityChanged(true);
  metrics_.OnClientReady();

  EXPECT_EQ(
      user_action_tester_.GetActionCount("Glic.Onboarding.OptInImpression"), 1);
}

TEST_F(GlicInstanceMetricsTest, OptinImpression_InvokesCallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicOptInImpressionMetrics);

  int callback_call_count = 0;
  metrics_.SetOptInShownCallback(base::BindRepeating(
      [](int* count, ukm::SourceId source_id) { (*count)++; },
      &callback_call_count));

  metrics_.OnOptinImpression();
  metrics_.OnVisibilityChanged(true);
  metrics_.OnClientReady();

  EXPECT_EQ(callback_call_count, 1);
}

TEST_F(GlicInstanceMetricsTest, OptinImpression_KillSwitchDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      features::kGlicOptInImpressionMetrics);

  metrics_.OnOptinImpression();
  metrics_.OnVisibilityChanged(true);
  metrics_.OnClientReady();

  EXPECT_EQ(
      user_action_tester_.GetActionCount("Glic.Onboarding.OptInImpression"), 0);
}

TEST_F(GlicInstanceMetricsTest, OnActivationChanged_LogsTimeSinceLastActive) {
  // First activation.
  metrics_.OnActivationChanged(true);
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeSinceLastActive", 0);

  // Deactivate.
  metrics_.OnActivationChanged(false);

  // Advance time.
  task_environment_.FastForwardBy(base::Minutes(5));

  // Second activation.
  metrics_.OnActivationChanged(true);
  histogram_tester_.ExpectUniqueTimeSample("Glic.Instance.TimeSinceLastActive",
                                           base::Minutes(5), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeSinceLastActive.24H", base::Minutes(5), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeSinceLastActive.7D", base::Minutes(5), 1);

  // Redundant activation should not log again.
  task_environment_.FastForwardBy(base::Minutes(1));
  metrics_.OnActivationChanged(true);
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeSinceLastActive", 1);
}

TEST_F(GlicInstanceMetricsTest, OnResponseStarted_WithoutInput_LogsError) {
  metrics_.OnResponseStarted();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStartWithoutInput, 1);
}

TEST_F(GlicInstanceMetricsTest,
       OnResponseStarted_WithActuation_DoesNotLogError) {
  metrics_.OnActionSubmitted(/*is_retry=*/false);
  metrics_.OnResponseStarted();
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStartWithoutInput, 0);
}

TEST_F(GlicInstanceMetricsTest, OnResponseStarted_WhileHidden_LogsError) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  metrics_.OnVisibilityChanged(false);
  metrics_.OnResponseStarted();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStartWhileHidingOrHidden, 1);
}

TEST_F(GlicInstanceMetricsTest, OnResponseStopped_WithoutInput_LogsError) {
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error.ByUser",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
}

TEST_F(GlicInstanceMetricsTest,
       OnResponseStopped_WithoutInputOtherCause_LogsError) {
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kOther);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error.Other",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
}

TEST_F(GlicInstanceMetricsTest,
       OnResponseStopped_WithoutInputUnknownCause_LogsError) {
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error.UnknownCause",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
}

TEST_F(GlicInstanceMetricsTest,
       OnResponseStopped_WithActuation_DoesNotLogError) {
  metrics_.OnActionSubmitted(/*is_retry=*/false);
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 0);
}

TEST_F(GlicInstanceMetricsTest, OnFloatyClosed_WithoutOpening_LogsError) {
  metrics_.OnFloatyClosed();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kFloatyClosedWithoutOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, OnSidePanelClosed_WithoutOpening_LogsError) {
  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kSidePanelClosedWithoutOpen, 1);
}

TEST_F(GlicInstanceMetricsTest,
       OnUserInputSubmitted_WhileResponseInProgress_LogsError) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  metrics_.OnResponseStarted();
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kInputSubmittedWhileResponseInProgress, 1);
}

TEST_F(GlicInstanceMetricsTest, OnUserInputSubmitted_WhileHidden_LogsError) {
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kInputSubmittedWhileHidden, 1);
}

TEST_F(GlicInstanceMetricsTest, SubmitQueryCuiOutcomeRecorded) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  metrics_.OnResponseStarted();
  histogram_tester_.ExpectUniqueSample("Glic.CUI.SubmitQuery.Outcome",
                                       GlicCuiOutcome::kSuccess, 1);
}

TEST_F(GlicInstanceMetricsTest, SubmitQueryCuiOutcomeRecorded_Failed) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  metrics_.OnWebUiStateChanged(mojom::WebUiState::kError);
  histogram_tester_.ExpectUniqueSample("Glic.CUI.SubmitQuery.Outcome",
                                       GlicCuiOutcome::kFailed, 1);
}

TEST_F(GlicInstanceMetricsTest, SubmitQueryCuiOutcomeRecorded_Abandoned) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  metrics_.OnVisibilityChanged(false);
  histogram_tester_.ExpectUniqueSample("Glic.CUI.SubmitQuery.Outcome",
                                       GlicCuiOutcome::kAbandoned, 1);
}

TEST_F(GlicInstanceMetricsTest, OnShowInFloaty_WhileAlreadyOpen_LogsError) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnShowInFloaty(show_options);
  metrics_.OnShowInFloaty(show_options);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kFloatyOpenedWhileAlreadyOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, OnShowInSidePanel_WhileAlreadyOpen_LogsError) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnShowInSidePanel(&mock_tab_);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kSidePanelOpenedWhileAlreadyOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, OnUnbindEmbedder_WithoutOpening_LogsError) {
  tabs::TabInterface* tab_ptr = &mock_tab_;
  metrics_.OnUnbindEmbedder(SidePanelEmbedderKey{tab_ptr});
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kTabUnbindWithoutOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, OnUserResizeStarted) {
  gfx::Size test_size(800, 600);
  metrics_.OnUserResizeStarted(test_size);

  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Glic.Instance.Floaty.UserResizeStarted"));
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Floaty.UserResizeStarted.Width", test_size.width(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Floaty.UserResizeStarted.Height", test_size.height(), 1);
}

TEST_F(GlicInstanceMetricsTest, OnUserResizeEnded) {
  gfx::Size test_size(1024, 768);
  metrics_.OnUserResizeEnded(test_size);

  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Glic.Instance.Floaty.UserResizeEnded"));
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Floaty.UserResizeEnded.Width", test_size.width(), 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Floaty.UserResizeEnded.Height", test_size.height(), 1);
}

TEST_F(GlicInstanceMetricsTest, ValidFloatyFlow_DoesNotLogError) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnShowInFloaty(show_options);
  metrics_.OnFloatyClosed();
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(GlicInstanceMetricsTest, ValidSidePanelFlow_DoesNotLogError) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(GlicInstanceMetricsTest, OnOpen_DoesNotOverrideInitialEntrypoint) {
  ShowOptions show_options1{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options1);
  EXPECT_EQ(metrics_.initial_invocation_source(),
            mojom::InvocationSource::kTopChromeButton);

  ShowOptions show_options2{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options2);
  EXPECT_EQ(metrics_.initial_invocation_source(),
            mojom::InvocationSource::kTopChromeButton);
}

TEST_F(GlicInstanceMetricsTest,
       MaybeSetInitialInvocationSource_SetsInitialInvocationSource) {
  metrics_.MaybeSetInitialInvocationSource(
      mojom::InvocationSource::kTopChromeButton);
  EXPECT_EQ(metrics_.initial_invocation_source(),
            mojom::InvocationSource::kTopChromeButton);
}

TEST_F(GlicInstanceMetricsTest,
       MaybeSetInitialInvocationSource_DoesNotOverrideInitialInvocationSource) {
  metrics_.MaybeSetInitialInvocationSource(
      mojom::InvocationSource::kTopChromeButton);
  EXPECT_EQ(metrics_.initial_invocation_source(),
            mojom::InvocationSource::kTopChromeButton);

  metrics_.MaybeSetInitialInvocationSource(mojom::InvocationSource::kOsButton);
  EXPECT_EQ(metrics_.initial_invocation_source(),
            mojom::InvocationSource::kTopChromeButton);
}

TEST_F(GlicInstanceMetricsTest, InitialInvocationSource_OnlyRecordedOnce) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.InitialInvocationSource",
      mojom::InvocationSource::kTopChromeButton, 1);

  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);
  // Should still be 1 sample of kTopChromeButton.
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.InitialInvocationSource",
      mojom::InvocationSource::kTopChromeButton, 1);
}

TEST_F(GlicInstanceMetricsTest,
       InitialInvocationSource_LoggedWhenStartingInactive) {
  // 1. Simulate opening the instance in an inactive side panel via a background
  // daisy-chain.
  metrics_.MaybeSetInitialInvocationSource(
      mojom::InvocationSource::kDaisyChainOnFollowLink);

  // 2. Later, the user activates the background tab, promoting the instance to
  // be fully shown.
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};
  metrics_.OnOpen(mojom::InvocationSource::kReshowInactive, show_options);

  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.InitialInvocationSource",
      mojom::InvocationSource::kDaisyChainOnFollowLink, 1);
}

TEST_F(GlicInstanceMetricsTest, FreInvoked_RecordedOnceWhenUnconsented) {
  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));

  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Fre.Invoked.InvocationSource",
      mojom::InvocationSource::kTopChromeButton, 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.Invoked"), 1);

  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Fre.Invoked.InvocationSource",
      mojom::InvocationSource::kTopChromeButton, 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.Invoked"), 1);
}

TEST_F(GlicInstanceMetricsTest, FreInvoked_NotRecordedWhenConsented) {
  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  histogram_tester_.ExpectTotalCount("Glic.Fre.Invoked.InvocationSource", 0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.Invoked"), 0);
}

TEST_F(GlicInstanceMetricsTest, SidePanelFirstOpenDuration_LoggedOnFirstClose) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));

  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  task_environment_.FastForwardBy(base::Minutes(5));

  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.TopChromeButton.SidePanelFirstOpenDuration",
      base::Minutes(5), 1);
}

TEST_F(GlicInstanceMetricsTest,
       SidePanelFirstOpenDuration_NotLoggedOnSecondClose) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));

  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);

  task_environment_.FastForwardBy(base::Minutes(5));
  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.SidePanelFirstOpenDuration", 1);

  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  task_environment_.FastForwardBy(base::Minutes(2));
  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.SidePanelFirstOpenDuration", 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.OsButton.SidePanelFirstOpenDuration", 0);
}

TEST_F(GlicInstanceMetricsTest,
       SidePanelFirstOpenDuration_ShownWithoutToggleCall) {
  metrics_.OnShowInSidePanel(&mock_tab_);
  task_environment_.FastForwardBy(base::Minutes(5));
  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.Unsupported.SidePanelFirstOpenDuration",
      base::Minutes(5), 1);
}

TEST_F(GlicInstanceMetricsTest, FloatyFirstOpenDuration_LoggedOnFirstClose) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInFloaty(show_options);
  task_environment_.FastForwardBy(base::Minutes(5));

  metrics_.OnFloatyClosed();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.TopChromeButton.FloatyFirstOpenDuration",
      base::Minutes(5), 1);
}

TEST_F(GlicInstanceMetricsTest,
       FloatyFirstOpenDuration_NotLoggedOnSecondClose) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInFloaty(show_options);

  task_environment_.FastForwardBy(base::Minutes(5));
  metrics_.OnFloatyClosed();

  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.FloatyFirstOpenDuration", 1);

  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);
  metrics_.OnShowInFloaty(show_options);
  task_environment_.FastForwardBy(base::Minutes(2));
  metrics_.OnFloatyClosed();

  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.FloatyFirstOpenDuration", 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.OsButton.FloatyFirstOpenDuration", 0);
}

TEST_F(GlicInstanceMetricsTest,
       FloatyFirstOpenDuration_ShownWithoutToggleCall) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnShowInFloaty(show_options);
  task_environment_.FastForwardBy(base::Minutes(5));
  metrics_.OnFloatyClosed();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.Unsupported.FloatyFirstOpenDuration",
      base::Minutes(5), 1);
}

TEST_F(GlicInstanceMetricsTest, FloatyFirstOpenDuration_LoggedOnUnbind) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInFloaty(show_options);
  task_environment_.FastForwardBy(base::Minutes(5));

  metrics_.OnUnbindEmbedder(FloatingEmbedderKey{});

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.TopChromeButton.FloatyFirstOpenDuration",
      base::Minutes(5), 1);

  // Verify that Floaty.OpenDuration is logged on unbind.
  histogram_tester_.ExpectUniqueTimeSample("Glic.Instance.Floaty.OpenDuration",
                                           base::Minutes(5), 1);
}

TEST_F(GlicInstanceMetricsTest, InstanceEvents_LogsEventCountsAndHadEvent) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);

  // This causes a GlicInstanceEvent::kTurnCompleted to be logged.
  metrics_.OnTurnCompleted(mojom::WebClientModel::kActor,
                           base::Milliseconds(100));

  histogram_tester_.ExpectBucketCount("Glic.Instance.EventCounts",
                                      GlicInstanceEvent::kTurnCompleted, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.EventCounts",
      GlicInstanceEvent::kTurnCompleted, 1);
  histogram_tester_.ExpectBucketCount("Glic.Instance.HadEvent",
                                      GlicInstanceEvent::kTurnCompleted, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.HadEvent",
      GlicInstanceEvent::kTurnCompleted, 1);

  // Second event: only EventCounts increment, HadEvent does not.
  metrics_.OnTurnCompleted(mojom::WebClientModel::kActor,
                           base::Milliseconds(100));

  histogram_tester_.ExpectBucketCount("Glic.Instance.EventCounts",
                                      GlicInstanceEvent::kTurnCompleted, 2);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.EventCounts",
      GlicInstanceEvent::kTurnCompleted, 2);
  histogram_tester_.ExpectBucketCount("Glic.Instance.HadEvent",
                                      GlicInstanceEvent::kTurnCompleted, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.HadEvent",
      GlicInstanceEvent::kTurnCompleted, 1);
}

TEST_F(GlicInstanceMetricsTest, InstanceEvents_Open) {
  ShowOptions show_options{FloatingShowOptions{}};

  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);

  histogram_tester_.ExpectBucketCount("Glic.Instance.EventCounts",
                                      GlicInstanceEvent::kOpen, 1);
  histogram_tester_.ExpectBucketCount("Glic.Instance.HadEvent",
                                      GlicInstanceEvent::kOpen, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.EventCounts",
      GlicInstanceEvent::kOpen, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.HadEvent",
      GlicInstanceEvent::kOpen, 1);

  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.Open"), 1);

  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.InitialInvocationSource",
      mojom::InvocationSource::kTopChromeButton, 1);

  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Floaty.OpenSource",
      mojom::InvocationSource::kTopChromeButton, 1);
}

TEST_F(GlicInstanceMetricsTest,
       InstanceEvents_SidePanelShowOrder_LogsTabBoundAndOpen) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};

  metrics_.MaybeSetInitialInvocationSource(
      mojom::InvocationSource::kPdfSummarizeButton);
  metrics_.OnBind();
  metrics_.OnOpen(mojom::InvocationSource::kPdfSummarizeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);

  for (GlicInstanceEvent event :
       {GlicInstanceEvent::kTabBound, GlicInstanceEvent::kOpen,
        GlicInstanceEvent::kSidePanelShown, GlicInstanceEvent::kShown}) {
    histogram_tester_.ExpectBucketCount(
        "Glic.InvocationSource.PdfSummarizeButton.EventCounts", event, 1);
    histogram_tester_.ExpectBucketCount(
        "Glic.InvocationSource.PdfSummarizeButton.HadEvent", event, 1);
  }
}

TEST_F(GlicInstanceMetricsTest,
       InstanceEvents_BackfillsPreOpenEventsWhenInitialSourceSet) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};

  metrics_.OnInstanceCreatedWithoutWarming();
  metrics_.OnRegisterConversation("conv-id");
  metrics_.OnBind();
  metrics_.OnBind();

  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.PdfSummarizeButton.EventCounts", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.PdfSummarizeButton.HadEvent", 0);

  metrics_.OnOpen(mojom::InvocationSource::kPdfSummarizeButton, show_options);

  // Check that relevant events were properly backfilled after invocation source
  // was set as kPdfSummarizeButton.
  for (GlicInstanceEvent event :
       {GlicInstanceEvent::kInstanceCreated,
        GlicInstanceEvent::kInstanceCreatedWithoutWarming,
        GlicInstanceEvent::kRegisterConversation, GlicInstanceEvent::kTabBound,
        GlicInstanceEvent::kOpen}) {
    histogram_tester_.ExpectBucketCount(
        "Glic.InvocationSource.PdfSummarizeButton.HadEvent", event, 1);
  }
  for (GlicInstanceEvent event :
       {GlicInstanceEvent::kInstanceCreated,
        GlicInstanceEvent::kInstanceCreatedWithoutWarming,
        GlicInstanceEvent::kRegisterConversation, GlicInstanceEvent::kOpen}) {
    histogram_tester_.ExpectBucketCount(
        "Glic.InvocationSource.PdfSummarizeButton.EventCounts", event, 1);
  }
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.PdfSummarizeButton.EventCounts",
      GlicInstanceEvent::kTabBound, 2);

  metrics_.OnBind();
  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);

  // Verify that initial invocation source is sticky.
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.PdfSummarizeButton.HadEvent",
      GlicInstanceEvent::kTabBound, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.PdfSummarizeButton.EventCounts",
      GlicInstanceEvent::kTabBound, 3);
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.OsButton.EventCounts", 0);
  histogram_tester_.ExpectTotalCount("Glic.InvocationSource.OsButton.HadEvent",
                                     0);
}

TEST_F(GlicInstanceMetricsTest,
       InstanceEvents_NoCrashWithoutInitialEntrypoint) {
  // We ensure we don't crash before the initial entrypoint gets assigned.
  // We bypass OnToggle so initial_entrypoint is null.
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.EventCounts", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.HadEvent", 0);

  metrics_.OnReaction(mojom::MetricUserInputReactionType::kModel);
  // We don't get entrypoint-specific logs, but we do get generic ones.
  histogram_tester_.ExpectBucketCount("Glic.Instance.EventCounts",
                                      GlicInstanceEvent::kReaction, 1);
  histogram_tester_.ExpectBucketCount("Glic.Instance.HadEvent",
                                      GlicInstanceEvent::kReaction, 1);
}

TEST_F(GlicInstanceMetricsTest, LogEvent_LogsUnderInitialEntrypointIfReopened) {
  ShowOptions show_options1{FloatingShowOptions{}};
  // First toggle open with TopChromeButton.
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options1);

  // A turn completed event should increment TopChromeButton.
  metrics_.OnTurnCompleted(mojom::WebClientModel::kActor,
                           base::Milliseconds(100));

  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.EventCounts",
      GlicInstanceEvent::kTurnCompleted, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.HadEvent",
      GlicInstanceEvent::kTurnCompleted, 1);

  // Simulate toggling with a different source later on.
  ShowOptions show_options2{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options2);

  // Another turn completed event.
  metrics_.OnTurnCompleted(mojom::WebClientModel::kActor,
                           base::Milliseconds(100));

  // Should continue to log under the original TopChromeButton entrypoint.
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.EventCounts",
      GlicInstanceEvent::kTurnCompleted, 2);
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.TopChromeButton.HadEvent",
      GlicInstanceEvent::kTurnCompleted, 1);

  // Should NOT log under OsButton.
  histogram_tester_.ExpectBucketCount(
      "Glic.InvocationSource.OsButton.EventCounts",
      GlicInstanceEvent::kTurnCompleted, 0);
  histogram_tester_.ExpectBucketCount("Glic.InvocationSource.OsButton.HadEvent",
                                      GlicInstanceEvent::kTurnCompleted, 0);
}

TEST_F(GlicInstanceMetricsTest, WebUiLoadTime_Visible) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);

  metrics_.OnWebUiStateChanged(mojom::WebUiState::kBeginLoad);
  task_environment_.FastForwardBy(base::Milliseconds(300));
  metrics_.OnWebUiStateChanged(mojom::WebUiState::kReady);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.WebUiLoadTime.Visible", base::Milliseconds(300), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.TopChromeButton.WebUiLoadTime.Visible",
      base::Milliseconds(300), 1);
}

TEST_F(GlicInstanceMetricsTest, WebUiLoadTime_Nonvisible) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  // Do not call OnVisibilityChanged(true) so it defaults to nonvisible.

  metrics_.OnWebUiStateChanged(mojom::WebUiState::kBeginLoad);
  task_environment_.FastForwardBy(base::Milliseconds(150));
  metrics_.OnWebUiStateChanged(mojom::WebUiState::kReady);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.WebUiLoadTime.Nonvisible", base::Milliseconds(150), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.TopChromeButton.WebUiLoadTime.Nonvisible",
      base::Milliseconds(150), 1);
}

TEST_F(GlicInstanceMetricsTest, WebUiLoadTime_Onboarding) {
  TestingProfile profile;

  // Set GlicCompletedFre pref to kNotStarted (unconsented)
  profile.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));

  // Create a new GlicInstanceMetrics with the profile
  GlicInstanceMetrics onboarding_metrics(&profile_metrics_service_, &profile);

  ShowOptions show_options{FloatingShowOptions{}};
  onboarding_metrics.OnOpen(mojom::InvocationSource::kTopChromeButton,
                            show_options);
  onboarding_metrics.OnVisibilityChanged(true);

  onboarding_metrics.OnWebUiStateChanged(mojom::WebUiState::kBeginLoad);
  task_environment_.FastForwardBy(base::Milliseconds(300));
  onboarding_metrics.OnWebUiStateChanged(mojom::WebUiState::kReady);

  // Verify Glic.Onboarding.WebUiLoadTime.Visible is logged
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Onboarding.WebUiLoadTime.Visible", base::Milliseconds(300), 1);

  // Now test the consented case
  profile.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  GlicInstanceMetrics consented_metrics(&profile_metrics_service_, &profile);

  consented_metrics.OnOpen(mojom::InvocationSource::kTopChromeButton,
                           show_options);
  consented_metrics.OnVisibilityChanged(true);

  consented_metrics.OnWebUiStateChanged(mojom::WebUiState::kBeginLoad);
  task_environment_.FastForwardBy(base::Milliseconds(200));

  base::HistogramTester new_histogram_tester;
  consented_metrics.OnWebUiStateChanged(mojom::WebUiState::kReady);

  // Verify Glic.Onboarding.WebUiLoadTime.Visible is NOT logged
  new_histogram_tester.ExpectTotalCount("Glic.Onboarding.WebUiLoadTime.Visible",
                                        0);
}

TEST_F(GlicInstanceMetricsTest, ValidResponseFlow_DoesNotLogError) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);

  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicResponseInputSubmit"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicResponseStart"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicResponse"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicResponseStop"));
  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicResponseStopByUser"));
}

TEST_F(GlicInstanceMetricsTest, ActuationIsRetry) {
  metrics_.OnActionSubmitted(true);
  histogram_tester_.ExpectUniqueSample("Glic.Turn.Actuation.IsRetry", true, 1);

  metrics_.OnActionSubmitted(false);
  histogram_tester_.ExpectBucketCount("Glic.Turn.Actuation.IsRetry", false, 1);
  histogram_tester_.ExpectTotalCount("Glic.Turn.Actuation.IsRetry", 2);
}

TEST_F(GlicInstanceMetricsTest, ActuationResponseStopTime) {
  metrics_.OnVisibilityChanged(true);

  // Submit an action result which sets up the actuation timing.
  metrics_.OnActionSubmitted(false);

  // Advance time so we have a non-zero measurable latency.
  task_environment_.FastForwardBy(base::Milliseconds(123));

  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);

  histogram_tester_.ExpectTotalCount("Glic.Turn.Actuation.ResponseStartTime",
                                     1);
  histogram_tester_.ExpectTotalCount("Glic.Turn.Actuation.ResponseStopTime", 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.Actuation.ResponseStopTime.ByUser", 1);
}

TEST_F(GlicInstanceMetricsTest, InputModesUsed_IgnoresUnknown) {
  {
    GlicInstanceMetrics metrics(&profile_metrics_service_, &profile_);
    metrics.OnVisibilityChanged(true);
    metrics.OnUserInputSubmitted(mojom::WebClientMode::kUnknown,
                                 mojom::PromptType::kUnspecified);
    metrics.OnUserInputSubmitted(mojom::WebClientMode::kAudio,
                                 mojom::PromptType::kUnspecified);
  }

  histogram_tester_.ExpectTotalCount("Glic.Instance.InputModesUsed", 1);
  histogram_tester_.ExpectBucketCount("Glic.Instance.InputModesUsed",
                                      InputModesUsed::kOnlyAudio, 1);

  {
    GlicInstanceMetrics metrics(&profile_metrics_service_, &profile_);
    metrics.OnVisibilityChanged(true);
    metrics.OnUserInputSubmitted(mojom::WebClientMode::kUnknown,
                                 mojom::PromptType::kUnspecified);
  }

  histogram_tester_.ExpectTotalCount("Glic.Instance.InputModesUsed", 2);
  histogram_tester_.ExpectBucketCount("Glic.Instance.InputModesUsed",
                                      InputModesUsed::kNone, 1);
}

TEST_F(GlicInstanceMetricsTest, OnTurnCompleted_LogsHistograms) {
  metrics_.OnTurnCompleted(mojom::WebClientModel::kDefault,
                           base::Milliseconds(100));
  histogram_tester_.ExpectUniqueTimeSample("Glic.Turn.Duration.Default",
                                           base::Milliseconds(100), 1);

  metrics_.OnTurnCompleted(mojom::WebClientModel::kActor,
                           base::Milliseconds(200));
  histogram_tester_.ExpectUniqueTimeSample("Glic.Turn.Duration.Actor",
                                           base::Milliseconds(200), 1);
}

TEST_F(GlicInstanceMetricsTest, ScrollToMetrics) {
  base::test::ScopedFeatureList features(features::kGlicScrollTo);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  metrics_.OnResponseStarted();
  metrics_.OnGlicScrollAttempt();
  task_environment_.FastForwardBy(base::Milliseconds(400));
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);
  metrics_.OnGlicScrollComplete(true);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.ScrollTo.UserPromptToScrollTime.Text", base::Milliseconds(400), 1);
}

TEST_F(GlicInstanceMetricsTest, Floaty_OpenCloseClose_LogsError) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnShowInFloaty(show_options);
  metrics_.OnFloatyClosed();
  metrics_.OnFloatyClosed();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kFloatyClosedWithoutOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, SidePanel_OpenCloseClose_LogsError) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kSidePanelClosedWithoutOpen, 1);
}

TEST_F(GlicInstanceMetricsTest, Response_InputStopStop_LogsError) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kResponseStopWithoutInput, 1);
}

TEST_F(GlicInstanceMetricsTest, RecordTabPinningStatusEventLogs) {
  base::TimeTicks now = base::TimeTicks::Now();
  GlicPinEvent pin_event(GlicPinTrigger::kContextMenu, now);
  metrics_.RecordTabPinningStatusEvent(&mock_tab_, pin_event);
  histogram_tester_.ExpectUniqueSample("Glic.Instance.TabPinTrigger",
                                       GlicPinTrigger::kContextMenu, 1);
}

TEST_F(GlicInstanceMetricsTest,
       RecordTabPinningStatusEvent_LogsUnpinHistogram) {
  base::TimeTicks now = base::TimeTicks::Now();
  GlicPinnedTabUsage usage(GlicPinTrigger::kContextMenu, now);
  GlicUnpinEvent unpin_event(GlicUnpinTrigger::kContextMenu, std::move(usage),
                             now);
  metrics_.RecordTabPinningStatusEvent(&mock_tab_, unpin_event);
  histogram_tester_.ExpectUniqueSample("Glic.Instance.TabUnpinTrigger",
                                       GlicUnpinTrigger::kContextMenu, 1);
}

TEST_F(GlicInstanceMetricsTest, TurnSegmentation_OsButtonAttachedText) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  metrics_.OnResponseStarted();

  histogram_tester_.ExpectBucketCount(
      "Glic.Response.Segmentation", ResponseSegmentation::kOsButtonAttachedText,
      1);
}

TEST_F(GlicInstanceMetricsTest, TurnSegmentation_3DotsMenuDetachedAudio) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kThreeDotsMenu, show_options);
  metrics_.OnShowInFloaty(show_options);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kAudio,
                                mojom::PromptType::kUnspecified);
  metrics_.OnResponseStarted();

  histogram_tester_.ExpectBucketCount(
      "Glic.Response.Segmentation",
      ResponseSegmentation::kThreeDotsMenuDetachedAudio, 1);
}

TEST_F(GlicInstanceMetricsTest, TurnUkm) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  metrics_.OnResponseStarted();

  auto entries = ukm_tester_.GetEntriesByName("Glic.Response");
  ASSERT_EQ(entries.size(), 1u);
  auto entry = entries[0];
  ukm_tester_.ExpectEntryMetric(entry, "Attached", true);
  ukm_tester_.ExpectEntryMetric(
      entry, "InvocationSource",
      static_cast<int64_t>(mojom::InvocationSource::kOsButton));
  ukm_tester_.ExpectEntryMetric(
      entry, "WebClientMode",
      static_cast<int64_t>(mojom::WebClientMode::kText));
}

TEST_F(GlicInstanceMetricsTest, RecordSkillsWebClientEvent_RoutesOpenedMenu) {
  metrics_.RecordSkillsWebClientEvent(mojom::SkillsWebClientEvent::kOpenedMenu);

  histogram_tester_.ExpectUniqueSample("Glic.Skills.WebClient.Event",
                                       mojom::SkillsWebClientEvent::kOpenedMenu,
                                       1);
  histogram_tester_.ExpectUniqueSample("Glic.Skills.Invoke.Funnel",
                                       SkillsInvokeFunnel::kOpenedMenu, 1);
  // Ensure it didn't accidentally log an invocation action.
  histogram_tester_.ExpectTotalCount("Skills.Invoke.Action", 0);
}

TEST_F(GlicInstanceMetricsTest,
       RecordSkillsWebClientEvent_RoutesFirstPartySkill) {
  metrics_.RecordSkillsWebClientEvent(
      mojom::SkillsWebClientEvent::kUsedFirstPartySkill);

  histogram_tester_.ExpectUniqueSample(
      "Glic.Skills.WebClient.Event",
      mojom::SkillsWebClientEvent::kUsedFirstPartySkill, 1);
  histogram_tester_.ExpectUniqueSample(
      "Skills.Invoke.Action", skills::SkillsInvokeAction::kFirstParty, 1);
  histogram_tester_.ExpectUniqueSample("Glic.Skills.Invoke.Funnel",
                                       SkillsInvokeFunnel::kInvokedSkill, 1);
}
TEST_F(GlicInstanceMetricsTest,
       RecordSkillsWebClientEvent_RoutesSkillBuilderPromo) {
  metrics_.RecordSkillsWebClientEvent(
      mojom::SkillsWebClientEvent::kSkillBuilderClickedPromoChip);

  histogram_tester_.ExpectUniqueSample("Glic.Skills.SkillBuilder.Event",
                                       SkillBuilderEvent::kClickedPromoChip, 1);
}

TEST_F(GlicInstanceMetricsTest, RecordSkillsWebClientEvent_IsNoOpWhenUnknown) {
  metrics_.RecordSkillsWebClientEvent(mojom::SkillsWebClientEvent::kUnknown);

  // No metrics should be emitted.
  EXPECT_TRUE(histogram_tester_.GetTotalCountsForPrefix("Skills.").empty());
}

TEST_F(GlicInstanceMetricsTest, OnInstanceDestroyed_LogsPerProfileTurnCount) {
  metrics::ProfileMetricsContext context = 1;  // Profile 1 -> .Profile1
  metrics::ProfileMetricsService profile_metrics_service{context};
  {
    GlicInstanceMetrics metrics_with_profile(&profile_metrics_service,
                                             &profile_);
    metrics_with_profile.OnTurnCompleted(mojom::WebClientModel::kDefault,
                                         base::Milliseconds(100));
    metrics_with_profile.OnTurnCompleted(mojom::WebClientModel::kDefault,
                                         base::Milliseconds(200));
  }  // Destructor calls OnInstanceDestroyed

  histogram_tester_.ExpectUniqueSample("Glic.Instance.TurnCount", 2, 1);
  histogram_tester_.ExpectUniqueSample("Glic.Instance.TurnCount.Profile1", 2,
                                       1);
}

TEST_F(GlicInstanceMetricsTest, ZoomChangeCount) {
  {
    GlicInstanceMetrics metrics(&profile_metrics_service_, &profile_);
    metrics.OnZoomLevelChange();
    metrics.OnZoomLevelChange();
    metrics.OnZoomLevelChange();
    metrics.OnClose();
  }  // Destructor calls OnInstanceDestroyed

  histogram_tester_.ExpectUniqueSample("Glic.Instance.ZoomChangeCount", 3, 1);
}

TEST_F(GlicInstanceMetricsTest, SidePanelFirstOpenDuration_WithPrompts) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);

  task_environment_.FastForwardBy(base::Minutes(5));
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.TopChromeButton.SidePanelFirstOpenDuration."
      "WithPrompts",
      base::Minutes(5), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.SidePanelFirstOpenDuration."
      "NoPrompts",
      0);
  histogram_tester_.ExpectUniqueSample(
      "Glic.InvocationSource.TopChromeButton.SidePanelFirstOpenPromptCount", 1,
      1);
}

TEST_F(GlicInstanceMetricsTest, SidePanelFirstOpenDuration_NoPrompts) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kAutoOpenedForPdf, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);

  task_environment_.FastForwardBy(base::Minutes(3));
  metrics_.OnSidePanelClosed(&mock_tab_,
                             GlicInstanceMetrics::CloseReason::kTabSwitched);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.InvocationSource.AutoOpenedForPdf.SidePanelFirstOpenDuration."
      "NoPrompts",
      base::Minutes(3), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.AutoOpenedForPdf.SidePanelFirstOpenDuration."
      "WithPrompts",
      0);
  histogram_tester_.ExpectUniqueSample(
      "Glic.InvocationSource.AutoOpenedForPdf.SidePanelFirstOpenPromptCount", 0,
      1);
}

TEST_F(GlicInstanceMetricsTest,
       FloatyPromptsDoNotIncrementSidePanelPromptCount) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInFloaty(show_options);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  metrics_.OnFloatyClosed();

  // SidePanel metrics should not have been recorded.
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.SidePanelFirstOpenPromptCount", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.InvocationSource.TopChromeButton.SidePanelFirstOpenDuration."
      "WithPrompts",
      0);
}

TEST_F(GlicInstanceMetricsTest, PanelPresentationTime_ActiveFlow_SidePanel) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(350));
  metrics_.OnClientReady();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.PanelPresentationTime.SidePanel", base::Milliseconds(350),
      1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.Instance.PanelPresentationTime.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Consumer.Instance.PanelPresentationTime.SidePanel",
      base::Milliseconds(350), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Consumer.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToDismissWhileLoading.SidePanel", 0);
}

TEST_F(GlicInstanceMetricsTest, PanelPresentationTime_ActiveFlow_Floaty) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kOsButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(250));
  metrics_.OnClientReady();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.PanelPresentationTime.Floaty", base::Milliseconds(250), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.Inactive.Floaty", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToDismissWhileLoading.Floaty", 0);
}

TEST_F(GlicInstanceMetricsTest,
       PanelPresentationTime_InactiveFlow_HiddenDuringLoad) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(150));

  // User hides/switches away from the panel before client load completes.
  metrics_.OnVisibilityChanged(false);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToDismissWhileLoading.SidePanel",
      base::Milliseconds(150), 1);

  // Client finishes loading in background 200ms later (total 350ms).
  task_environment_.FastForwardBy(base::Milliseconds(200));
  metrics_.OnClientReady();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.PanelPresentationTime.Inactive.SidePanel",
      base::Milliseconds(350), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.SidePanel", 0);
}

TEST_F(GlicInstanceMetricsTest,
       PanelPresentationTime_ActiveFlow_ReopenedWhileLoading) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(100));

  // User hides/switches away.
  metrics_.OnVisibilityChanged(false);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToDismissWhileLoading.SidePanel",
      base::Milliseconds(100), 1);

  // User reopens panel 50ms later while still loading.
  task_environment_.FastForwardBy(base::Milliseconds(50));
  metrics_.OnVisibilityChanged(true);

  // Client finishes loading 200ms later (total 350ms) while panel is visible.
  task_environment_.FastForwardBy(base::Milliseconds(200));
  metrics_.OnClientReady();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.PanelPresentationTime.SidePanel", base::Milliseconds(350),
      1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.Instance.PanelPresentationTime.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Consumer.Instance.PanelPresentationTime.SidePanel",
      base::Milliseconds(350), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Consumer.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
}

TEST_F(GlicInstanceMetricsTest,
       PanelPresentationTime_TimeToDismissWhileLoading_ClosedBeforeReady) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(200));

  // User explicitly closes before load finishes.
  metrics_.OnClose();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToDismissWhileLoading.SidePanel",
      base::Milliseconds(200), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.Instance.PanelPresentationTime.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Consumer.Instance.PanelPresentationTime.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Consumer.Instance.PanelPresentationTime.Inactive.SidePanel", 0);

  // Client finishes loading later after close; should not record presentation
  // time.
  task_environment_.FastForwardBy(base::Seconds(5));
  metrics_.OnClientReady();

  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.Instance.PanelPresentationTime.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Enterprise.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Consumer.Instance.PanelPresentationTime.SidePanel", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Consumer.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
}

TEST_F(GlicInstanceMetricsTest,
       PanelPresentationTime_ReshowAfterBackgroundedLoadDoesNotDuplicate) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(100));

  metrics_.OnVisibilityChanged(false);
  task_environment_.FastForwardBy(base::Milliseconds(250));
  metrics_.OnClientReady();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.PanelPresentationTime.Inactive.SidePanel",
      base::Milliseconds(350), 1);

  // User returns 10 minutes later. Client was already ready, so calling
  // OnClientReady again should not record any additional/inflated sample.
  task_environment_.FastForwardBy(base::Minutes(10));
  metrics_.OnVisibilityChanged(true);
  metrics_.OnClientReady();

  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.Inactive.SidePanel", 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.SidePanel", 0);
}

TEST_F(
    GlicInstanceMetricsTest,
    PanelPresentationTime_ActiveFlow_MultipleOnOpenCallsDoNotResetStartTime) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(100));

  // A second OnOpen call occurs while still loading (e.g. tab switch during
  // load).
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  task_environment_.FastForwardBy(base::Milliseconds(150));

  metrics_.OnClientReady();

  // Presentation time should be measured from the initial OnOpen (250ms total),
  // not overwritten by the second OnOpen (which would be 150ms).
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.PanelPresentationTime.SidePanel", base::Milliseconds(250),
      1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
}

TEST_F(GlicInstanceMetricsTest,
       PanelPresentationTime_EmbedderTypeUpdatedOnSubsequentOnOpen) {
  ShowOptions side_panel_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton,
                  side_panel_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(100));

  // Switched to floaty while still loading.
  ShowOptions floaty_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, floaty_options);
  task_environment_.FastForwardBy(base::Milliseconds(150));

  metrics_.OnClientReady();

  // Should log under Floaty with the full 250ms elapsed time.
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.PanelPresentationTime.Floaty", base::Milliseconds(250), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.SidePanel", 0);
}

TEST_F(GlicInstanceMetricsTest,
       PanelPresentationTime_ClosedBeforeReadyResetsStartTimeForNextOpen) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(100));

  // Explicit close cancels the in-flight load measurement.
  metrics_.OnClose();
  task_environment_.FastForwardBy(base::Milliseconds(50));

  // Re-opened fresh.
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  metrics_.OnClientReady();

  // Should measure 200ms from the second open, not 350ms.
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.PanelPresentationTime.SidePanel", base::Milliseconds(200),
      1);
}

TEST_F(GlicInstanceMetricsTest,
       TimeToActionable_WarmedGuest_WaitsForPresentedFrame) {
  // Guest emits actionability mark while warming in the background before any
  // panel open. Must not mark the guest as presented yet.
  metrics_.OnGuestActionableMarkObserved(base::TimeTicks::Now());
  task_environment_.FastForwardBy(base::Seconds(2));

  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToActionable.SidePanel",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToActionable.Inactive.SidePanel", 0);

  // Rendered frame is presented 25ms after open.
  task_environment_.FastForwardBy(base::Milliseconds(25));
  metrics_.OnGuestFrameSubmitted();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToActionable.SidePanel", base::Milliseconds(25), 1);
}

TEST_F(GlicInstanceMetricsTest,
       TimeToActionable_ActiveFlow_WaitsForPresentedFrame) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);

  // Actionability mark and client ready arrive while visible. Legacy
  // PanelPresentationTime records on OnClientReady(), while TimeToActionable
  // waits for the submitted frame (OnGuestFrameSubmitted).
  task_environment_.FastForwardBy(base::Milliseconds(300));
  metrics_.OnGuestActionableMarkObserved(base::TimeTicks::Now());
  metrics_.OnClientReady();
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.PanelPresentationTime.SidePanel", base::Milliseconds(300),
      1);
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToActionable.SidePanel",
                                     0);

  // Visual frame callback completes 20ms later (total 320ms).
  task_environment_.FastForwardBy(base::Milliseconds(20));
  metrics_.OnGuestFrameSubmitted();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToActionable.SidePanel", base::Milliseconds(320), 1);
}

TEST_F(GlicInstanceMetricsTest,
       TimeToActionable_OnlyRecordedOncePerGuestUntilReset) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  metrics_.OnGuestActionableMarkObserved(base::TimeTicks::Now());
  metrics_.OnGuestFrameSubmitted();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToActionable.SidePanel", base::Milliseconds(200), 1);

  // Closing and reopening the same guest must not record a second sample.
  metrics_.OnVisibilityChanged(false);
  metrics_.OnClose();
  task_environment_.FastForwardBy(base::Seconds(1));
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnGuestFrameSubmitted();

  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToActionable.SidePanel",
                                     1);

  // In-place guest reload while the panel is already open marks the reloaded
  // guest as presented without emitting a histogram sample, so closing and
  // reopening later does not record a stale sample.
  metrics_.OnGuestReset();
  metrics_.OnGuestActionableMarkObserved(base::TimeTicks::Now());
  metrics_.OnGuestFrameSubmitted();
  metrics_.OnVisibilityChanged(false);
  metrics_.OnClose();
  task_environment_.FastForwardBy(base::Minutes(5));
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnGuestFrameSubmitted();
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToActionable.SidePanel",
                                     1);

  // After a guest reset while closed (e.g. hibernate), the next open records
  // once upon presentation.
  metrics_.OnVisibilityChanged(false);
  metrics_.OnClose();
  metrics_.OnGuestReset();

  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(120));
  metrics_.OnGuestActionableMarkObserved(base::TimeTicks::Now());
  metrics_.OnGuestFrameSubmitted();

  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToActionable.SidePanel",
                                     2);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Instance.TimeToActionable.SidePanel", base::Milliseconds(120), 1);
}

TEST_F(GlicInstanceMetricsTest,
       TimeToActionable_ClosedBeforePresentationLogsOnNextOpen) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(100));

  // Closing before client readiness records TimeToDismissWhileLoading.
  metrics_.OnClose();
  metrics_.OnVisibilityChanged(false);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToDismissWhileLoading.SidePanel",
      base::Milliseconds(100), 1);

  // Mark, client readiness, or a late visual callback arriving while
  // closed/hidden must not log TimeToActionable or latch `recorded`.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  metrics_.OnGuestActionableMarkObserved(base::TimeTicks::Now());
  metrics_.OnClientReady();
  metrics_.OnGuestFrameSubmitted();
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToActionable.SidePanel",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToActionable.Inactive.SidePanel", 0);

  // Reopening presents the already-actionable guest and logs once when the
  // rendered frame is presented while visible.
  task_environment_.FastForwardBy(base::Milliseconds(300));
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(10));
  metrics_.OnGuestFrameSubmitted();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToActionable.SidePanel", base::Milliseconds(10), 1);
}

TEST_F(GlicInstanceMetricsTest,
       TimeToActionable_InactiveFlow_HiddenDuringLoad) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(100));

  // User switches tabs while guest is still loading.
  metrics_.OnVisibilityChanged(false);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToDismissWhileLoading.SidePanel",
      base::Milliseconds(100), 1);

  // Guest emits actionability mark 250ms later (total 350ms) while hidden.
  task_environment_.FastForwardBy(base::Milliseconds(250));
  metrics_.OnGuestActionableMarkObserved(base::TimeTicks::Now());

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToActionable.Inactive.SidePanel",
      base::Milliseconds(350), 1);
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToActionable.SidePanel",
                                     0);

  // Returning 10 minutes later must not record an additional/inflated sample.
  task_environment_.FastForwardBy(base::Minutes(10));
  metrics_.OnVisibilityChanged(true);
  metrics_.OnGuestFrameSubmitted();
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToActionable.Inactive.SidePanel", 1);
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToActionable.SidePanel",
                                     0);
}

TEST_F(GlicInstanceMetricsTest,
       TimeToActionable_WarmedGuest_HiddenBeforeFrameRecordsWhenShown) {
  // Guest emits actionability mark while warming in the background 2s before
  // open.
  metrics_.OnGuestActionableMarkObserved(base::TimeTicks::Now());
  task_environment_.FastForwardBy(base::Seconds(2));

  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(5));

  // User hides panel before InsertVisualStateCallback completes, and the frame
  // callback fires during the hide debounce window while hidden. Must not
  // record an Inactive sample, and remembers that the frame was submitted so
  // showing the panel again records the active metric.
  metrics_.OnVisibilityChanged(false);
  metrics_.OnGuestFrameSubmitted();

  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToActionable.SidePanel",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToActionable.Inactive.SidePanel", 0);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToDismissWhileLoading.SidePanel",
      base::Milliseconds(5), 1);

  // Switching back to the tab records the active metric as soon as the panel is
  // shown again.
  task_environment_.FastForwardBy(base::Milliseconds(20));
  metrics_.OnVisibilityChanged(true);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToActionable.SidePanel", base::Milliseconds(25), 1);
}
TEST_F(GlicInstanceMetricsTest, OnUserInputSubmitted_RecordsPromptType) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  histogram_tester_.ExpectBucketCount("Glic.Turn.PromptType",
                                      mojom::PromptType::kTypedText, 1);

  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kZeroStateSuggestions);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.PromptType", mojom::PromptType::kZeroStateSuggestions, 1);

  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kAutoPrompt);
  histogram_tester_.ExpectBucketCount("Glic.Turn.PromptType",
                                      mojom::PromptType::kAutoPrompt, 1);

  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kAudio,
                                mojom::PromptType::kAudio);
  histogram_tester_.ExpectBucketCount("Glic.Turn.PromptType",
                                      mojom::PromptType::kAudio, 1);

  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);
  histogram_tester_.ExpectBucketCount("Glic.Turn.PromptType",
                                      mojom::PromptType::kUnspecified, 1);

  histogram_tester_.ExpectTotalCount("Glic.Turn.PromptType", 5);
}

TEST_F(GlicInstanceMetricsTest,
       DidRequestContextFromFocusedTab_LogsFocusedAndGeneralContextEvents) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);

  metrics_.DidRequestContextFromFocusedTab();
  metrics_.OnResponseStarted();

  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.EventCounts",
      GlicInstanceEvent::kFocusedTabContextRequested, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.HadEvent", GlicInstanceEvent::kFocusedTabContextRequested,
      1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.EventCounts", GlicInstanceEvent::kTabContextRequested, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.HadEvent", GlicInstanceEvent::kTabContextRequested, 1);
  histogram_tester_.ExpectTotalCount("Glic.Turn.ResponseStartTime.WithContext",
                                     1);
}

TEST_F(GlicInstanceMetricsTest,
       DidRequestContextFromTab_LogsOnlyGeneralTabContextEvent) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kUnspecified);

  metrics_.DidRequestContextFromTab();
  metrics_.OnResponseStarted();

  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.EventCounts",
      GlicInstanceEvent::kFocusedTabContextRequested, 0);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.HadEvent", GlicInstanceEvent::kFocusedTabContextRequested,
      0);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.EventCounts", GlicInstanceEvent::kTabContextRequested, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.HadEvent", GlicInstanceEvent::kTabContextRequested, 1);
  histogram_tester_.ExpectTotalCount("Glic.Turn.ResponseStartTime.WithContext",
                                     1);
}

TEST_F(GlicInstanceMetricsTest,
       ResetShownState_ReturnsTrueOnlyWhenPreviouslyShown) {
  EmbedderKey key = SidePanelEmbedderKey(&mock_tab_);
  EXPECT_FALSE(metrics_.ResetShownState(key));
  EXPECT_TRUE(metrics_.MarkShownAndCheckIfFirstTime(key));
  EXPECT_FALSE(metrics_.MarkShownAndCheckIfFirstTime(key));
  EXPECT_TRUE(metrics_.ResetShownState(key));
  EXPECT_FALSE(metrics_.ResetShownState(key));
}

class GlicInstanceMetricsGeicTest : public GlicInstanceMetricsTest {
 protected:
  GlicInstanceMetricsGeicTest() {
    geic_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGeic,
        {{"geic-guest-url", "https://business.gemini.google/side-panel"}});
  }

  void SetUp() override {
    GlicInstanceMetricsTest::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.push_back(
        {GlicKeyedServiceFactory::GetInstance(),
         base::BindRepeating(
             [](ProfileManager* pm, GlicProfileManager* gpm,
                content::BrowserContext* context)
                 -> std::unique_ptr<KeyedService> {
               return std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
                   context,
                   IdentityManagerFactory::GetForProfile(
                       Profile::FromBrowserContext(context)),
                   pm, gpm,
                   /*contextual_cueing_service=*/nullptr,
                   /*actor_keyed_service=*/nullptr);
             },
             testing_profile_manager_.profile_manager(),
             &glic_profile_manager_)});

    geic_profile_ = testing_profile_manager_.CreateTestingProfile(
        "geic_profile", std::move(factories));
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(geic_profile_);

    ASSERT_TRUE(
        GlicKeyedServiceFactory::GetGlicKeyedService(geic_profile_.get(),
                                                     /*create=*/true));

    geic_metrics_ = std::make_unique<GlicInstanceMetrics>(
        &profile_metrics_service_, geic_profile_);
  }

  GlicEnabling::ScopedBypassEnablementChecksForTesting bypass_enablement_;
  base::test::ScopedFeatureList geic_feature_list_;
  GlicProfileManager glic_profile_manager_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> geic_profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<GlicInstanceMetrics> geic_metrics_;
};

TEST_F(GlicInstanceMetricsGeicTest,
       PanelPresentationTime_ActiveFlow_SidePanel_Geic) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  geic_metrics_->OnOpen(mojom::InvocationSource::kTopChromeButton,
                        show_options);
  geic_metrics_->OnVisibilityChanged(true);
  task_environment_.FastForwardBy(base::Milliseconds(350));
  geic_metrics_->OnClientReady();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Enterprise.Instance.PanelPresentationTime.SidePanel",
      base::Milliseconds(350), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.SidePanel", 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.Inactive.SidePanel", 0);
}

TEST_F(GlicInstanceMetricsGeicTest,
       PanelPresentationTime_InactiveFlow_SidePanel_Geic) {
  ShowOptions show_options{SidePanelShowOptions(mock_tab_)};
  geic_metrics_->OnOpen(mojom::InvocationSource::kTopChromeButton,
                        show_options);
  geic_metrics_->OnVisibilityChanged(false);
  task_environment_.FastForwardBy(base::Milliseconds(350));
  geic_metrics_->OnClientReady();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Enterprise.Instance.PanelPresentationTime.Inactive.SidePanel",
      base::Milliseconds(350), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.Inactive.SidePanel", 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.SidePanel", 0);
}

TEST_F(GlicInstanceMetricsGeicTest, WebUiLoadTime_Geic) {
  ShowOptions show_options{FloatingShowOptions{}};
  geic_metrics_->OnOpen(mojom::InvocationSource::kTopChromeButton,
                        show_options);
  geic_metrics_->OnVisibilityChanged(true);

  geic_metrics_->OnWebUiStateChanged(mojom::WebUiState::kBeginLoad);
  task_environment_.FastForwardBy(base::Milliseconds(300));
  geic_metrics_->OnWebUiStateChanged(mojom::WebUiState::kReady);

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Enterprise.Instance.WebUiLoadTime.Visible", base::Milliseconds(300),
      1);
  histogram_tester_.ExpectTotalCount("Glic.Instance.WebUiLoadTime.Visible", 1);
}

TEST_F(GlicInstanceMetricsGeicTest, TurnResponseStartTime_Geic) {
  ShowOptions show_options{FloatingShowOptions{}};
  geic_metrics_->OnOpen(mojom::InvocationSource::kTopChromeButton,
                        show_options);
  geic_metrics_->OnVisibilityChanged(true);
  geic_metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText,
                                      mojom::PromptType::kUnspecified);
  task_environment_.FastForwardBy(base::Milliseconds(200));
  geic_metrics_->OnResponseStarted();

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Enterprise.Turn.ResponseStartTime", base::Milliseconds(200), 1);
  histogram_tester_.ExpectTotalCount("Glic.Turn.ResponseStartTime", 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Enterprise.Turn.ResponseStartTime.InputMode.Text",
      base::Milliseconds(200), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.ResponseStartTime.InputMode.Text", 1);
}

TEST_F(GlicInstanceMetricsGeicTest, TurnDuration_Geic) {
  ShowOptions show_options{FloatingShowOptions{}};
  geic_metrics_->OnOpen(mojom::InvocationSource::kTopChromeButton,
                        show_options);
  geic_metrics_->OnVisibilityChanged(true);

  geic_metrics_->OnUserInputSubmitted(mojom::WebClientMode::kText,
                                      mojom::PromptType::kUnspecified);
  task_environment_.FastForwardBy(base::Milliseconds(500));
  geic_metrics_->OnTurnCompleted(mojom::WebClientModel::kActor,
                                 base::Milliseconds(500));

  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Enterprise.Turn.Duration.Actor", base::Milliseconds(500), 1);
  histogram_tester_.ExpectTotalCount("Glic.Turn.Duration.Actor", 1);
}
TEST_F(GlicInstanceMetricsGeicTest, PanelWebUiStateFinishState3_Geic) {
  ShowOptions show_options{FloatingShowOptions{}};
  geic_metrics_->OnOpen(mojom::InvocationSource::kTopChromeButton,
                        show_options);
  geic_metrics_->OnWebUiStateChanged(mojom::WebUiState::kReady);
  geic_metrics_->OnClose();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Enterprise.PanelWebUiState.FinishState3", mojom::WebUiState::kReady,
      1);
  histogram_tester_.ExpectTotalCount("Glic.PanelWebUiState.FinishState3", 1);
}

class GlicInstanceProviderMetricsTest
    : public testing::TestWithParam<GlicProvider> {
 protected:
  GlicInstanceProviderMetricsTest() {
    if (GetParam() == GlicProvider::kGeminiEnterprise) {
      feature_list_.InitAndEnableFeatureWithParameters(
          features::kGeic,
          {{"geic-guest-url", "https://business.gemini.google/side-panel"}});
    } else {
      feature_list_.InitAndDisableFeature(features::kGeic);
    }
  }

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    TestingProfile::TestingFactories factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    factories.push_back(
        {GlicKeyedServiceFactory::GetInstance(),
         base::BindRepeating(
             [](ProfileManager* pm, GlicProfileManager* gpm,
                content::BrowserContext* context)
                 -> std::unique_ptr<KeyedService> {
               return std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
                   context,
                   IdentityManagerFactory::GetForProfile(
                       Profile::FromBrowserContext(context)),
                   pm, gpm,
                   /*contextual_cueing_service=*/nullptr,
                   /*actor_keyed_service=*/nullptr);
             },
             testing_profile_manager_.profile_manager(),
             &glic_profile_manager_)});

    profile_ = testing_profile_manager_.CreateTestingProfile(
        "profile", std::move(factories));
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    ASSERT_TRUE(GlicKeyedServiceFactory::GetGlicKeyedService(profile_,
                                                             /*create=*/true));

    metrics_ = std::make_unique<GlicInstanceMetrics>(&profile_metrics_service_,
                                                     profile_);
  }

  GlicEnabling::ScopedBypassEnablementChecksForTesting bypass_enablement_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  GlicProfileManager glic_profile_manager_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  metrics::ProfileMetricsService profile_metrics_service_{1};
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
  std::unique_ptr<GlicInstanceMetrics> metrics_;
};

TEST_P(GlicInstanceProviderMetricsTest, GeicConsumerNames) {
  metrics_->OnActivationChanged(true);
  task_environment_.FastForwardBy(base::Minutes(5));
  metrics_->OnActivationChanged(false);
  task_environment_.FastForwardBy(base::Minutes(10));
  metrics_->OnActivationChanged(true);

  metrics_->OnTurnCompleted(mojom::WebClientModel::kDefault,
                            base::Milliseconds(100));
  metrics_->OnReaction(mojom::MetricUserInputReactionType::kModel);

  std::string specific_prefix = (GetParam() == GlicProvider::kGeminiEnterprise)
                                    ? "Glic.Enterprise"
                                    : "Glic.Consumer";
  std::string other_prefix = (GetParam() == GlicProvider::kGeminiEnterprise)
                                 ? "Glic.Consumer"
                                 : "Glic.Enterprise";

  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeSinceLastActive", 1);
  histogram_tester_.ExpectTotalCount(
      specific_prefix + ".Instance.TimeSinceLastActive", 1);
  histogram_tester_.ExpectTotalCount(
      other_prefix + ".Instance.TimeSinceLastActive", 0);

  // Check Glic.X=1
  histogram_tester_.ExpectUniqueTimeSample("Glic.Turn.Duration.Default",
                                           base::Milliseconds(100), 1);
  // Check specific prefix=1
  histogram_tester_.ExpectUniqueTimeSample(
      specific_prefix + ".Turn.Duration.Default", base::Milliseconds(100), 1);
  // Check other prefix=0
  histogram_tester_.ExpectTotalCount(other_prefix + ".Turn.Duration.Default",
                                     0);

  // Check no-dot action GlicReactionModelled
  EXPECT_EQ(1, user_action_tester_.GetActionCount("GlicReactionModelled"));

  // Check dotted action Glic.Instance.Created (emitted in constructor)
  EXPECT_EQ(1, user_action_tester_.GetActionCount("Glic.Instance.Created"));

  metrics_->OnFloatyClosed();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.Metrics.Error",
      GlicInstanceMetricsError::kFloatyClosedWithoutOpen, 1);
  histogram_tester_.ExpectUniqueSample(
      specific_prefix + ".Instance.Metrics.Error",
      GlicInstanceMetricsError::kFloatyClosedWithoutOpen, 1);
  histogram_tester_.ExpectTotalCount(other_prefix + ".Instance.Metrics.Error",
                                     0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicInstanceProviderMetricsTest,
                         testing::Values(GlicProvider::kGemini,
                                         GlicProvider::kGeminiEnterprise));

TEST_F(GlicInstanceMetricsTest,
       FreImpressionCuiTracker_SuccessOnUpfrontFreAndCancelledOnMessageFirst) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicOptInImpressionMetrics);

  ShowOptions show_options{FloatingShowOptions{}};

  // 1. Upfront FRE: user opens Glic while unconsented and FRE is shown.
  metrics_.OnVisibilityChanged(true);
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  task_environment_.FastForwardBy(base::Milliseconds(250));
  metrics_.OnOptinImpression();
  metrics_.OnClientReady();

  histogram_tester_.ExpectBucketCount("Glic.CUI.FreImpression.Outcome",
                                      GlicCuiOutcome::kSuccess, 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.CUI.FreImpression.Latency.Success", base::Milliseconds(250), 1);

  // 2. Message-First FRE: user opens Glic while unconsented and submits input
  // before FRE is shown -> resolves as kUnknownCancel without recording
  // latency.
  metrics_.OnInstanceHidden();
  metrics_.OnVisibilityChanged(true);
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  task_environment_.FastForwardBy(base::Milliseconds(120));
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);

  histogram_tester_.ExpectBucketCount("Glic.CUI.FreImpression.Outcome",
                                      GlicCuiOutcome::kUnknownCancel, 1);
  histogram_tester_.ExpectTotalCount("Glic.CUI.FreImpression.Latency", 1);
}

TEST_F(GlicInstanceMetricsTest,
       FreImpressionCuiTracker_SuccessWhenWarmedInBackground) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicOptInImpressionMetrics);

  ShowOptions show_options{FloatingShowOptions{}};

  // Instance is warmed in the background while hidden: the web client becomes
  // ready and signals OnOptinImpression() before the panel becomes visible or
  // opened.
  metrics_.OnOptinImpression();
  metrics_.OnClientReady();
  histogram_tester_.ExpectTotalCount("Glic.CUI.FreImpression.Outcome", 0);

  // User opens the warmed instance: OnVisibilityChanged(true) records the
  // pending opt-in impression, and OnOpen() immediately resolves the FRE
  // impression CUI tracker as kSuccess.
  metrics_.OnVisibilityChanged(true);
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);

  histogram_tester_.ExpectBucketCount("Glic.CUI.FreImpression.Outcome",
                                      GlicCuiOutcome::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("Glic.CUI.FreImpression.Latency.Success",
                                     1);
}

TEST_F(GlicInstanceMetricsTest,
       ResponseCompletionCuiTracker_SuccessAndUserStop) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  histogram_tester_.ExpectBucketCount("Glic.CUI.ResponseCompletion.Outcome",
                                      GlicCuiOutcome::kSuccess, 1);

  // Second turn stopped by user should resolve ResponseCompletion as kAbandoned
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);

  histogram_tester_.ExpectBucketCount("Glic.CUI.ResponseCompletion.Outcome",
                                      GlicCuiOutcome::kAbandoned, 1);

  // Third turn stopped by system (kOther) should resolve ResponseCompletion as
  // kFailed
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kOther);

  histogram_tester_.ExpectBucketCount("Glic.CUI.ResponseCompletion.Outcome",
                                      GlicCuiOutcome::kFailed, 1);
}

TEST_F(GlicInstanceMetricsTest, OnInputFocused_RecordsLatencyOncePerCycle) {
  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));

  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  task_environment_.FastForwardBy(base::Milliseconds(100));
  metrics_.OnClientReady();
  task_environment_.FastForwardBy(base::Milliseconds(50));

  metrics_.OnInputFocused();
  // Second focus in the same open should not re-record.
  metrics_.OnInputFocused();
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Instance.InputFocused"),
            2);

  histogram_tester_.ExpectUniqueTimeSample("Glic.Instance.TimeToInputFocus",
                                           base::Milliseconds(150), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToInputFocus.Cold", base::Milliseconds(150), 1);
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToInputFocus.Warm", 0);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.ClientReadyToInputFocus", base::Milliseconds(50), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToInputFocus.NonOnboarded", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToInputFocus.Cold.NonOnboarded", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.ClientReadyToInputFocus.NonOnboarded", 0);

  // Hide and warm-reopen: should record TimeToInputFocus.Warm.
  metrics_.OnInstanceHidden();
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  task_environment_.FastForwardBy(base::Milliseconds(35));
  metrics_.OnInputFocused();

  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToInputFocus", 2);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToInputFocus.Warm", base::Milliseconds(35), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToInputFocus.Warm.NonOnboarded", 0);
}

TEST_F(GlicInstanceMetricsTest,
       OnInputFocused_NonOnboardedRecordsVariantUntilConsented) {
  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      static_cast<int>(prefs::FreStatus::kNotStarted));

  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  task_environment_.FastForwardBy(base::Milliseconds(100));
  metrics_.OnClientReady();
  task_environment_.FastForwardBy(base::Milliseconds(50));
  metrics_.OnInputFocused();

  histogram_tester_.ExpectUniqueTimeSample("Glic.Instance.TimeToInputFocus",
                                           base::Milliseconds(150), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToInputFocus.NonOnboarded", base::Milliseconds(150),
      1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToInputFocus.Cold.NonOnboarded",
      base::Milliseconds(150), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.ClientReadyToInputFocus.NonOnboarded",
      base::Milliseconds(50), 1);

  // Warm reopen while still non-onboarded (e.g., Message-First FRE or
  // Just-In-Time consent before acceptance).
  metrics_.OnInstanceHidden();
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  task_environment_.FastForwardBy(base::Milliseconds(35));
  metrics_.OnInputFocused();

  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToInputFocus.NonOnboarded", 2);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Instance.TimeToInputFocus.Warm.NonOnboarded",
      base::Milliseconds(35), 1);

  // Once the profile completes onboarding, subsequent opens only record the
  // base histograms.
  profile_.GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre, static_cast<int>(prefs::FreStatus::kCompleted));
  metrics_.OnInstanceHidden();
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  task_environment_.FastForwardBy(base::Milliseconds(25));
  metrics_.OnInputFocused();

  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToInputFocus", 3);
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToInputFocus.NonOnboarded", 2);
}

TEST_F(GlicInstanceMetricsTest, ShowingOutcome_Default_VisibleAndActive) {
  ShowOptions show_options = ShowOptions::ForSidePanel(mock_tab_);

  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kTopChromeButton, show_options),
      /*has_conversation_id=*/false);
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnActivationChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  histogram_tester_.ExpectBucketCount("Glic.Turn.ShowingOutcome.Default",
                                      GlicTurnShowingOutcome::kShowingAndActive,
                                      1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome", GlicTurnShowingOutcome::kShowingAndActive, 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed", 0);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(GlicInstanceMetricsTest, ShowingOutcome_Actor_VisibleAndActive) {
  ShowOptions show_options{FloatingShowOptions{}};

  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kTopChromeButton, show_options),
      /*has_conversation_id=*/false);
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnActivationChanged(true);

  // Actor turn: submit prompt, submit action result, stop response.
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnActionSubmitted(/*is_retry=*/false);
  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  histogram_tester_.ExpectBucketCount("Glic.Turn.ShowingOutcome.Actor",
                                      GlicTurnShowingOutcome::kShowingAndActive,
                                      1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome", GlicTurnShowingOutcome::kShowingAndActive, 1);
  histogram_tester_.ExpectTotalCount("Glic.Turn.ShowingOutcome.Default", 0);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(GlicInstanceMetricsTest, ShowingOutcome_ShowingInactiveThenActive) {
  ShowOptions show_options = ShowOptions::ForSidePanel(mock_tab_);

  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kTopChromeButton, show_options),
      /*has_conversation_id=*/false);
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnActivationChanged(false);

  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnResponseStarted();
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  // Visible but inactive when response completes: pending, not recorded yet.
  histogram_tester_.ExpectTotalCount("Glic.Turn.ShowingOutcome.Default", 0);
  histogram_tester_.ExpectTotalCount("Glic.Turn.ShowingOutcome", 0);

  // Window becomes active later.
  metrics_.OnActivationChanged(true);

  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome.Default",
      GlicTurnShowingOutcome::kShowingInactiveThenActive, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome",
      GlicTurnShowingOutcome::kShowingInactiveThenActive, 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed", 0);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(GlicInstanceMetricsTest, ShowingOutcome_ShowingInactiveNeverActive) {
  ShowOptions show_options{FloatingShowOptions{}};

  {
    GlicInstanceMetrics metrics(&profile_metrics_service_, &profile_);
    metrics.OnWillShow(
        ShowOptionsFor(mojom::InvocationSource::kTopChromeButton, show_options),
        /*has_conversation_id=*/false);
    metrics.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
    metrics.OnVisibilityChanged(true);
    metrics.OnActivationChanged(false);
    metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                 mojom::PromptType::kTypedText);
    metrics.OnResponseStarted();
    metrics.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

    histogram_tester_.ExpectTotalCount("Glic.Turn.ShowingOutcome.Default", 0);
    histogram_tester_.ExpectTotalCount("Glic.Turn.ShowingOutcome", 0);
  }  // Destructor calls OnInstanceDestroyed().

  histogram_tester_.ExpectUniqueSample(
      "Glic.Turn.ShowingOutcome.Default",
      GlicTurnShowingOutcome::kShowingInactiveNeverActive, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Turn.ShowingOutcome",
      GlicTurnShowingOutcome::kShowingInactiveNeverActive, 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed", 0);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(GlicInstanceMetricsTest, CompletedWhileHidden_ShownOutcomesAndTimes) {
  ShowOptions show_options = ShowOptions::ForSidePanel(mock_tab_);

  // 1. Shown via tab switch back:
  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kTopChromeButton, show_options),
      /*has_conversation_id=*/false);
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnActivationChanged(true);
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnResponseStarted();
  // Tab switched away.
  metrics_.OnVisibilityChanged(false);
  // Response completes while hidden.
  task_environment_.FastForwardBy(base::Seconds(2));
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  // Still hidden, so outcome, return source, and time to show not recorded yet.
  histogram_tester_.ExpectTotalCount("Glic.Turn.ShowingOutcome.Default", 0);
  histogram_tester_.ExpectTotalCount("Glic.Turn.ShowingOutcome", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", 0);

  // User switches back to tab after 5 seconds.
  task_environment_.FastForwardBy(base::Seconds(5));
  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kReshowInactive, show_options),
      /*has_conversation_id=*/true);
  metrics_.OnVisibilityChanged(true);

  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome.Default",
      GlicTurnShowingOutcome::kHiddenThenShowing, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome", GlicTurnShowingOutcome::kHiddenThenShowing,
      1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource.Default",
      mojom::InvocationSource::kReshowInactive, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource",
      mojom::InvocationSource::kReshowInactive, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", base::Seconds(5), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", base::Seconds(5), 1);

  // 2. Shown via panel reopen:
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnResponseStarted();
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  metrics_.OnVisibilityChanged(false);
  task_environment_.FastForwardBy(base::Seconds(1));
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  task_environment_.FastForwardBy(base::Seconds(10));
  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kTopChromeButton, show_options),
      /*has_conversation_id=*/true);
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);

  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome.Default",
      GlicTurnShowingOutcome::kHiddenThenShowing, 2);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome", GlicTurnShowingOutcome::kHiddenThenShowing,
      2);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource.Default",
      mojom::InvocationSource::kTopChromeButton, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource",
      mojom::InvocationSource::kTopChromeButton, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", base::Seconds(10),
      1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", base::Seconds(10), 1);

  // 3. Shown via activity hub:
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnResponseStarted();
  metrics_.OnVisibilityChanged(false);
  task_environment_.FastForwardBy(base::Seconds(1));
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  task_environment_.FastForwardBy(base::Seconds(3));
  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kActivityListBubble,
                     show_options),
      /*has_conversation_id=*/true);
  metrics_.OnVisibilityChanged(true);

  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome.Default",
      GlicTurnShowingOutcome::kHiddenThenShowing, 3);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome", GlicTurnShowingOutcome::kHiddenThenShowing,
      3);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource.Default",
      mojom::InvocationSource::kActivityListBubble, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource",
      mojom::InvocationSource::kActivityListBubble, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", base::Seconds(3), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", base::Seconds(3), 1);

  // 4. Shown via conversation switch:
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnResponseStarted();
  metrics_.OnVisibilityChanged(false);
  task_environment_.FastForwardBy(base::Seconds(1));
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  task_environment_.FastForwardBy(base::Seconds(8));
  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kConversationSwitch,
                     show_options),
      /*has_conversation_id=*/true);
  metrics_.OnVisibilityChanged(true);

  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome.Default",
      GlicTurnShowingOutcome::kHiddenThenShowing, 4);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome", GlicTurnShowingOutcome::kHiddenThenShowing,
      4);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource.Default",
      mojom::InvocationSource::kConversationSwitch, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource",
      mojom::InvocationSource::kConversationSwitch, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", base::Seconds(8), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", base::Seconds(8), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", 4);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", 4);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed", 0);

  // 5. Surface move while already visible does not re-trigger.
  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kDetachAttachButton,
                     show_options),
      /*has_conversation_id=*/true);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", 4);
  histogram_tester_.ExpectTotalCount("Glic.Turn.ShowingOutcome.Default", 4);

  // 6. User-stopped response does not start pending state or record outcomes.
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnResponseStarted();
  metrics_.OnVisibilityChanged(false);
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUser);
  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kTopChromeButton, show_options),
      /*has_conversation_id=*/true);
  metrics_.OnVisibilityChanged(true);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", 4);
  histogram_tester_.ExpectTotalCount("Glic.Turn.ShowingOutcome.Default", 4);

  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(GlicInstanceMetricsTest, CompletedWhileHidden_NeverShowing) {
  ShowOptions show_options{FloatingShowOptions{}};

  // 1. Default instance destroyed while hidden:
  {
    GlicInstanceMetrics metrics(&profile_metrics_service_, &profile_);
    metrics.OnWillShow(
        ShowOptionsFor(mojom::InvocationSource::kTopChromeButton, show_options),
        /*has_conversation_id=*/false);
    metrics.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
    metrics.OnVisibilityChanged(true);
    metrics.OnActivationChanged(true);
    metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                 mojom::PromptType::kTypedText);
    metrics.OnResponseStarted();
    metrics.OnVisibilityChanged(false);
    metrics.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

    task_environment_.FastForwardBy(base::Seconds(12));
  }  // Destructor calls OnInstanceDestroyed().

  histogram_tester_.ExpectUniqueSample(
      "Glic.Turn.ShowingOutcome.Default",
      GlicTurnShowingOutcome::kHiddenNeverShowing, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Turn.ShowingOutcome", GlicTurnShowingOutcome::kHiddenNeverShowing,
      1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource", 0);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed.Default",
      base::Seconds(12), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed", base::Seconds(12),
      1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Consumer.Turn.CompletedWhileHidden.TimeUntilDestroyed.Default",
      base::Seconds(12), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Consumer.Turn.CompletedWhileHidden.TimeUntilDestroyed",
      base::Seconds(12), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", 0);

  // 2. Actor instance destroyed while hidden:
  {
    GlicInstanceMetrics metrics(&profile_metrics_service_, &profile_);
    metrics.OnWillShow(
        ShowOptionsFor(mojom::InvocationSource::kTopChromeButton, show_options),
        /*has_conversation_id=*/false);
    metrics.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
    metrics.OnVisibilityChanged(true);
    metrics.OnActivationChanged(true);
    metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                 mojom::PromptType::kTypedText);
    metrics.OnActionSubmitted(/*is_retry=*/false);
    metrics.OnResponseStarted();
    metrics.OnVisibilityChanged(false);
    metrics.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

    task_environment_.FastForwardBy(base::Seconds(7));
  }  // Destructor calls OnInstanceDestroyed().

  histogram_tester_.ExpectUniqueSample(
      "Glic.Turn.ShowingOutcome.Actor",
      GlicTurnShowingOutcome::kHiddenNeverShowing, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome", GlicTurnShowingOutcome::kHiddenNeverShowing,
      2);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource.Actor", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource", 0);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed.Actor",
      base::Seconds(7), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed", base::Seconds(7), 1);
  histogram_tester_.ExpectUniqueTimeSample(
      "Glic.Consumer.Turn.CompletedWhileHidden.TimeUntilDestroyed.Actor",
      base::Seconds(7), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Consumer.Turn.CompletedWhileHidden.TimeUntilDestroyed",
      base::Seconds(7), 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Actor", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", 0);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(
    GlicInstanceMetricsTest,
    CompletedWhileHidden_MultiStepActorTaskDoesNotRecordIntermediateNeverShown) {
  ShowOptions show_options{FloatingShowOptions{}};

  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kTopChromeButton, show_options),
      /*has_conversation_id=*/false);
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnActivationChanged(true);

  // Turn 1: user submits prompt. Instance is hidden and response completes.
  metrics_.OnUserInputSubmitted(mojom::WebClientMode::kText,
                                mojom::PromptType::kTypedText);
  metrics_.OnResponseStarted();
  metrics_.OnVisibilityChanged(false);
  task_environment_.FastForwardBy(base::Seconds(2));
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  // Step 1: action result submitted in background. Intermediate response
  // completes.
  task_environment_.FastForwardBy(base::Seconds(5));
  metrics_.OnActionSubmitted(/*is_retry=*/false);
  metrics_.OnResponseStarted();
  task_environment_.FastForwardBy(base::Seconds(1));
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  // Step 2: another action result submitted in background. Final response
  // completes.
  task_environment_.FastForwardBy(base::Seconds(5));
  metrics_.OnActionSubmitted(/*is_retry=*/false);
  metrics_.OnResponseStarted();
  task_environment_.FastForwardBy(base::Seconds(1));
  metrics_.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  // Intermediate steps should NOT emit false never-shown events.
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome.Actor",
      GlicTurnShowingOutcome::kHiddenNeverShowing, 0);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome.Default",
      GlicTurnShowingOutcome::kHiddenNeverShowing, 0);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome", GlicTurnShowingOutcome::kHiddenNeverShowing,
      0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed.Actor", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed.Default", 0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Turn.CompletedWhileHidden.TimeUntilDestroyed", 0);

  // User views response 4s after final step completes.
  task_environment_.FastForwardBy(base::Seconds(4));
  metrics_.OnWillShow(
      ShowOptionsFor(mojom::InvocationSource::kReshowInactive, show_options),
      /*has_conversation_id=*/true);
  metrics_.OnVisibilityChanged(true);

  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome.Actor",
      GlicTurnShowingOutcome::kHiddenThenShowing, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.ShowingOutcome", GlicTurnShowingOutcome::kHiddenThenShowing,
      1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource.Actor",
      mojom::InvocationSource::kReshowInactive, 1);
  histogram_tester_.ExpectBucketCount(
      "Glic.Turn.CompletedWhileHidden.ReturnSource",
      mojom::InvocationSource::kReshowInactive, 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow.Actor", base::Seconds(4), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "Glic.Turn.CompletedWhileHidden.TimeToShow", base::Seconds(4), 1);
  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(GlicInstanceMetricsTest, ReturnSource_RecordedCorrectly) {
  ShowOptions show_options = ShowOptions::ForSidePanel(mock_tab_);
  auto options_for = [&](mojom::InvocationSource source) {
    ShowOptions options = show_options;
    options.invocation_source = source;
    return options;
  };

  // Cold open with no conversation history -> ReturnSource NOT recorded.
  metrics_.OnWillShow(options_for(mojom::InvocationSource::kTopChromeButton),
                      /*has_conversation_id=*/false);
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);
  histogram_tester_.ExpectTotalCount("Glic.Instance.ReturnSource", 0);

  // Conversation switch while visible with no conversation ID -> NOT recorded.
  metrics_.OnWillShow(options_for(mojom::InvocationSource::kConversationSwitch),
                      /*has_conversation_id=*/false);
  histogram_tester_.ExpectTotalCount("Glic.Instance.ReturnSource", 0);

  // Tab switch away and back with no conversation ID -> NOT recorded.
  metrics_.OnVisibilityChanged(false);
  metrics_.OnWillShow(options_for(mojom::InvocationSource::kReshowInactive),
                      /*has_conversation_id=*/false);
  metrics_.OnVisibilityChanged(true);
  histogram_tester_.ExpectTotalCount("Glic.Instance.ReturnSource", 0);

  // Tab switch away and back with conversation ID.
  metrics_.OnVisibilityChanged(false);
  metrics_.OnWillShow(options_for(mojom::InvocationSource::kReshowInactive),
                      /*has_conversation_id=*/true);
  metrics_.OnVisibilityChanged(true);

  histogram_tester_.ExpectBucketCount("Glic.Instance.ReturnSource",
                                      mojom::InvocationSource::kReshowInactive,
                                      1);

  // Surface move while already visible -> ReturnSource NOT recorded.
  metrics_.OnWillShow(options_for(mojom::InvocationSource::kDetachAttachButton),
                      /*has_conversation_id=*/true);
  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.ReturnSource",
      mojom::InvocationSource::kDetachAttachButton, 0);

  // Close panel and reopen via Top Chrome button.
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  metrics_.OnVisibilityChanged(false);
  metrics_.OnWillShow(options_for(mojom::InvocationSource::kTopChromeButton),
                      /*has_conversation_id=*/true);
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);

  histogram_tester_.ExpectBucketCount("Glic.Instance.ReturnSource",
                                      mojom::InvocationSource::kTopChromeButton,
                                      1);

  // Close panel and reopen via Hotkey.
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  metrics_.OnVisibilityChanged(false);
  metrics_.OnWillShow(options_for(mojom::InvocationSource::kOsHotkey),
                      /*has_conversation_id=*/true);
  metrics_.OnOpen(mojom::InvocationSource::kOsHotkey, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);

  histogram_tester_.ExpectBucketCount("Glic.Instance.ReturnSource",
                                      mojom::InvocationSource::kOsHotkey, 1);

  // Close panel and reopen via Activity Hub.
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  metrics_.OnVisibilityChanged(false);
  metrics_.OnWillShow(options_for(mojom::InvocationSource::kActivityListBubble),
                      /*has_conversation_id=*/true);
  metrics_.OnOpen(mojom::InvocationSource::kActivityListBubble, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);

  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.ReturnSource",
      mojom::InvocationSource::kActivityListBubble, 1);

  // Re-open via Actor task icon.
  metrics_.OnSidePanelClosed(
      &mock_tab_, GlicInstanceMetrics::CloseReason::kExplicitlyClosed);
  metrics_.OnVisibilityChanged(false);
  metrics_.OnWillShow(options_for(mojom::InvocationSource::kActorTaskIcon),
                      /*has_conversation_id=*/true);
  metrics_.OnOpen(mojom::InvocationSource::kActorTaskIcon, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);

  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.ReturnSource", mojom::InvocationSource::kActorTaskIcon, 1);

  // In-place conversation switch while visible.
  metrics_.OnWillShow(options_for(mojom::InvocationSource::kConversationSwitch),
                      /*has_conversation_id=*/true);

  histogram_tester_.ExpectBucketCount(
      "Glic.Instance.ReturnSource",
      mojom::InvocationSource::kConversationSwitch, 1);

  histogram_tester_.ExpectTotalCount("Glic.Instance.Metrics.Error", 0);
}

TEST_F(GlicInstanceMetricsTest, TooltipMetrics_Impression) {
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  histogram_tester_.ExpectUniqueSample("Glic.Tooltip.MultiTab.Impression",
                                       mojom::TooltipImpressionType::kTooltip1,
                                       1);

  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip2a);
  histogram_tester_.ExpectBucketCount("Glic.Tooltip.MultiTab.Impression",
                                      mojom::TooltipImpressionType::kTooltip2a,
                                      1);
  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.Impression", 2);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_OnFloatyClosed) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnShowInFloaty(show_options);

  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  metrics_.OnFloatyClosed();

  histogram_tester_.ExpectUniqueSample("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                       true, 1);

  // Subsequent destruction should NOT double-record.
  metrics_.OnInstanceDestroyed();
  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_OnSidePanelClosed) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);

  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectUniqueSample("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                       true, 1);

  // Subsequent destruction should NOT double-record.
  metrics_.OnInstanceDestroyed();
  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_OnUnbindOpenEmbedder) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);

  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  task_environment_.FastForwardBy(base::Seconds(3));
  metrics_.OnUnbindEmbedder(SidePanelEmbedderKey{&mock_tab_});

  histogram_tester_.ExpectUniqueSample("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", true, 1);

  metrics_.OnInstanceDestroyed();
  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_OnUnbindOpenFloatyEmbedder) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInFloaty(show_options);

  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  task_environment_.FastForwardBy(base::Seconds(3));
  metrics_.OnUnbindEmbedder(FloatingEmbedderKey{});

  histogram_tester_.ExpectUniqueSample("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", true, 1);

  metrics_.OnInstanceDestroyed();
  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_OnBoundTabDestroyed) {
  tabs::MockTabInterface background_tab;
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  EXPECT_CALL(background_tab, GetTabHandle())
      .WillRepeatedly(testing::Return(2));

  // Open side panel on background_tab, then switch away (kTabSwitched removes
  // it from `side_panel_open_times_`) and open on mock_tab_.
  ShowOptions bg_options{SidePanelShowOptions{background_tab}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, bg_options);
  metrics_.OnShowInSidePanel(&background_tab);
  metrics_.OnSidePanelClosed(static_cast<tabs::TabInterface*>(&background_tab),
                             GlicInstanceMetrics::CloseReason::kTabSwitched);

  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  task_environment_.FastForwardBy(base::Seconds(3));

  // Closing the backgrounded tab must NOT falsely emit tooltip close metrics.
  metrics_.OnBoundTabDestroyed(&background_tab, /*is_active_embedder=*/false);
  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 0);

  // Closing the active open-panel tab emits both tooltip close metrics once.
  metrics_.OnBoundTabDestroyed(&mock_tab_, /*is_active_embedder=*/true);
  histogram_tester_.ExpectUniqueSample("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", true, 1);

  metrics_.OnInstanceDestroyed();
  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     1);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_DetachThenCloseOriginalTab) {
  tabs::MockTabInterface detached_tab;
  EXPECT_CALL(detached_tab, GetTabHandle()).WillRepeatedly(testing::Return(2));

  ShowOptions sp_options{SidePanelShowOptions{detached_tab}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, sp_options);
  metrics_.OnShowInSidePanel(&detached_tab);

  // Detaching to Floaty leaves `detached_tab` in `side_panel_open_times_`
  // without an OnSidePanelClosed() callback.
  metrics_.OnDetach();
  metrics_.OnShowInFloaty(ShowOptions{FloatingShowOptions{}});
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  task_environment_.FastForwardBy(base::Seconds(3));

  // Closing the original tab while Floaty is active must NOT emit close metrics
  // or clear the tooltip flag before the Floaty window closes.
  metrics_.OnBoundTabDestroyed(&detached_tab, /*is_active_embedder=*/false);
  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 0);

  metrics_.OnFloatyClosed();
  histogram_tester_.ExpectUniqueSample("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                       true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", true, 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_OnInstanceDestroyed) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  metrics_.OnInstanceDestroyed();

  histogram_tester_.ExpectUniqueSample("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                       true, 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_RecordsFalseAfterHidden) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnShowInFloaty(show_options);

  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  metrics_.OnTooltipHidden();
  task_environment_.FastForwardBy(base::Seconds(2));
  metrics_.OnFloatyClosed();

  histogram_tester_.ExpectUniqueSample("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                       false, 1);
  // PanelCloseWithin10s still tracks closes within 10s of an impression.
  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", true, 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_ReShownAfterHidden) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  metrics_.OnTooltipHidden();
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip2a);
  metrics_.OnInstanceDestroyed();

  histogram_tester_.ExpectUniqueSample("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                       true, 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_NotEmittedOnTabSwitched) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);
  metrics_.OnVisibilityChanged(true);

  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  metrics_.OnSidePanelClosed(static_cast<tabs::TabInterface*>(&mock_tab_),
                             GlicInstanceMetrics::CloseReason::kTabSwitched);
  metrics_.OnVisibilityChanged(false);

  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 0);

  // Destroying the hidden switched-away instance later must not falsely record
  // either close metric.
  metrics_.OnInstanceDestroyed();
  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 0);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_PreservedAcrossClientReady) {
  metrics_.OnShowInFloaty(ShowOptions{FloatingShowOptions{}});
  metrics_.OnVisibilityChanged(true);
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);

  // PanelWillOpenComplete (OnClientReady) can arrive after initial tooltip
  // impressions without tearing down the web client.
  metrics_.OnClientReady();
  metrics_.OnTooltipHidden();
  metrics_.OnFloatyClosed();

  histogram_tester_.ExpectUniqueSample("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                       false, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", true, 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_ClearedOnWebClientCleared) {
  metrics_.OnShowInFloaty(ShowOptions{FloatingShowOptions{}});
  metrics_.OnVisibilityChanged(true);
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);

  // Simulate web client reload, crash recovery, or hibernation.
  metrics_.OnWebClientCleared();
  metrics_.OnFloatyClosed();

  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 0);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_ClosedWhileShown_NotEmittedWithoutImpressionOrUnknown) {
  metrics_.OnShowInFloaty(ShowOptions{FloatingShowOptions{}});
  metrics_.OnFloatyClosed();
  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     0);

  metrics_.OnVisibilityChanged(true);
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kUnknown);
  metrics_.OnInstanceDestroyed();

  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.ClosedWhileShown",
                                     0);
}

TEST_F(GlicInstanceMetricsTest, TooltipMetrics_MultiTabPromptSubmitted) {
  testing::NiceMock<MockGlicSharingManager> sharing_manager;
  GlicInstanceMetrics metrics(&profile_metrics_service_, &sharing_manager,
                              nullptr, &profile_);
  tabs::MockTabInterface tab1;
  tabs::MockTabInterface tab2;
  std::vector<raw_ptr<tabs::TabInterface>> pinned_tabs = {&tab1, &tab2};
  EXPECT_CALL(sharing_manager, GetPinnedTabs())
      .WillRepeatedly(testing::ReturnPointee(&pinned_tabs));

  metrics.OnVisibilityChanged(true);
  metrics.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);

  // First prompt in session synchronously triggers OnSessionStarted(); verify
  // the pre-prompt tooltip impression is preserved and MultiTabPromptSubmitted
  // is emitted when 2+ tabs are pinned.
  metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                               mojom::PromptType::kTypedText);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.MultiTabPromptSubmitted", true, 1);

  metrics.OnResponseStarted();
  metrics.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  // Follow-up prompts in the same session should NOT emit duplicate samples.
  metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                               mojom::PromptType::kTypedText);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.MultiTabPromptSubmitted", true, 1);
  metrics.OnResponseStarted();
  metrics.OnResponseStopped(mojom::ResponseStopCause::kUnknown);

  // Finish the session (via inactivity timeout) and verify tooltip state resets
  // so a subsequent session without a new impression does not emit.
  task_environment_.FastForwardBy(
      features::kGlicMetricsSessionInactivityTimeout.Get());
  metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                               mojom::PromptType::kTypedText);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.MultiTabPromptSubmitted", true, 1);
}

TEST_F(GlicInstanceMetricsTest, TooltipMetrics_UnknownImpressionIgnored) {
  testing::NiceMock<MockGlicSharingManager> sharing_manager;
  GlicInstanceMetrics metrics(&profile_metrics_service_, &sharing_manager,
                              nullptr, &profile_);
  tabs::MockTabInterface tab1;
  tabs::MockTabInterface tab2;
  std::vector<raw_ptr<tabs::TabInterface>> pinned_tabs = {&tab1, &tab2};
  EXPECT_CALL(sharing_manager, GetPinnedTabs())
      .WillRepeatedly(testing::ReturnPointee(&pinned_tabs));

  metrics.OnVisibilityChanged(true);
  metrics.OnTooltipImpression(mojom::TooltipImpressionType::kUnknown);
  histogram_tester_.ExpectTotalCount("Glic.Tooltip.MultiTab.Impression", 0);

  metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                               mojom::PromptType::kTypedText);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.MultiTabPromptSubmitted", 0);

  metrics.OnInstanceDestroyed();
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 0);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_MultiTabPromptSubmitted_RequiresTwoPinnedTabs) {
  testing::NiceMock<MockGlicSharingManager> sharing_manager;
  GlicInstanceMetrics metrics(&profile_metrics_service_, &sharing_manager,
                              nullptr, &profile_);
  tabs::MockTabInterface tab1;
  std::vector<raw_ptr<tabs::TabInterface>> pinned_tabs = {&tab1};
  EXPECT_CALL(sharing_manager, GetPinnedTabs())
      .WillRepeatedly(testing::ReturnPointee(&pinned_tabs));

  metrics.OnVisibilityChanged(true);
  metrics.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                               mojom::PromptType::kTypedText);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.MultiTabPromptSubmitted", 0);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_MultiTabPromptSubmitted_RequiresTooltipImpression) {
  testing::NiceMock<MockGlicSharingManager> sharing_manager;
  GlicInstanceMetrics metrics(&profile_metrics_service_, &sharing_manager,
                              nullptr, &profile_);
  tabs::MockTabInterface tab1;
  tabs::MockTabInterface tab2;
  std::vector<raw_ptr<tabs::TabInterface>> pinned_tabs = {&tab1, &tab2};
  EXPECT_CALL(sharing_manager, GetPinnedTabs())
      .WillRepeatedly(testing::ReturnPointee(&pinned_tabs));

  metrics.OnVisibilityChanged(true);
  metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                               mojom::PromptType::kTypedText);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.MultiTabPromptSubmitted", 0);
}

TEST_F(
    GlicInstanceMetricsTest,
    TooltipMetrics_MultiTabPromptSubmitted_PreservedAcrossPendingTabSwitchAndUnfocus) {
  testing::NiceMock<MockGlicSharingManager> sharing_manager;
  GlicInstanceMetrics metrics(&profile_metrics_service_, &sharing_manager,
                              nullptr, &profile_);
  tabs::MockTabInterface tab1;
  tabs::MockTabInterface tab2;
  std::vector<raw_ptr<tabs::TabInterface>> pinned_tabs = {&tab1};
  EXPECT_CALL(sharing_manager, GetPinnedTabs())
      .WillRepeatedly(testing::ReturnPointee(&pinned_tabs));
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));

  // Side Panel tab switch within the initial 5s pending window must NOT wipe
  // `tooltip_shown_in_session_`.
  ShowOptions side_panel_options{SidePanelShowOptions{mock_tab_}};
  metrics.OnOpen(mojom::InvocationSource::kTopChromeButton, side_panel_options);
  metrics.OnShowInSidePanel(&mock_tab_);
  metrics.OnVisibilityChanged(true);
  metrics.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);

  metrics.OnSidePanelClosed(static_cast<tabs::TabInterface*>(&mock_tab_),
                            GlicInstanceMetrics::CloseReason::kTabSwitched);
  metrics.OnVisibilityChanged(false);

  // User switches back, pins a second tab, and submits within < 5s.
  metrics.OnVisibilityChanged(true);
  pinned_tabs.push_back(&tab2);
  metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                               mojom::PromptType::kTypedText);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.MultiTabPromptSubmitted", true, 1);

  // Floaty focus loss (clicking browser window to pin a tab) within the
  // pending window must also preserve `tooltip_shown_in_session_`.
  pinned_tabs = {&tab1};
  metrics.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip2a);
  metrics.OnActivationChanged(false);
  metrics.OnActivationChanged(true);
  pinned_tabs.push_back(&tab2);
  metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                               mojom::PromptType::kTypedText);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.MultiTabPromptSubmitted", true, 2);
}

TEST_F(
    GlicInstanceMetricsTest,
    TooltipMetrics_MultiTabPromptSubmitted_ResetsOnExplicitCloseBeforeStart) {
  testing::NiceMock<MockGlicSharingManager> sharing_manager;
  GlicInstanceMetrics metrics(&profile_metrics_service_, &sharing_manager,
                              nullptr, &profile_);
  tabs::MockTabInterface tab1;
  tabs::MockTabInterface tab2;
  std::vector<raw_ptr<tabs::TabInterface>> pinned_tabs = {&tab1, &tab2};
  EXPECT_CALL(sharing_manager, GetPinnedTabs())
      .WillRepeatedly(testing::ReturnPointee(&pinned_tabs));

  ShowOptions floaty_options{FloatingShowOptions{}};
  metrics.OnOpen(mojom::InvocationSource::kTopChromeButton, floaty_options);
  metrics.OnShowInFloaty(floaty_options);
  metrics.OnVisibilityChanged(true);
  metrics.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);

  // Explicit panel close before session start clears
  // `tooltip_shown_in_session_`.
  metrics.OnFloatyClosed();
  metrics.OnVisibilityChanged(false);

  // Reopening later without a new tooltip impression does not credit the closed
  // tooltip.
  metrics.OnShowInFloaty(floaty_options);
  metrics.OnVisibilityChanged(true);
  metrics.OnUserInputSubmitted(mojom::WebClientMode::kText,
                               mojom::PromptType::kTypedText);
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.MultiTabPromptSubmitted", 0);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_PanelCloseWithin10s_NoImpressionAndRefreshesTimestamp) {
  {
    testing::NiceMock<MockGlicSharingManager> sharing_manager;
    GlicInstanceMetrics no_impression_metrics(
        &profile_metrics_service_, &sharing_manager, nullptr, &profile_);
    no_impression_metrics.OnVisibilityChanged(true);
    no_impression_metrics.OnInstanceDestroyed();
    histogram_tester_.ExpectTotalCount(
        "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 0);
  }

  metrics_.OnVisibilityChanged(true);
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  task_environment_.FastForwardBy(base::Seconds(8));
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip2a);
  task_environment_.FastForwardBy(base::Seconds(5));
  metrics_.OnInstanceDestroyed();

  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", true, 1);
}

TEST_F(GlicInstanceMetricsTest, TooltipMetrics_PanelCloseWithin10s_Emitted) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  task_environment_.FastForwardBy(base::Seconds(5));
  metrics_.OnInstanceDestroyed();

  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", true, 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_PanelCloseWithin10s_After10sRecordsFalse) {
  metrics_.OnVisibilityChanged(true);
  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  task_environment_.FastForwardBy(base::Seconds(11));
  metrics_.OnInstanceDestroyed();

  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", false, 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_PanelCloseWithin10s_OnSidePanelClosedAfter10s) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);

  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  task_environment_.FastForwardBy(base::Seconds(11));

  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", false, 1);

  // Subsequent destruction should NOT double-record.
  metrics_.OnInstanceDestroyed();
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_PanelCloseWithin10s_OnSidePanelClosed) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);

  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  task_environment_.FastForwardBy(base::Seconds(4));

  metrics_.OnSidePanelClosed(
      static_cast<tabs::TabInterface*>(&mock_tab_),
      GlicInstanceMetrics::CloseReason::kExplicitlyClosed);

  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", true, 1);

  // Subsequent destruction should NOT double-record.
  metrics_.OnInstanceDestroyed();
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 1);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_PanelCloseWithin10s_NotEmittedOnTabSwitched) {
  EXPECT_CALL(mock_tab_, GetTabHandle()).WillRepeatedly(testing::Return(1));
  ShowOptions show_options{SidePanelShowOptions{mock_tab_}};
  metrics_.OnOpen(mojom::InvocationSource::kTopChromeButton, show_options);
  metrics_.OnShowInSidePanel(&mock_tab_);

  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  task_environment_.FastForwardBy(base::Seconds(3));

  // Tab switch is not an explicit side panel close.
  metrics_.OnSidePanelClosed(static_cast<tabs::TabInterface*>(&mock_tab_),
                             GlicInstanceMetrics::CloseReason::kTabSwitched);

  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 0);
}

TEST_F(GlicInstanceMetricsTest,
       TooltipMetrics_PanelCloseWithin10s_OnFloatyClosed) {
  ShowOptions show_options{FloatingShowOptions{}};
  metrics_.OnShowInFloaty(show_options);

  metrics_.OnTooltipImpression(mojom::TooltipImpressionType::kTooltip1);
  task_environment_.FastForwardBy(base::Seconds(3));

  metrics_.OnFloatyClosed();

  histogram_tester_.ExpectUniqueSample(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", true, 1);

  // Subsequent destruction should NOT double-record.
  metrics_.OnInstanceDestroyed();
  histogram_tester_.ExpectTotalCount(
      "Glic.Tooltip.MultiTab.PanelCloseWithin10s", 1);
}

TEST_F(GlicInstanceMetricsTest,
       NativeFreShown_RecordsImpressionOnceAndSuppressesDuplicates) {
  base::test::ScopedFeatureList feature_list{
      features::kGlicOptInImpressionMetrics};
  ShowOptions show_options = MakeNativeFreShowOptions();

  // Pre-warmed while hidden before OnWillShow must not record until OnOpen.
  metrics_.OnClientReady();
  metrics_.OnOptinImpression();
  metrics_.OnWillShow(show_options, /*has_conversation_id=*/false);
  metrics_.OnVisibilityChanged(true);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Glic.Onboarding.OptInImpression"), 0);

  metrics_.OnOpen(mojom::InvocationSource::kFre, show_options);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Glic.Onboarding.OptInImpression"), 1);
  histogram_tester_.ExpectUniqueSample("Glic.CUI.FreImpression.Outcome",
                                       GlicCuiOutcome::kSuccess, 1);

  // Subsequent WebUI impression while visible must not double-record, even
  // after backgrounding and reshowing via kReshowInactive.
  metrics_.OnOptinImpression();
  metrics_.OnVisibilityChanged(false);
  metrics_.OnWillShow(ShowOptionsFor(mojom::InvocationSource::kReshowInactive,
                                     ShowOptions::ForSidePanel(mock_tab_)),
                      /*has_conversation_id=*/false);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnOptinImpression();
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Glic.Onboarding.OptInImpression"), 1);
}

TEST_F(GlicInstanceMetricsTest,
       NativeFreDismissOrError_SuppressesLoadingDismissAndActionableMetrics) {
  ShowOptions show_options = MakeNativeFreShowOptions();

  // Pre-accept dismiss counts as FRE decline, not loading abandon.
  metrics_.OnWillShow(show_options, /*has_conversation_id=*/false);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnOpen(mojom::InvocationSource::kFre, show_options);
  task_environment_.FastForwardBy(base::Milliseconds(500));
  metrics_.OnVisibilityChanged(false);
  metrics_.OnClose();

  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.TimeToDismissWhileLoading.SidePanel", 0);
  histogram_tester_.ExpectTotalCount("Glic.Fre.PanelWebUiState.FinishState", 1);

  // Load error after accept suppresses AcceptToChatActionable.
  metrics_.OnWillShow(show_options, /*has_conversation_id=*/false);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnOpen(mojom::InvocationSource::kFre, show_options);
  metrics_.OnNativeFreAccepted();
  metrics_.OnClientLoadError(ClientLoadErrorReason::kUnknown);
  metrics_.OnGuestFrameSubmitted();

  histogram_tester_.ExpectTotalCount("Glic.Fre.AcceptToChatActionable", 0);
}

TEST_F(
    GlicInstanceMetricsTest,
    NativeFreAccept_RecordsActionableOnShowAndPreservesWarmFrameAcrossReopen) {
  ShowOptions show_options = MakeNativeFreShowOptions();

  metrics_.OnWillShow(show_options, /*has_conversation_id=*/false);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnOpen(mojom::InvocationSource::kFre, show_options);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.PanelPresentationTime.SidePanel", 0, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.TimeToActionable.SidePanel", 0, 1);
  EXPECT_TRUE(metrics_.IsWaitingForActionable());

  // Cold background navigation commit while native FRE is up keeps
  // TimeToActionable recorded while still waiting for the guest frame.
  metrics_.OnGuestReset();
  EXPECT_TRUE(metrics_.IsWaitingForActionable());

  // Guest ready and frame arrive in the background while the native FRE card is
  // up, without logging a second PanelPresentationTime or TimeToActionable.
  task_environment_.FastForwardBy(base::Milliseconds(150));
  metrics_.OnClientReady();
  metrics_.OnGuestFrameSubmitted();
  EXPECT_FALSE(metrics_.IsWaitingForActionable());
  histogram_tester_.ExpectTotalCount(
      "Glic.Instance.PanelPresentationTime.SidePanel", 1);
  histogram_tester_.ExpectTotalCount("Glic.Instance.TimeToActionable.SidePanel",
                                     1);

  // Hiding/closing and reopening before accept preserves the guest frame state
  // for the same guest document without double-recording TimeToActionable.
  metrics_.OnVisibilityChanged(false);
  metrics_.OnClose();
  metrics_.OnWillShow(show_options, /*has_conversation_id=*/false);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnOpen(mojom::InvocationSource::kFre, show_options);

  task_environment_.FastForwardBy(base::Milliseconds(250));
  metrics_.OnNativeFreAccepted();

  histogram_tester_.ExpectUniqueSample("Glic.Fre.AcceptToChatActionable", 0, 1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Instance.TimeToActionable.SidePanel", 0, 1);
}

TEST_F(GlicInstanceMetricsTest,
       ColdNativeAndWebFreAccept_RecordsOnceGuestFrameSubmits) {
  ShowOptions show_options = MakeNativeFreShowOptions();

  // Cold native FRE: accept completes before guest frame submits.
  metrics_.OnWillShow(show_options, /*has_conversation_id=*/false);
  metrics_.OnVisibilityChanged(true);
  metrics_.OnOpen(mojom::InvocationSource::kFre, show_options);
  metrics_.OnNativeFreAccepted();
  histogram_tester_.ExpectTotalCount("Glic.Fre.AcceptToChatActionable", 0);

  task_environment_.FastForwardBy(base::Milliseconds(500));
  metrics_.OnGuestFrameSubmitted();
  histogram_tester_.ExpectUniqueSample("Glic.Fre.AcceptToChatActionable", 500,
                                       1);

  // Control arm (WebUI FRE): duplicate OnWebFreAccepted does not overwrite.
  metrics_.OnClose();
  metrics_.OnGuestReset();
  metrics_.OnOpen(mojom::InvocationSource::kFre,
                  ShowOptions::ForSidePanel(mock_tab_));
  metrics_.OnVisibilityChanged(true);
  metrics_.OnClientReady();
  metrics_.OnWebFreAccepted();
  task_environment_.FastForwardBy(base::Milliseconds(100));
  metrics_.OnWebFreAccepted();
  task_environment_.FastForwardBy(base::Milliseconds(150));
  metrics_.OnGuestFrameSubmitted();

  histogram_tester_.ExpectBucketCount("Glic.Fre.AcceptToChatActionable", 250,
                                      1);
  histogram_tester_.ExpectTotalCount("Glic.Fre.AcceptToChatActionable", 2);
}

}  // namespace glic
