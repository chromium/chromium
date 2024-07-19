// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_panel_manager.h"

#include <optional>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_context.h"
#include "chrome/browser/ash/input_method/editor_geolocation_mock_provider.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

constexpr std::string_view kAllowedCountryCode = "au";

class FakeContextObserver : public EditorContext::Observer {
 public:
  FakeContextObserver() = default;
  ~FakeContextObserver() override = default;

  // EditorContext::Observer overrides
  void OnContextUpdated() override {}
  void OnImeChange(std::string_view engine_id) override {}
};

class FakeSystem : public EditorContext::System {
 public:
  FakeSystem() = default;
  ~FakeSystem() override = default;

  // EditorContext::System overrides
  std::optional<ukm::SourceId> GetUkmSourceId() override {
    return std::nullopt;
  }
};

class FakeEditorClient : public orca::mojom::EditorClient {
 public:
  FakeEditorClient() = default;
  ~FakeEditorClient() override = default;
  void GetPresetTextQueries(GetPresetTextQueriesCallback callback) override {
    std::move(callback).Run({});
  }
  void RequestPresetRewrite(const std::string& text_query_id,
                            const std::optional<std::string>& text_override,
                            RequestPresetRewriteCallback callback) override {}
  void RequestFreeformRewrite(
      const std::string& input,
      const std::optional<std::string>& text_override,
      RequestFreeformRewriteCallback callback) override {}
  void RequestFreeformWrite(const std::string& input,
                            RequestFreeformWriteCallback callback) override {}
  void InsertText(const std::string& text) override {}
  void ApproveConsent() override {}
  void DeclineConsent() override {}
  void DismissConsent() override {}
  void OpenUrlInNewWindow(const GURL& url) override {}
  void ShowUI() override {}
  void CloseUI() override {}
  void AppendText(const std::string& text) override {}
  void PreviewFeedback(const std::string& result_id,
                       PreviewFeedbackCallback callback) override {}
  void SubmitFeedback(const std::string& result_id,
                      const std::string& user_description) override {}
  void OnTrigger(orca::mojom::TriggerContextPtr trigger_context) override {}
  void EmitMetricEvent(orca::mojom::MetricEvent metric_event) override {}
};

class EditorPanelManagerDelegateForTesting
    : public EditorPanelManager::Delegate {
 public:
  EditorPanelManagerDelegateForTesting(
      EditorOpportunityMode opportunity_mode,
      ConsentStatus consent_status,
      const std::vector<EditorBlockedReason>& blocked_reasons)
      : opportunity_mode_(opportunity_mode),
        consent_status_(consent_status),
        blocked_reasons_(blocked_reasons),
        geolocation_provider_(kAllowedCountryCode),
        context_(&context_observer_, &system_, &geolocation_provider_),
        metrics_recorder_(&context_, opportunity_mode) {}
  void BindEditorClient(mojo::PendingReceiver<orca::mojom::EditorClient>
                            pending_receiver) override {}
  void OnPromoCardDeclined() override {}
  void ProcessConsentAction(ConsentAction consent_action) override {}
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

  EditorMode GetEditorMode() const override { return EditorMode::kSoftBlocked; }

  ConsentStatus GetConsentStatus() const override { return consent_status_; }

 private:
  EditorOpportunityMode opportunity_mode_;
  ConsentStatus consent_status_;
  std::vector<EditorBlockedReason> blocked_reasons_;
  FakeSystem system_;
  FakeContextObserver context_observer_;
  EditorGeolocationMockProvider geolocation_provider_;
  EditorContext context_;
  EditorMetricsRecorder metrics_recorder_;
};

class EditorPanelManagerTest : public testing::Test {
 public:
  EditorPanelManagerTest() = default;
  ~EditorPanelManagerTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(EditorPanelManagerTest,
       EditorPanelContextCallbackShouldReturnConsentStatusSettled) {
  EditorPanelManagerDelegateForTesting editor_panel_manager_delegate(
      EditorOpportunityMode::kWrite, ConsentStatus::kApproved, {});
  EditorPanelManager manager(&editor_panel_manager_delegate);
  FakeEditorClient fake_editor_client;

  mojo::Receiver<orca::mojom::EditorClient> receiver{&fake_editor_client};
  manager.SetEditorClientForTesting(receiver.BindNewPipeAndPassRemote());

  base::test::TestFuture<crosapi::mojom::EditorPanelContextPtr> future;
  manager.GetEditorPanelContext(future.GetCallback());

  crosapi::mojom::EditorPanelContextPtr expected =
      crosapi::mojom::EditorPanelContext::New();
  expected->editor_panel_mode = crosapi::mojom::EditorPanelMode::kSoftBlocked;
  expected->consent_status_settled = true;

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), expected);
}

TEST_F(EditorPanelManagerTest,
       GetEditorPanelContextCallbackShouldNotReturnConsentStatusSettled) {
  EditorPanelManagerDelegateForTesting editor_panel_manager_delegate(
      EditorOpportunityMode::kWrite, ConsentStatus::kUnset, {});
  EditorPanelManager manager(&editor_panel_manager_delegate);
  FakeEditorClient fake_editor_client;

  mojo::Receiver<orca::mojom::EditorClient> receiver{&fake_editor_client};
  manager.SetEditorClientForTesting(receiver.BindNewPipeAndPassRemote());

  base::test::TestFuture<crosapi::mojom::EditorPanelContextPtr> future;
  manager.GetEditorPanelContext(future.GetCallback());

  crosapi::mojom::EditorPanelContextPtr expected =
      crosapi::mojom::EditorPanelContext::New();
  expected->editor_panel_mode = crosapi::mojom::EditorPanelMode::kSoftBlocked;
  expected->consent_status_settled = false;

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), expected);
}

TEST_F(EditorPanelManagerTest, LogMetricsInWriteMode) {
  EditorPanelManagerDelegateForTesting editor_panel_manager_delegate(
      EditorOpportunityMode::kWrite, ConsentStatus::kApproved, {});
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
      EditorOpportunityMode::kRewrite, ConsentStatus::kApproved, {});
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
      EditorOpportunityMode::kWrite, ConsentStatus::kDeclined,
      {
          EditorBlockedReason::kBlockedByConsent,
          EditorBlockedReason::kBlockedByInvalidFormFactor,
          EditorBlockedReason::kBlockedByNetworkStatus,
          EditorBlockedReason::kBlockedByTextLength,
          EditorBlockedReason::kBlockedByUrl,
      });
  EditorPanelManager manager(&editor_panel_manager_delegate);
  base::HistogramTester histogram_tester;

  manager.LogEditorMode(crosapi::mojom::EditorPanelMode::kSoftBlocked);

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
      EditorOpportunityMode::kRewrite, ConsentStatus::kApproved,
      {
          EditorBlockedReason::kBlockedByApp,
          EditorBlockedReason::kBlockedByInputMethod,
          EditorBlockedReason::kBlockedBySetting,
      });
  EditorPanelManager manager(&editor_panel_manager_delegate);
  base::HistogramTester histogram_tester;

  manager.LogEditorMode(crosapi::mojom::EditorPanelMode::kSoftBlocked);

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
      EditorOpportunityMode::kRewrite, ConsentStatus::kUnset, {});
  EditorPanelManager manager(&editor_panel_manager_delegate);
  base::HistogramTester histogram_tester;

  manager.OnPromoCardDeclined();

  histogram_tester.ExpectUniqueSample("InputMethod.Manta.Orca.States.Rewrite",
                                      EditorStates::kPromoCardExplicitDismissal,
                                      1);
}

TEST_F(EditorPanelManagerTest, LogMetricWhenPromoCardIsShown) {
  EditorPanelManagerDelegateForTesting editor_panel_manager_delegate(
      EditorOpportunityMode::kWrite, ConsentStatus::kUnset, {});
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
