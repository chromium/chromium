// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cstdint>
#include <iosfwd>
#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "chrome/browser/privacy_budget/privacy_budget_browsertest_util.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "chrome/common/privacy_budget/types.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_recorder_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "url/gurl.h"

class Profile;

namespace ukm {
class UkmService;
}  // namespace ukm

namespace {

using testing::_;
using testing::AllOf;
using testing::Contains;
using testing::Each;
using testing::Field;
using testing::IsSupersetOf;
using testing::Key;
using testing::Pair;
using testing::UnorderedElementsAreArray;

class EnableRandomSampling {
 public:
  EnableRandomSampling()
      : privacy_budget_config_(
            test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling) {}

 private:
  test::ScopedPrivacyBudgetConfig privacy_budget_config_;
};

class PrivacyBudgetBrowserTestEnableRandomSampling
    : private EnableRandomSampling,
      public PlatformBrowserTest {};

class PrivacyBudgetBrowserTestWithTestRecorder
    : private EnableRandomSampling,
      public PrivacyBudgetBrowserTestBaseWithTestRecorder {};

}  // namespace

IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTestEnableRandomSampling,
                       BrowserSideSettingsIsActive) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));
  const auto* settings = blink::IdentifiabilityStudySettings::Get();
  EXPECT_TRUE(settings->IsActive());
}

namespace {

using PrivacyBudgetBrowserTestForWorkersClientAdded =
    PrivacyBudgetBrowserTestWithTestRecorder;
}  // namespace

IN_PROC_BROWSER_TEST_P(PrivacyBudgetBrowserTestForWorkersClientAdded,
                       WorkersRecordWorkerClientAddedMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::DOMMessageQueue messages(web_contents());
  base::RunLoop run_loop;

  std::vector<uint64_t> expected_keys = {
      blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal,
          blink::IdentifiableSurface::ReservedSurfaceMetrics::
              kWorkerClientAdded_ClientSourceId)
          .ToUkmMetricHash(),
      blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal,
          blink::IdentifiableSurface::ReservedSurfaceMetrics::
              kWorkerClientAdded_WorkerType)
          .ToUkmMetricHash(),
  };

  // We wait for the expected metrics to be reported. Since some of the
  // metrics are reported from the renderer process, this is the only reliable
  // way to be sure we waited long enough.
  auto quit_run_loop = [this, &expected_keys, &run_loop]() {
    if (GetReportedSurfaceKeys(expected_keys).size() == expected_keys.size())
      run_loop.Quit();
  };

  recorder().SetOnAddEntryCallback(ukm::builders::Identifiability::kEntryName,
                                   base::BindLambdaForTesting(quit_run_loop));

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(FilePathXYZ() + ".html")));

  // The document calls a bunch of instrumented functions and sends a message
  // back to the test. Receipt of the message indicates that the script
  // successfully completed.
  std::string done;
  ASSERT_TRUE(messages.WaitForMessage(&done));

  // Wait for the metrics to come down the pipe.
  run_loop.Run();

  // The previously registered callback will be invalid after the test class is
  // destructed.
  recorder().SetOnAddEntryCallback(ukm::builders::Identifiability::kEntryName,
                                   {});

  // Test succeeds if there is no timeout. However, let's recheck the metrics
  // here, so that if there is a timeout we get an output of which metrics are
  // missing.
  EXPECT_THAT(GetReportedSurfaceKeys(expected_keys),
              UnorderedElementsAreArray(expected_keys));
}

IN_PROC_BROWSER_TEST_P(PrivacyBudgetBrowserTestForWorkersClientAdded,
                       ReportWorkerClientAddedMetricForEveryRegisteredClient) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::DOMMessageQueue messages(web_contents());
  base::RunLoop run_loop;

  uint64_t expected_key =
      blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal,
          blink::IdentifiableSurface::ReservedSurfaceMetrics::
              kWorkerClientAdded_ClientSourceId)
          .ToUkmMetricHash();

  // We wait for the expected metrics to be reported. Since some of the
  // metrics are reported from the renderer process, this is the only reliable
  // way to be sure we waited long enough.
  auto quit_run_loop = [this, &expected_key, &run_loop]() {
    if (GetSurfaceKeyCount(expected_key) == 2)
      run_loop.Quit();
  };

  recorder().SetOnAddEntryCallback(ukm::builders::Identifiability::kEntryName,
                                   base::BindLambdaForTesting(quit_run_loop));

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          FilePathXYZ() + "_with_two_clients.html")));

  // The document calls a bunch of instrumented functions and sends a message
  // back to the test. Receipt of the message indicates that the script
  // successfully completed.
  std::string done;
  ASSERT_TRUE(messages.WaitForMessage(&done));

  // Wait for the metrics to come down the pipe.
  run_loop.Run();

  // The previously registered callback will be invalid after the test class is
  // destructed.
  recorder().SetOnAddEntryCallback(ukm::builders::Identifiability::kEntryName,
                                   {});

  // Test succeeds if there is no timeout.
  // Both surfaces should come from the same source but have different client
  // ids.
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>> entries =
      recorder().GetEntriesByName(ukm::builders::Identifiability::kEntryName);

  base::flat_set<uint64_t> source_ids;
  base::flat_set<uint64_t> client_source_ids;
  for (const ukm::mojom::UkmEntry* entry : entries) {
    for (const auto& metric : entry->metrics) {
      if (metric.first == expected_key) {
        source_ids.insert(entry->source_id);
        client_source_ids.insert(metric.second);
      }
    }
  }
  EXPECT_EQ(source_ids.size(), 1u);
  EXPECT_EQ(client_source_ids.size(), 2u);
}

INSTANTIATE_TEST_SUITE_P(
    PrivacyBudgetBrowserTestForWorkersClientAddedParameterized,
    PrivacyBudgetBrowserTestForWorkersClientAdded,
    ::testing::Values(
// Shared workers are not supported on Android.
#if !BUILDFLAG(IS_ANDROID)
        "/privacy_budget/calls_shared_worker",
#endif
        "/privacy_budget/calls_service_worker"));

namespace {

// Test class that allows to enable UKM recording.
class PrivacyBudgetBrowserTestWithUkmRecording
    : private EnableRandomSampling,
      public PrivacyBudgetBrowserTestBaseWithUkmRecording {};

}  // namespace

// When UKM resets the Client ID for some reason the study should reset its
// local state as well.
IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTestWithUkmRecording,
                       UkmClientIdChangesResetStudyState) {
  EXPECT_TRUE(blink::IdentifiabilityStudySettings::Get()->IsActive());
  ASSERT_TRUE(EnableUkmRecording());

  local_state()->SetString(prefs::kPrivacyBudgetSeenSurfaces, "1,2,3");

  ASSERT_TRUE(DisableUkmRecording());

  EXPECT_TRUE(
      local_state()->GetString(prefs::kPrivacyBudgetSeenSurfaces).empty())
      << "Active surface list still exists after resetting client ID";
}

IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTestWithUkmRecording,
                       IncludesMetadata) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));
  ASSERT_TRUE(EnableUkmRecording());

  constexpr blink::IdentifiableToken kDummyToken = 1;
  constexpr blink::IdentifiableSurface kDummySurface =
      blink::IdentifiableSurface::FromMetricHash(2125235);
  auto* ukm_recorder = ukm::UkmRecorder::Get();

  blink::IdentifiabilityMetricBuilder(ukm::UkmRecorder::GetNewSourceID())
      .Add(kDummySurface, kDummyToken)
      .Record(ukm_recorder);

  blink::IdentifiabilitySampleCollector::Get()->Flush(ukm_recorder);

  ukm::UkmTestHelper ukm_test_helper(ukm_service());
  ukm_test_helper.BuildAndStoreLog();
  std::unique_ptr<ukm::Report> ukm_report = ukm_test_helper.GetUkmReport();
  ASSERT_TRUE(ukm_test_helper.HasUnsentLogs());
  ASSERT_TRUE(ukm_report);
  ASSERT_NE(ukm_report->entries_size(), 0);

  std::map<uint64_t, int64_t> seen_metrics;
  for (const auto& entry : ukm_report->entries()) {
    ASSERT_TRUE(entry.has_event_hash());
    if (entry.event_hash() != ukm::builders::Identifiability::kEntryNameHash) {
      continue;
    }
    for (const auto& metric : entry.metrics()) {
      ASSERT_TRUE(metric.has_metric_hash());
      ASSERT_TRUE(metric.has_value());
      seen_metrics.insert({metric.metric_hash(), metric.value()});
    }
  }

  const std::pair<uint64_t, int64_t> kExpectedGenerationEntry{
      ukm::builders::Identifiability::kStudyGeneration_626NameHash,
      test::ScopedPrivacyBudgetConfig::kDefaultGeneration};
  EXPECT_THAT(seen_metrics, testing::Contains(kExpectedGenerationEntry));

  const std::pair<uint64_t, int64_t> kExpectedGeneratorEntry{
      ukm::builders::Identifiability::kGeneratorVersion_926NameHash,
      IdentifiabilityStudyState::kGeneratorVersion};
  EXPECT_THAT(seen_metrics, testing::Contains(kExpectedGeneratorEntry));
}

namespace {

class PrivacyBudgetGroupConfigBrowserTest : public PlatformBrowserTest {
 public:
  PrivacyBudgetGroupConfigBrowserTest() {
    test::ScopedPrivacyBudgetConfig::Parameters parameters;

    constexpr auto kSurfacesPerGroup = 40;
    constexpr auto kGroupCount = 200;

    auto counter = 0;
    for (auto i = 0; i < kGroupCount; ++i) {
      parameters.blocks.emplace_back();
      auto& group = parameters.blocks.back();
      group.reserve(kSurfacesPerGroup);
      for (auto j = 0; j < kSurfacesPerGroup; ++j) {
        group.push_back(blink::IdentifiableSurface::FromTypeAndToken(
            blink::IdentifiableSurface::Type::kNavigator_GetUserMedia,
            ++counter));
      }
    }

    scoped_config_.Apply(parameters);
  }

 private:
  test::ScopedPrivacyBudgetConfig scoped_config_;
};

}  // namespace

// TODO(https://crbug.com/385000599): Flaky on linux-chromeos-rel builder.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_LoadsAGroup DISABLED_LoadsAGroup
#else
#define MAYBE_LoadsAGroup LoadsAGroup
#endif
IN_PROC_BROWSER_TEST_F(PrivacyBudgetGroupConfigBrowserTest, MAYBE_LoadsAGroup) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));

  const auto* settings = blink::IdentifiabilityStudySettings::Get();
  ASSERT_TRUE(settings->IsActive());
}

namespace {

class PrivacyBudgetAssignedBlockSamplingConfigTest
    : public PlatformBrowserTest {
 public:
  PrivacyBudgetAssignedBlockSamplingConfigTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kIdentifiabilityStudy,
          {{features::kIdentifiabilityStudyBlockedMetrics.name, "44033,44289"},
           {features::kIdentifiabilityStudyBlockedTypes.name, "11,25,28"},
           {features::kIdentifiabilityStudyBlockWeights.name,
            "5202,37515,34582"},
           {features::kIdentifiabilityStudyBlocks.name,
            // Define three blocks of surfaces.
            "9129224;865032;8710152;8678920;9305096,"
            "1722309467823238416;3972031034286914064,"
            "3873813933275956760;7532279523433960728;13014994009983628312,"},
           {features::kIdentifiabilityStudyGeneration.name, "7"}}}},
        /*disabled_features=*/{features::kIdentifiabilityStudyMetaExperiment});
  }

  static constexpr auto kBlockedSurface =
      blink::IdentifiableSurface::FromMetricHash(44033);

  // This surface is not mentioned above and is not blocked by default. It
  // should be considered allowed, but its metrics will not be recorded because
  // it is not one of the active surfaces.
  static constexpr auto kAllowedInactiveSurface =
      blink::IdentifiableSurface::FromMetricHash(44290);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

// This test checks that the Identifiability Study configuration is picked up
// correctly from the field trial parameters.
IN_PROC_BROWSER_TEST_F(PrivacyBudgetAssignedBlockSamplingConfigTest,
                       LoadsSettingsFromFieldTrialParameters) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));

  const auto* settings = blink::IdentifiabilityStudySettings::Get();
  EXPECT_TRUE(settings->IsActive());
  // Allowed by default.
  EXPECT_TRUE(settings->ShouldSampleType(
      blink::IdentifiableSurface::Type::kCanvasReadback));

  // Blocked surfaces.
  EXPECT_FALSE(settings->ShouldSampleSurface(kBlockedSurface));

  // Some random surface that shouldn't be blocked.
  EXPECT_TRUE(settings->ShouldSampleSurface(kAllowedInactiveSurface));

  // Blocked types
  EXPECT_FALSE(settings->ShouldSampleType(
      blink::IdentifiableSurface::Type::kHTMLMediaElement_CanPlayType));
  EXPECT_FALSE(settings->ShouldSampleType(
      blink::IdentifiableSurface::Type::kMediaCapabilities_DecodingInfo));
}

namespace {

class EnableMetaExperiment {
 public:
  EnableMetaExperiment() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kIdentifiabilityStudyMetaExperiment,
          {{features::kIdentifiabilityStudyMetaExperimentActivationProbability
                .name,
            "1"}}}},
        {features::kIdentifiabilityStudy});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class PrivacyBudgetMetaExperimentBrowserTestWithUkmRecording
    : private EnableMetaExperiment,
      public PrivacyBudgetBrowserTestBaseWithUkmRecording {};

class UkmRecorderAddEntryObserver : public ukm::UkmRecorderObserver {
 public:
  explicit UkmRecorderAddEntryObserver(
      base::RepeatingCallback<void(ukm::mojom::UkmEntryPtr entry)> callback)
      : callback_(std::move(callback)) {}
  void OnEntryAdded(ukm::mojom::UkmEntryPtr entry) override {
    callback_.Run(std::move(entry));
  }

 private:
  base::RepeatingCallback<void(ukm::mojom::UkmEntryPtr entry)> callback_;
};

}  // namespace
