// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <iosfwd>
#include <map>
#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_test_helper.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/variations/service/buildflags.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content
namespace ukm {
class UkmService;
}  // namespace ukm

#if defined(OS_ANDROID)
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
class PrivacyBudgetBrowserTest : public SyncTest {
 public:
  PrivacyBudgetBrowserTest() : SyncTest(SINGLE_CLIENT) {
    privacy_budget_config_.Apply(test::ScopedPrivacyBudgetConfig::Parameters());
  }

  void SetUpOnMainThread() override {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    SyncTest::SetUpOnMainThread();
  }

  ukm::TestUkmRecorder& recorder() { return *ukm_recorder_; }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  ukm::UkmService* ukm_service() const {
    return g_browser_process->GetMetricsServicesManager()->GetUkmService();
  }

  PrefService* local_state() const { return g_browser_process->local_state(); }

  void EnableSyncForProfile(Profile* profile) {
    std::unique_ptr<ProfileSyncServiceHarness> harness =
        metrics::test::InitializeProfileForSync(profile,
                                                GetFakeServer()->AsWeakPtr());
    EXPECT_TRUE(harness->SetupSync());

    unified_consent::UnifiedConsentService* consent_service =
        UnifiedConsentServiceFactory::GetForProfile(profile);
    if (consent_service)
      consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
  }

 private:
  test::ScopedPrivacyBudgetConfig privacy_budget_config_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTest, BrowserSideSettingsIsActive) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));
  auto* settings = blink::IdentifiabilityStudySettings::Get();
  EXPECT_TRUE(settings->IsActive());
}

// When UKM resets the Client ID for some reason the study should reset its
// local state as well.
IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTest,
                       UkmClientIdChangesResetStudyState) {
  EXPECT_TRUE(blink::IdentifiabilityStudySettings::Get()->IsActive());

  // Sync needs to be enabled for reporting to be enabled.
  auto* profile = ProfileManager::GetActiveUserProfile();
  EnableSyncForProfile(profile);

  bool reporting_allowed = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &reporting_allowed);

  // UpdateUploadPermissions causes the MetricsServicesManager to look at the
  // consent signals and re-evaluate whether reporting should be enabled.
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);

  ASSERT_TRUE(ukm::UkmTestHelper(ukm_service()).IsRecordingEnabled())
      << "UKM recording not enabled";

  local_state()->SetString(prefs::kPrivacyBudgetActiveSurfaces, "1,2,3");
  const auto first_prng_seed =
      local_state()->GetUint64(prefs::kPrivacyBudgetSeed);

  // Disallowing reporting is equivalent to revoking consent.
  reporting_allowed = false;
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);
  ASSERT_FALSE(ukm::UkmTestHelper(ukm_service()).IsRecordingEnabled())
      << "UKM recording not disabled";

  const auto second_prng_seed =
      local_state()->GetUint64(prefs::kPrivacyBudgetSeed);

  EXPECT_NE(first_prng_seed, second_prng_seed)
      << "PRNG seeds from before and after resetting UKM Client ID are still "
         "the same";
  EXPECT_TRUE(
      local_state()->GetString(prefs::kPrivacyBudgetActiveSurfaces).empty())
      << "Active surface list still exists after resetting client ID";
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTest, SamplingScreenAPIs) {
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

IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTest, CallsCanvasToBlob) {
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

  constexpr uint64_t input_digest = 9;
  EXPECT_THAT(merged_entries.begin()->second->metrics,
              IsSupersetOf({
                  Key(blink::IdentifiableSurface::FromTypeAndToken(
                          blink::IdentifiableSurface::Type::kCanvasReadback,
                          input_digest)
                          .ToUkmMetricHash()),
              }));
}

IN_PROC_BROWSER_TEST_F(PrivacyBudgetBrowserTest,
                       CanvasToBlobDifferentDocument) {
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

  // (kCanvasReadback | input_digest << kTypeBits) = one of the merged_entries
  // If the value of the relevant merged entry changes, input_digest needs to
  // change. The new input_digest can be calculated by:
  // new_input_digest = new_ukm_entry >> kTypeBits
  constexpr uint64_t input_digest = UINT64_C(61919955620835840);
  EXPECT_THAT(merged_entries.begin()->second->metrics,
              IsSupersetOf({
                  Key(blink::IdentifiableSurface::FromTypeAndToken(
                          blink::IdentifiableSurface::Type::kCanvasReadback,
                          input_digest)
                          .ToUkmMetricHash()),
              }));
}

#if BUILDFLAG(FIELDTRIAL_TESTING_ENABLED)

namespace {
class PrivacyBudgetDefaultConfigBrowserTest : public PlatformBrowserTest {};
}  // namespace

// //testing/variations/fieldtrial_testing_config.json defines a set of
// parameters that should effectively enable the identifiability study for
// browser tests. This test verifies that those settings work.
IN_PROC_BROWSER_TEST_F(PrivacyBudgetDefaultConfigBrowserTest, Variations) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kIdentifiabilityStudy));

  auto* settings = blink::IdentifiabilityStudySettings::Get();
  EXPECT_TRUE(settings->IsActive());
  EXPECT_TRUE(settings->IsTypeAllowed(
      blink::IdentifiableSurface::Type::kCanvasReadback));
  EXPECT_FALSE(
      settings->IsTypeAllowed(blink::IdentifiableSurface::Type::kMediaQuery));
}

#endif
