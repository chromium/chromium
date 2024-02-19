// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

using CrossDocumentResourceReuseTest = MetricIntegrationTest;

// The test verifies the metrics for reusing resources among different
// documents. The first and the second page share an image in common. We
// verify the resource reuse is correctly recorded in the UMA.
IN_PROC_BROWSER_TEST_F(CrossDocumentResourceReuseTest,
                       CrossDocumentResourceReuse) {
  Start();
  Load("/cross_document_resource.html");
  // The new document contains an imaged reused from the previous HTML
  Load("/cross_document_resource_reuse.html");

  // Finish session.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  // Actually fetch the metrics.
  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  ExpectUniqueUMABucketCount(
      "Blink.MemoryCache.CrossDocumentCachedResource2",
      static_cast<base::Histogram::Sample>(blink::ResourceType::kImage), 1);
}
