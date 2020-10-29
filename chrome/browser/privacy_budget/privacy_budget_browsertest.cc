// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/variations/service/buildflags.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-shared.h"

#if defined(OS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace {

using testing::IsSupersetOf;
using testing::Key;

// This test runs on Android as well as desktop platforms.
class PrivacyBudgetBrowserTest : public PlatformBrowserTest {
 public:
  PrivacyBudgetBrowserTest() {
    privacy_budget_config_.Apply(test::ScopedPrivacyBudgetConfig::Parameters());
  }

  void SetUpOnMainThread() override {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  ukm::TestUkmRecorder& recorder() { return *ukm_recorder_; }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
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
  const auto& metrics = merged_entries.begin()->second->metrics;

  // All of the following features should be included in the list of returned
  // metrics here. The exact values depend on the test host.
  for (auto feature :
       {blink::mojom::WebFeature::kV8Screen_Height_AttributeGetter,
        blink::mojom::WebFeature::kV8Screen_Width_AttributeGetter,
        blink::mojom::WebFeature::kV8Screen_AvailLeft_AttributeGetter,
        blink::mojom::WebFeature::kV8Screen_AvailTop_AttributeGetter,
        blink::mojom::WebFeature::kV8Screen_AvailWidth_AttributeGetter,
        blink::mojom::WebFeature::kV8Screen_Height_AttributeGetter}) {
    EXPECT_TRUE(metrics.contains(
        blink::IdentifiableSurface::FromTypeAndToken(
            blink::IdentifiableSurface::Type::kWebFeature, feature)
            .ToUkmMetricHash()));
  }
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
}

#endif
