// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/integration_tests/metric_integration_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using ukm::builders::Graphics_Smoothness_NormalizedPercentDroppedFrames;

namespace {

bool ExtractUKMSmoothnessMetric(const ukm::TestUkmRecorder& ukm_recorder,
                                std::string_view metric_name,
                                int64_t* extracted_value) {
  std::map<ukm::SourceId, ukm::mojom::UkmEntryPtr> merged_entries =
      ukm_recorder.GetMergedEntriesByName(
          Graphics_Smoothness_NormalizedPercentDroppedFrames::kEntryName);
  EXPECT_EQ(1ul, merged_entries.size());
  if (merged_entries.size() != 1u)
    return false;
  const auto& kv = merged_entries.begin();
  auto* metric_value =
      ukm::TestUkmRecorder::GetEntryMetric(kv->second.get(), metric_name);
  if (!metric_value)
    return false;
  *extracted_value = *metric_value;
  return true;
}

}  // namespace

// TODO(crbug.com/40934889): Re-enable this test
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_BasicSmoothnessAverage DISABLED_BasicSmoothnessAverage
#else
#define MAYBE_BasicSmoothnessAverage BasicSmoothnessAverage
#endif
IN_PROC_BROWSER_TEST_F(MetricIntegrationTest, MAYBE_BasicSmoothnessAverage) {
  LoadHTML(R"HTML(<div id='animate' style='width: 20px; height: 20px'></div>
    <img src="images/lcp-16x16.png"></img>
    <script>
      runtest = async() => {
        const promise = new Promise(resolve => {
          var r = 0;
          function run() {
            if (r >= 200) {
              resolve(true);
              return;
            }
            const now = new Date();
            while ((new Date() - now) < 20) {}
            animate.style.backgroundColor = `rgb(${r * 2}, 0, 0)`;
            requestAnimationFrame(run);
            ++r;
          }
          run();
        });
        return await promise;
      };
    </script>
    )HTML");

  ASSERT_TRUE(EvalJs(web_contents(), "runtest()").ExtractBool());

  // Finish session.
  web_contents()->ClosePage();
  ui_test_utils::WaitForBrowserToClose(browser());

  // Ensure that the smoothness UKM is reported.
  int64_t avg_value;
  ASSERT_TRUE(ExtractUKMSmoothnessMetric(
      ukm_recorder(),
      Graphics_Smoothness_NormalizedPercentDroppedFrames::kAverageName,
      &avg_value));
  // Some of the frames should be dropped. It is not possible to measure the
  // exact number of dropped frames, so validate that it is non-zero.
  EXPECT_NE(avg_value, 0);
}
