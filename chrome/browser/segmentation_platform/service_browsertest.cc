// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/segmentation_platform/ukm_data_manager_test_utils.h"
#include "chrome/browser/segmentation_platform/ukm_database_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_observer.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/internal/execution/mock_model_provider.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/ukm/ukm_service.h"
#include "content/public/test/browser_test.h"

namespace segmentation_platform {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;

constexpr SegmentId kSegmentId =
    SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT;

constexpr char kSqlFeatureQuery[] = "SELECT COUNT(*) from metrics";

class SegmentationPlatformTest : public InProcessBrowserTest {
 public:
  SegmentationPlatformTest() {
    // Low Engagement Segment is used to test segmentation service without multi
    // output. Search User Segment supports  multi output path.
    feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(features::kSegmentationPlatformFeature,
                                         {}),
         base::test::FeatureRefAndParams(
             features::kSegmentationPlatformUkmEngine, {}),
         base::test::FeatureRefAndParams(
             features::kSegmentationPlatformLowEngagementFeature,
             {{"enable_default_model", "true"}}),
         base::test::FeatureRefAndParams(
             features::kSegmentationPlatformSearchUser,
             {{"enable_default_model", "true"}})},
        {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("segmentation-platform-refresh-results");
  }

  bool HasResultPref(base::StringPiece segmentation_key) {
    const base::Value::Dict& dictionary =
        browser()->profile()->GetPrefs()->GetDict(kSegmentationResultPref);
    return !!dictionary.FindByDottedPath(segmentation_key);
  }

  void OnResultPrefUpdated() {
    if (!wait_for_pref_callback_.is_null() &&
        HasResultPref(kChromeLowUserEngagementSegmentationKey)) {
      std::move(wait_for_pref_callback_).Run();
    }
  }

  void WaitForPrefUpdate() {
    if (HasResultPref(kChromeLowUserEngagementSegmentationKey))
      return;

    base::RunLoop wait_for_pref;
    wait_for_pref_callback_ = wait_for_pref.QuitClosure();
    pref_registrar_.Init(browser()->profile()->GetPrefs());
    pref_registrar_.Add(
        kSegmentationResultPref,
        base::BindRepeating(&SegmentationPlatformTest::OnResultPrefUpdated,
                            weak_ptr_factory_.GetWeakPtr()));
    wait_for_pref.Run();

    pref_registrar_.RemoveAll();
  }

  bool HasClientResultPref(const std::string& segmentation_key) {
    PrefService* pref_service = browser()->profile()->GetPrefs();
    std::unique_ptr<ClientResultPrefs> result_prefs_ =
        std::make_unique<ClientResultPrefs>(pref_service);
    return result_prefs_->ReadClientResultFromPrefs(segmentation_key)
        .has_value();
  }

  void OnClientResultPrefUpdated() {
    if (!wait_for_pref_callback_.is_null() &&
        HasClientResultPref(kSearchUserKey)) {
      std::move(wait_for_pref_callback_).Run();
    }
  }

  void WaitForClientResultPrefUpdate() {
    if (HasClientResultPref(kSearchUserKey)) {
      return;
    }

    base::RunLoop wait_for_pref;
    wait_for_pref_callback_ = wait_for_pref.QuitClosure();
    pref_registrar_.Init(browser()->profile()->GetPrefs());
    pref_registrar_.Add(
        kSegmentationClientResultPrefs,
        base::BindRepeating(
            &SegmentationPlatformTest::OnClientResultPrefUpdated,
            weak_ptr_factory_.GetWeakPtr()));
    wait_for_pref.Run();

    pref_registrar_.RemoveAll();
  }

  void WaitForPlatformInit() {
    base::RunLoop wait_for_init;
    SegmentationPlatformService* service = segmentation_platform::
        SegmentationPlatformServiceFactory::GetForProfile(browser()->profile());
    while (!service->IsPlatformInitialized()) {
      wait_for_init.RunUntilIdle();
    }
  }

  void ExpectSegmentSelectionResult(const std::string& segmentation_key,
                                    bool result_expected) {
    SegmentationPlatformService* service = segmentation_platform::
        SegmentationPlatformServiceFactory::GetForProfile(browser()->profile());
    base::RunLoop wait_for_segment;
    service->GetSelectedSegment(
        segmentation_key, base::BindOnce(
                              [](bool result_expected, base::OnceClosure quit,
                                 const SegmentSelectionResult& result) {
                                EXPECT_EQ(result_expected, result.is_ready);
                                std::move(quit).Run();
                              },
                              result_expected, wait_for_segment.QuitClosure()));
    wait_for_segment.Run();
  }

  void ExpectClassificationResult(const std::string& segmentation_key,
                                  PredictionStatus expected_prediction_status) {
    SegmentationPlatformService* service = segmentation_platform::
        SegmentationPlatformServiceFactory::GetForProfile(browser()->profile());
    PredictionOptions options;
    options.on_demand_execution = true;
    base::RunLoop wait_for_segment;
    service->GetClassificationResult(
        segmentation_key, options, nullptr,
        base::BindOnce(&SegmentationPlatformTest::OnGetClassificationResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       wait_for_segment.QuitClosure(),
                       expected_prediction_status));
    wait_for_segment.Run();
  }

  void OnGetClassificationResult(base::RepeatingClosure closure,
                                 PredictionStatus expected_prediction_status,
                                 const ClassificationResult& actual) {
    EXPECT_EQ(expected_prediction_status, actual.status);
    EXPECT_TRUE(actual.ordered_labels.size() > 0);
    std::move(closure).Run();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  PrefChangeRegistrar pref_registrar_;
  base::OnceClosure wait_for_pref_callback_;
  base::WeakPtrFactory<SegmentationPlatformTest> weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(SegmentationPlatformTest, RunDefaultModel) {
  WaitForPlatformInit();
  WaitForPrefUpdate();

  // Result is available from previous session's selection.
  ExpectSegmentSelectionResult(kChromeLowUserEngagementSegmentationKey,
                               /*result_expected=*/true);

  // This session runs default model and updates again.
  WaitForPrefUpdate();
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformTest,
                       PRE_OnDemandFlowForClassificationModel) {
  WaitForPlatformInit();
  WaitForClientResultPrefUpdate();
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformTest,
                       OnDemandFlowForClassificationModel) {
  WaitForPlatformInit();
  // Result is available from previous session's prefs.
  ExpectClassificationResult(
      kSearchUserKey,
      /*expected_prediction_status=*/PredictionStatus::kSucceeded);
}

class SegmentationPlatformUkmModelTest : public SegmentationPlatformTest {
 public:
  SegmentationPlatformUkmModelTest() : utils_(&ukm_recorder_) {}

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(parts);
    utils_.PreProfileInit(
        {{kSegmentId, utils_.GetSamplePageLoadMetadata(kSqlFeatureQuery)}});

    MockDefaultModelProvider* provider = utils_.GetDefaultOverride(kSegmentId);
    EXPECT_CALL(*provider, ExecuteModelWithInput(_, _))
        .WillRepeatedly(Invoke([&](const ModelProvider::Request& inputs,
                                   ModelProvider::ExecutionCallback callback) {
          input_feature_in_last_execution_ = inputs;
          std::move(callback).Run(ModelProvider::Response(1, 0.5));
        }));
  }

  void PreRunTestOnMainThread() override {
    SegmentationPlatformTest::PreRunTestOnMainThread();
    utils_.set_history_service(HistoryServiceFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS));
  }

 protected:
  ukm::TestUkmRecorder ukm_recorder_;
  UkmDataManagerTestUtils utils_;
  absl::optional<ModelProvider::Request> input_feature_in_last_execution_;
};

// This test is disabled in CrOS because CrOS creates a signin profile that uses
// incognito mode. This disables the segmentation platform data collection.
// TODO(ssid): Fix this test for CrOS by waiting for signin profile to be
// deleted at startup before adding metrics.
// https://crbug.com/1467530 -- Flaky on Mac
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_PRE_RunUkmBasedModel DISABLED_PRE_RunUkmBasedModel
#define MAYBE_RunUkmBasedModel DISABLED_RunUkmBasedModel
#else
#define MAYBE_PRE_RunUkmBasedModel PRE_RunUkmBasedModel
#define MAYBE_RunUkmBasedModel RunUkmBasedModel
#endif

IN_PROC_BROWSER_TEST_F(SegmentationPlatformUkmModelTest,
                       MAYBE_PRE_RunUkmBasedModel) {
  const GURL kUrl1("https://www.url1.com");

  WaitForPlatformInit();

  utils_.WaitForUkmObserverRegistration();

  // Wait for the default model to run and save results to prefs.
  WaitForPrefUpdate();

  // Record page load UKM that should be recorded in the database, persisted
  // across sessions.
  utils_.RecordPageLoadUkm(kUrl1, base::Time::Now());
  while (!utils_.IsUrlInDatabase(kUrl1)) {
    base::RunLoop().RunUntilIdle();
  }
  // There are no UKM metrics written to the database, count = 0.
  EXPECT_EQ(ModelProvider::Request({0}), input_feature_in_last_execution_);
}

IN_PROC_BROWSER_TEST_F(SegmentationPlatformUkmModelTest,
                       MAYBE_RunUkmBasedModel) {
  const GURL kUrl1("https://www.url1.com");

  WaitForPlatformInit();

  // Verify that the URL recorded in last session is still in database.
  EXPECT_TRUE(utils_.IsUrlInDatabase(kUrl1));

  // Result is available from previous session's selection.
  ExpectSegmentSelectionResult(kChromeLowUserEngagementSegmentationKey,
                               /*result_expected=*/true);

  utils_.WaitForUkmObserverRegistration();
  WaitForPrefUpdate();

  // There are 2 UKM metrics written to the database, count = 2.
  EXPECT_EQ(ModelProvider::Request({2}), input_feature_in_last_execution_);
}

}  // namespace segmentation_platform
