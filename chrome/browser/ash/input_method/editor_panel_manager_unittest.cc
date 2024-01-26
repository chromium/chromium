// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_panel_manager.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

class EditorPanelManagerDelegateForTesting
    : public EditorPanelManager::Delegate {
 public:
  EditorPanelManagerDelegateForTesting(
      EditorOpportunityMode opportunity_mode,
      const std::vector<EditorBlockedReason>& blocked_reasons)
      : opportunity_mode_(opportunity_mode),
        blocked_reasons_(blocked_reasons),
        metrics_recorder_(opportunity_mode) {}
  void BindEditorClient(mojo::PendingReceiver<orca::mojom::EditorClient>
                            pending_receiver) override {}
  void OnPromoCardDeclined() override {}
  void HandleTrigger(std::optional<std::string_view> preset_query_id,
                     std::optional<std::string_view> freeform_text) override {}
  EditorOpportunityMode GetEditorOpportunityMode() const override {
    return opportunity_mode_;
  }
  std::vector<EditorBlockedReason> GetBlockedReasons() const override {
    return blocked_reasons_;
  }
  void CacheContext() override {}
  EditorMetricsRecorder* GetMetricsRecorder() override {
    return &metrics_recorder_;
  }
  // not used.
  EditorMode GetEditorMode() const override { return EditorMode::kBlocked; }

 private:
  EditorOpportunityMode opportunity_mode_;
  std::vector<EditorBlockedReason> blocked_reasons_;
  EditorMetricsRecorder metrics_recorder_;
};

class EditorPanelManagerTest : public testing::Test {
 public:
  EditorPanelManagerTest() = default;
  ~EditorPanelManagerTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(EditorPanelManagerTest, LogMetricsInWriteMode) {
  EditorPanelManagerDelegateForTesting editor_panel_manager_delegate(
      EditorOpportunityMode::kWrite, {});
  EditorPanelManager manager(&editor_panel_manager_delegate);
  base::HistogramTester histogram_tester;

  manager.LogEditorMode(crosapi::mojom::EditorPanelMode::kWrite);

  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kNativeUIShowOpportunity, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kNativeUIShown, 1);
  histogram_tester.ExpectTotalCount("InputMethod.Manta.Orca.States.Write", 2);
}

TEST_F(EditorPanelManagerTest, LogMetricsInRewriteMode) {
  EditorPanelManagerDelegateForTesting editor_panel_manager_delegate(
      EditorOpportunityMode::kRewrite, {});
  EditorPanelManager manager(&editor_panel_manager_delegate);
  base::HistogramTester histogram_tester;

  manager.LogEditorMode(crosapi::mojom::EditorPanelMode::kRewrite);

  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Rewrite",
                                     EditorStates::kNativeUIShowOpportunity, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Rewrite",
                                     EditorStates::kNativeUIShown, 1);
  histogram_tester.ExpectTotalCount("InputMethod.Manta.Orca.States.Rewrite", 2);
}

TEST_F(EditorPanelManagerTest, LogMetricsInBlockedWriteMode) {
  EditorPanelManagerDelegateForTesting editor_panel_manager_delegate(
      EditorOpportunityMode::kWrite,
      {
          EditorBlockedReason::kBlockedByConsent,
          EditorBlockedReason::kBlockedByInvalidFormFactor,
          EditorBlockedReason::kBlockedByNetworkStatus,
          EditorBlockedReason::kBlockedByTextLength,
          EditorBlockedReason::kBlockedByUrl,
      });
  EditorPanelManager manager(&editor_panel_manager_delegate);
  base::HistogramTester histogram_tester;

  manager.LogEditorMode(crosapi::mojom::EditorPanelMode::kBlocked);

  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kNativeUIShowOpportunity, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kNativeUIShown, 0);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kBlocked, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kBlockedByConsent, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kBlockedByInvalidFormFactor,
                                     1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kBlockedByNetworkStatus, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kBlockedByTextLength, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kBlockedByUrl, 1);
  histogram_tester.ExpectTotalCount("InputMethod.Manta.Orca.States.Write", 7);
}

TEST_F(EditorPanelManagerTest, LogMetricsInBlockedMode) {
  EditorPanelManagerDelegateForTesting editor_panel_manager_delegate(
      EditorOpportunityMode::kRewrite,
      {
          EditorBlockedReason::kBlockedByApp,
          EditorBlockedReason::kBlockedByInputMethod,
          EditorBlockedReason::kBlockedBySetting,
      });
  EditorPanelManager manager(&editor_panel_manager_delegate);
  base::HistogramTester histogram_tester;

  manager.LogEditorMode(crosapi::mojom::EditorPanelMode::kBlocked);

  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Rewrite",
                                     EditorStates::kNativeUIShowOpportunity, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Rewrite",
                                     EditorStates::kNativeUIShown, 0);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Rewrite",
                                     EditorStates::kBlocked, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Rewrite",
                                     EditorStates::kBlockedByApp, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Rewrite",
                                     EditorStates::kBlockedByInputMethod, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Rewrite",
                                     EditorStates::kBlockedBySetting, 1);
  histogram_tester.ExpectTotalCount("InputMethod.Manta.Orca.States.Rewrite", 5);
}

TEST_F(EditorPanelManagerTest, LogMetricWhenPromoCardIsExplicitlyDismissed) {
  EditorPanelManagerDelegateForTesting editor_panel_manager_delegate(
      EditorOpportunityMode::kRewrite, {});
  EditorPanelManager manager(&editor_panel_manager_delegate);
  base::HistogramTester histogram_tester;

  manager.OnPromoCardDeclined();

  histogram_tester.ExpectUniqueSample("InputMethod.Manta.Orca.States.Rewrite",
                                      EditorStates::kPromoCardExplicitDismissal,
                                      1);
}

TEST_F(EditorPanelManagerTest, LogMetricWhenPromoCardIsShown) {
  EditorPanelManagerDelegateForTesting editor_panel_manager_delegate(
      EditorOpportunityMode::kWrite, {});
  EditorPanelManager manager(&editor_panel_manager_delegate);
  base::HistogramTester histogram_tester;

  manager.LogEditorMode(crosapi::mojom::EditorPanelMode::kPromoCard);

  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kPromoCardImpression, 1);
  histogram_tester.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                     EditorStates::kNativeUIShowOpportunity, 1);
  histogram_tester.ExpectTotalCount("InputMethod.Manta.Orca.States.Write", 2);
}

}  // namespace
}  // namespace ash::input_method
