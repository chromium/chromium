// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_IDENTIFIABILITY_METRICS_TEST_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_IDENTIFIABILITY_METRICS_TEST_UTIL_H_

#include "base/run_loop.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "extensions/common/extension_id.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace content {
class WebContents;
class RenderFrameHost;
}

namespace extensions {

// This can be incorporated into an in-process browser test to help test
// which identifiability metrics got collected.
//
// Usage:
// 1) include as a member of test fixture, e.g.
// identifiability_metrics_test_helper_
// 2) Call SetUpOnMainThread() from fixture's SetUpOnMainThread().
// 3) In the test:
//    base::RunLoop run_loop;
//    identifiability_metrics_test_helper_.PrepareForTest(&run_loop);
//    /* do stuff */
//    auto metrics =
//        identifiability_metrics_test_helper_.NavigateToBlankAndWaitForMetrics(
//            web_contents, &run_loop);
//    /* check that metrics has the right stuff.
//       extensions::SurfaceForExtension may be useful here. */
//
// For negative tests (those where the test page doesn't generate
// identifiability UKM), you can call EnsureIdentifiabilityEventGenerated() to
// give NavigateToBlankAndWaitForMetrics something to wait for.
class IdentifiabilityMetricsTestHelper {
 public:
  IdentifiabilityMetricsTestHelper();
  ~IdentifiabilityMetricsTestHelper();

  void SetUpOnMainThread();

  void PrepareForTest(base::RunLoop* run_loop);

  // Navigates to about:blank and returns metrics from the page that is
  // replaced.
  //
  // WARNING: The situation where both renderer and browser produce these events
  // currently hasn't been tested with this method.
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr>
  NavigateToBlankAndWaitForMetrics(content::WebContents* contents,
                                   base::RunLoop* run_loop);

  // Similar to the above, but uses RenderFrameHost.
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr>
  NavigateToBlankAndWaitForMetrics(content::RenderFrameHost* render_frame_host,
                                   base::RunLoop* run_loop);

  // Makes sure that |contents| has a non-extension identifiability event
  // generated on it, so that NavigateToBlankAndWaitForMetrics() can terminate
  // in negative tests.
  void EnsureIdentifiabilityEventGenerated(content::WebContents* contents);

  // Returns whether the passed in map has any identifiability event for a given
  // surface type.
  static bool ContainsSurfaceOfType(
      const std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr>& merged_entries,
      blink::IdentifiableSurface::Type type);

  // Returns for which UKM IDs the passed in map has a identifiability event for
  // the exact surface + extension ID pair.
  static std::set<ukm::SourceId> GetSourceIDsForSurfaceAndExtension(
      const std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr>& merged_entries,
      blink::IdentifiableSurface::Type type,
      const ExtensionId& extension_id);

 private:
  test::ScopedPrivacyBudgetConfig privacy_budget_config_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_IDENTIFIABILITY_METRICS_TEST_UTIL_H_
