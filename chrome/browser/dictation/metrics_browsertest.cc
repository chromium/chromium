// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/dictation/dictation_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"

namespace dictation {

class DictationMetricsBrowserTest : public DictationBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordMetricsOnSessionStart) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  histogram_tester.ExpectUniqueSample(kSessionStartSourceHistogramName,
                                      DictationSessionEntryPoint::kContextMenu,
                                      1);
  histogram_tester.ExpectUniqueSample(
      kStreamStartTriggerHistogramName,
      DictationStreamStartTrigger::kSessionStart, 1);
}

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordMetricsOnContextMenuExistingSessionSameTab) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  histogram_tester.ExpectBucketCount(kStreamStartTriggerHistogramName,
                                     DictationStreamStartTrigger::kSessionStart,
                                     1);

  // Trigger context menu again during existing session.
  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(456));

  histogram_tester.ExpectBucketCount(
      kStreamStartTriggerHistogramName,
      DictationStreamStartTrigger::kContextMenuExistingSession, 1);

  // Exactly two streams were started.
  histogram_tester.ExpectTotalCount(kStreamStartTriggerHistogramName, 2);

  // Only a single session was used for both streams.
  histogram_tester.ExpectTotalCount(kSessionStartSourceHistogramName, 1);
}

IN_PROC_BROWSER_TEST_F(
    DictationMetricsBrowserTest,
    RecordMetricsOnContextMenuExistingSessionDifferentWindow) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));
  histogram_tester.ExpectBucketCount(kStreamStartTriggerHistogramName,
                                     DictationStreamStartTrigger::kSessionStart,
                                     1);

  Browser* second_browser = CreateBrowser(profile());
  content::WebContents* window2_contents =
      second_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(window2_contents, nullptr);

  SimulateInvokeViaContextMenu(window2_contents->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(456));
  histogram_tester.ExpectBucketCount(
      kStreamStartTriggerHistogramName,
      DictationStreamStartTrigger::kContextMenuExistingSession, 1);

  // Exactly two streams were started.
  histogram_tester.ExpectTotalCount(kStreamStartTriggerHistogramName, 2);

  // Only a single session was used for both streams.
  histogram_tester.ExpectTotalCount(kSessionStartSourceHistogramName, 1);
}

}  // namespace dictation
