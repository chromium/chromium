// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/identifiability_metrics_test_util.h"

#include "base/run_loop.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/identifiability_metrics.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_sample_collector.h"

namespace extensions {

IdentifiabilityMetricsTestHelper::IdentifiabilityMetricsTestHelper() {
  privacy_budget_config_.Apply(test::ScopedPrivacyBudgetConfig::Parameters(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling));
}

IdentifiabilityMetricsTestHelper::~IdentifiabilityMetricsTestHelper() = default;

void IdentifiabilityMetricsTestHelper::SetUpOnMainThread() {
  ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
}

void IdentifiabilityMetricsTestHelper::PrepareForTest(base::RunLoop* run_loop) {
  DCHECK(ukm_recorder_) << "IdentifiabilityMetricsTestHelper::"
                           "SetUpOnMainThread hasn't been called";
  ukm_recorder_->SetOnAddEntryCallback(
      ukm::builders::Identifiability::kEntryName, run_loop->QuitClosure());
}

std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr>
IdentifiabilityMetricsTestHelper::NavigateToBlankAndWaitForMetrics(
    content::WebContents* contents,
    base::RunLoop* run_loop) {
  DCHECK(ukm_recorder_) << "IdentifiabilityMetricsTestHelper::"
                           "SetUpOnMainThread hasn't been called";

  // Need to navigate away to force a metrics flush; otherwise it would be
  // dependent on periodic flush heuristics.
  content::NavigateToURLBlockUntilNavigationsComplete(contents,
                                                      GURL("about:blank"), 1);

  // Also force a browser-side flush.
  blink::IdentifiabilitySampleCollector::Get()->Flush(ukm::UkmRecorder::Get());

  run_loop->Run();
  return ukm_recorder_->GetMergedEntriesByName(
      ukm::builders::Identifiability::kEntryName);
}

std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr>
IdentifiabilityMetricsTestHelper::NavigateToBlankAndWaitForMetrics(
    content::RenderFrameHost* render_frame_host,
    base::RunLoop* run_loop) {
  DCHECK(ukm_recorder_) << "IdentifiabilityMetricsTestHelper::"
                           "SetUpOnMainThread hasn't been called";

  // Need to navigate away to force a metrics flush; otherwise it would be
  // dependent on periodic flush heuristics.
  EXPECT_TRUE(content::NavigateToURLFromRenderer(render_frame_host,
                                                 GURL("about:blank")));

  // Also force a browser-side flush.
  blink::IdentifiabilitySampleCollector::Get()->Flush(ukm::UkmRecorder::Get());

  run_loop->Run();
  return ukm_recorder_->GetMergedEntriesByName(
      ukm::builders::Identifiability::kEntryName);
}

void IdentifiabilityMetricsTestHelper::EnsureIdentifiabilityEventGenerated(
    content::WebContents* contents) {
  // Create a canvas and serialize it to force at least one event to happen,
  // since otherwise there is no way to synchronize with the renderer.
  constexpr char kForceMetricScript[] =
      R"(
        var c = document.createElement("canvas");
        document.body.appendChild(c);
        var ctx = c.getContext("2d");
        var url = c.toDataURL();
      )";
  EXPECT_TRUE(content::ExecJs(contents, kForceMetricScript));
}

// static
bool IdentifiabilityMetricsTestHelper::ContainsSurfaceOfType(
    const std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr>& merged_entries,
    blink::IdentifiableSurface::Type type) {
  for (const auto& entry : merged_entries) {
    const auto& metrics = entry.second->metrics;
    for (const auto& surface_value : metrics) {
      if (blink::IdentifiableSurface::FromMetricHash(surface_value.first)
              .GetType() == type) {
        return true;
      }
    }
  }
  return false;
}

// static
std::set<ukm::SourceId>
IdentifiabilityMetricsTestHelper::GetSourceIDsForSurfaceAndExtension(
    const std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr>& merged_entries,
    blink::IdentifiableSurface::Type type,
    const ExtensionId& extension_id) {
  std::set<ukm::SourceId> result;
  for (const auto& entry : merged_entries) {
    const auto& metrics = entry.second->metrics;
    if (metrics.contains(
            SurfaceForExtension(type, extension_id).ToUkmMetricHash())) {
      result.insert(entry.first);
    }
  }
  return result;
}

}  // namespace extensions
