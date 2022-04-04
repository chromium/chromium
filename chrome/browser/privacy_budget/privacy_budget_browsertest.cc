// Copyright 2020 The Chromium Authors. All rights reserved.
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

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "chrome/common/privacy_budget/types.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_test_helper.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/variations/service/buildflags.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"
#include "url/gurl.h"

class Profile;

namespace ukm {
class UkmService;
}  // namespace ukm

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace {

using testing::IsSupersetOf;
using testing::Key;

uint64_t HashFeature(const blink::mojom::WebFeature& feature) {
  return blink::IdentifiableSurface::FromTypeAndToken(
             blink::IdentifiableSurface::Type::kWebFeature, feature)
      .ToUkmMetricHash();
}

// This test runs on Android as well as desktop platforms.
class PrivacyBudgetBrowserTestBase : public SyncTest {
 public:
  PrivacyBudgetBrowserTestBase() : SyncTest(SINGLE_CLIENT) {}

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  static ukm::UkmService* ukm_service() {
    return g_browser_process->GetMetricsServicesManager()->GetUkmService();
  }

  static PrefService* local_state() { return g_browser_process->local_state(); }

  bool EnableUkmRecording() {
    // 1. Enable sync.
    Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
    sync_test_harness_ = metrics::test::InitializeProfileForSync(
        profile, GetFakeServer()->AsWeakPtr());
    EXPECT_TRUE(sync_test_harness_->SetupSync());

    // 2. Signal consent for UKM reporting.
    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    if (consent_service != nullptr)
      consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);

    // 3. Enable metrics reporting.
    is_metrics_reporting_enabled_ = true;
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &is_metrics_reporting_enabled_);

    // UpdateUploadPermissions causes the MetricsServicesManager to look at the
    // consent signals and re-evaluate whether reporting should be enabled.
    g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(
        true);

    // The following sequence synchronously completes UkmService initialization
    // (if it wasn't initialized yet) and flushes any accumulated metrics.
    ukm::UkmTestHelper ukm_test_helper(ukm_service());
    ukm_test_helper.BuildAndStoreLog();
    std::unique_ptr<ukm::Report> report_to_discard =
        ukm_test_helper.GetUkmReport();

    return ukm::UkmTestHelper(ukm_service()).IsRecordingEnabled();
  }

  bool DisableUkmRecording() {
    EXPECT_TRUE(is_metrics_reporting_enabled_)
        << "DisableUkmRecording() should only be called after "
           "EnableUkmRecording()";
    is_metrics_reporting_enabled_ = false;
    g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(
        true);
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
    return !ukm::UkmTestHelper(ukm_service()).IsRecordingEnabled();
  }

  void TearDown() override {
    if (is_metrics_reporting_enabled_) {
      ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
          nullptr);
    }
  }

 private:
  bool is_metrics_reporting_enabled_ = false;
  std::unique_ptr<SyncServiceImplHarness> sync_test_harness_;
};

class PrivacyBudgetBrowserTestWithScopedConfig
    : public PrivacyBudgetBrowserTestBase {
 public:
  PrivacyBudgetBrowserTestWithScopedConfig() {
    privacy_budget_config_.Apply(test::ScopedPrivacyBudgetConfig::Parameters());
    feature_list_.InitAndEnableFeatureWithParameters(
        ukm::kUkmFeature,
        base::FieldTrialParams{{"WhitelistEntries", "Identifiability"}});
  }

 private:
  test::ScopedPrivacyBudgetConfig privacy_budget_config_;
  base::test::ScopedFeatureList feature_list_;
};

class PrivacyBudgetBrowserTestWithTestRecorder
    : public PrivacyBudgetBrowserTestWithScopedConfig {
 public:
  void SetUpOnMainThread() override {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    PrivacyBudgetBrowserTestBase::SetUpOnMainThread();
  }

  ukm::TestUkmRecorder& recorder() { return *ukm_recorder_; }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTestWithTestRecorder,
                       BrowserSideSettingsIsActive) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));
  const auto* settings = blink::IdentifiabilityStudySettings::Get();
  EXPECT_TRUE(settings->IsActive());
}

// When UKM resets the Client ID for some reason the study should reset its
// local state as well.
IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTestWithTestRecorder,
                       UkmClientIdChangesResetStudyState) {
  EXPECT_TRUE(blink::IdentifiabilityStudySettings::Get()->IsActive());
  ASSERT_TRUE(EnableUkmRecording());

  local_state()->SetString(prefs::kPrivacyBudgetSeenSurfaces, "1,2,3");

  ASSERT_TRUE(DisableUkmRecording());

  EXPECT_TRUE(
      local_state()->GetString(prefs::kPrivacyBudgetSeenSurfaces).empty())
      << "Active surface list still exists after resetting client ID";
}

IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTestWithTestRecorder,
                       SamplingScreenAPIs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::DOMMessageQueue messages;
  base::RunLoop run_loop;

  recorder().SetOnAddEntryCallback(ukm::builders::Identifiability::kEntryName,
                                   run_loop.QuitClosure());

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "/privacy_budget/samples_screen_attributes.html")));

  // The document calls a bunch of instrumented functions and sends a message
  // back to the test. Receipt of the message indicates that the script
  // successfully completed.
  std::string screen_scrape;
  ASSERT_TRUE(messages.WaitForMessage(&screen_scrape));

  // The contents of the received message isn't used for anything other than
  // diagnostics.
  SCOPED_TRACE(screen_scrape);

  // Navigating away from the test page causes the document to be unloaded. That
  // will cause any buffered metrics to be flushed.
  content::NavigateToURLBlockUntilNavigationsComplete(web_contents(),
                                                      GURL("about:blank"), 1);

  // Wait for the metrics to come down the pipe.
  run_loop.Run();

  auto merged_entries = recorder().GetMergedEntriesByName(
      ukm::builders::Identifiability::kEntryName);
  // Shouldn't be more than one source here. If this changes, then we'd need to
  // adjust this test to deal.
  ASSERT_EQ(1u, merged_entries.size());

  // All of the following features should be included in the list of returned
  // metrics here. The exact values depend on the test host.
  EXPECT_THAT(
      merged_entries.begin()->second->metrics,
      IsSupersetOf({
          Key(HashFeature(
              blink::mojom::WebFeature::kV8Screen_Height_AttributeGetter)),
          Key(HashFeature(
              blink::mojom::WebFeature::kV8Screen_Width_AttributeGetter)),
          Key(HashFeature(
              blink::mojom::WebFeature::kV8Screen_AvailLeft_AttributeGetter)),
          Key(HashFeature(
              blink::mojom::WebFeature::kV8Screen_AvailTop_AttributeGetter)),
          Key(HashFeature(
              blink::mojom::WebFeature::kV8Screen_AvailWidth_AttributeGetter)),
      }));
}

IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTestWithTestRecorder,
                       CallsCanvasToBlob) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::DOMMessageQueue messages;
  base::RunLoop run_loop;

  recorder().SetOnAddEntryCallback(ukm::builders::Identifiability::kEntryName,
                                   run_loop.QuitClosure());

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "/privacy_budget/calls_canvas_to_blob.html")));

  // The document calls an instrumented method and sends a message
  // back to the test. Receipt of the message indicates that the script
  // successfully completed. However, we must also wait for the UKM metric to be
  // recorded, which happens on a TaskRunner.
  std::string blob_type;
  ASSERT_TRUE(messages.WaitForMessage(&blob_type));

  // Navigating away from the test page causes the document to be unloaded. That
  // will cause any buffered metrics to be flushed.
  content::NavigateToURLBlockUntilNavigationsComplete(web_contents(),
                                                      GURL("about:blank"), 1);

  // Wait for the metrics to come down the pipe.
  content::RunAllTasksUntilIdle();
  run_loop.Run();

  auto merged_entries = recorder().GetMergedEntriesByName(
      ukm::builders::Identifiability::kEntryName);
  // Shouldn't be more than one source here. If this changes, then we'd need to
  // adjust this test to deal.
  ASSERT_EQ(1u, merged_entries.size());

  // toBlob() is called on a context-less canvas, hence -1, which is the value
  // of blink::CanvasRenderingContext::CanvasRenderingAPI::kUnknown.
  constexpr uint64_t input_digest = -1;
  EXPECT_THAT(merged_entries.begin()->second->metrics,
              IsSupersetOf({
                  Key(blink::IdentifiableSurface::FromTypeAndToken(
                          blink::IdentifiableSurface::Type::kCanvasReadback,
                          input_digest)
                          .ToUkmMetricHash()),
              }));
}

// TODO(crbug.com/1238940, crbug.com/1238859): Test is flaky on Win and Android.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#define MAYBE_CanvasToBlobDifferentDocument \
  DISABLED_CanvasToBlobDifferentDocument
#else
#define MAYBE_CanvasToBlobDifferentDocument CanvasToBlobDifferentDocument
#endif
IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTestWithTestRecorder,
                       MAYBE_CanvasToBlobDifferentDocument) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::DOMMessageQueue messages;
  base::RunLoop run_loop;

  recorder().SetOnAddEntryCallback(ukm::builders::Identifiability::kEntryName,
                                   run_loop.QuitClosure());

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_test_server()->GetURL(
                          "/privacy_budget/calls_canvas_to_blob_xdoc.html")));

  // The document calls an instrumented method and sends a message
  // back to the test. Receipt of the message indicates that the script
  // successfully completed. However, we must also wait for the UKM metric to be
  // recorded, which happens on a TaskRunner.
  std::string message;
  ASSERT_TRUE(messages.WaitForMessage(&message));

  // Navigating away from the test page causes the document to be unloaded. That
  // will cause any buffered metrics to be flushed.
  content::NavigateToURLBlockUntilNavigationsComplete(web_contents(),
                                                      GURL("about:blank"), 1);

  // Wait for the metrics to come down the pipe.
  content::RunAllTasksUntilIdle();
  run_loop.Run();

  auto merged_entries = recorder().GetMergedEntriesByName(
      ukm::builders::Identifiability::kEntryName);
  // Shouldn't be more than one source here. If this changes, then we'd need to
  // adjust this test to deal.
  ASSERT_EQ(1u, merged_entries.size());

  auto& metrics = merged_entries.begin()->second->metrics;

  // (kCanvasReadback | input_digest << kTypeBits) = one of the merged_entries
  // If the value of the relevant merged entry changes, input_digest needs to
  // change. The new input_digest can be calculated by:
  // new_input_digest = new_ukm_entry >> kTypeBits
  constexpr uint64_t input_digest = UINT64_C(33457614533296512);
  EXPECT_THAT(metrics,
              IsSupersetOf({
                  Key(blink::IdentifiableSurface::FromTypeAndToken(
                          blink::IdentifiableSurface::Type::kCanvasReadback,
                          input_digest)
                          .ToUkmMetricHash()),
              }));

  for (auto& metric : metrics) {
    auto surface(blink::IdentifiableSurface::FromMetricHash(metric.first));
    LOG(INFO) << "surface type " << static_cast<uint64_t>(surface.GetType())
              << " surface input hash " << surface.GetInputHash() << " value "
              << metric.second;
  }
}

IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTestWithScopedConfig,
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

IN_PROC_BROWSER_TEST_F(PrivacyBudgetGroupConfigBrowserTest, LoadsAGroup) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));

  const auto* settings = blink::IdentifiabilityStudySettings::Get();
  ASSERT_TRUE(settings->IsActive());
}

// The following test requires that the testing config defined in
// testing/variations/fieldtrial_testing_config.json is applied. The testing
// config is only applied by default if 1) the
// "disable_fieldtrial_testing_config" GN flag is set to false, and 2) the build
// is a non-Chrome branded build.
#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED) && !BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace {
class PrivacyBudgetFieldtrialConfigTest : public PrivacyBudgetBrowserTestBase {
 public:
  // This surface is blocked in fieldtrial_testing_config.json.
  static constexpr auto kBlockedSurface =
      blink::IdentifiableSurface::FromMetricHash(44033);

  // This surface is not mentioned in fieldtrial_testing_config.json and is
  // not blocked by default. It should be considered allowed, but its metrics
  // will not be recorded because it is not one of the active surfaces.
  static constexpr auto kAllowedInactiveSurface =
      blink::IdentifiableSurface::FromMetricHash(44290);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

const blink::IdentifiableSurface
    PrivacyBudgetFieldtrialConfigTest::kBlockedSurface;
const blink::IdentifiableSurface
    PrivacyBudgetFieldtrialConfigTest::kAllowedInactiveSurface;
}  // namespace

// //testing/variations/fieldtrial_testing_config.json defines a set of
// parameters that should effectively enable the identifiability study for
// browser tests. This test verifies that those settings work. This isn't
// testing whether fieldtrial work, but rather we are testing if the process
// picks up the config correctly since it is non-trivial.
IN_PROC_BROWSER_TEST_F(PrivacyBudgetFieldtrialConfigTest,
                       LoadsSettingsFromFieldTrialConfig) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));
  ASSERT_TRUE(EnableUkmRecording());

  const auto* settings = blink::IdentifiabilityStudySettings::Get();
  EXPECT_TRUE(settings->IsActive());
  // Allowed by default.
  EXPECT_TRUE(settings->ShouldSampleType(
      blink::IdentifiableSurface::Type::kCanvasReadback));

  // Blocked surfaces. See fieldtrial_testing_config.json#IdentifiabilityStudy.
  EXPECT_FALSE(settings->ShouldSampleSurface(kBlockedSurface));

  // Some random surface that shouldn't be blocked.
  EXPECT_TRUE(settings->ShouldSampleSurface(kAllowedInactiveSurface));

  // Blocked types
  EXPECT_FALSE(settings->ShouldSampleType(
      blink::IdentifiableSurface::Type::kLocalFontLookupByFallbackCharacter));
  EXPECT_FALSE(settings->ShouldSampleType(
      blink::IdentifiableSurface::Type::kMediaCapabilities_DecodingInfo));
}

#endif  // BUILDFLAG(FIELDTRIAL_TESTING_ENABLED) &&
        // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
