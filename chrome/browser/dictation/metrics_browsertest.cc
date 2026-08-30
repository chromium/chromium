// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/dictation/dictation_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "content/public/browser/editable_level.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/global_dom_node_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/dom/dom_node_id.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

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

// Note: DictationUrlCategory::kGlic is tested in
// DictationGlicBrowserTest.RecordsSessionUrlCategoryGlic in
// dictation_keyed_service_browsertest.cc.
IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordSessionUrlCategoryWebOnSessionStart) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  histogram_tester.ExpectUniqueSample(kSessionUrlCategoryHistogramName,
                                      DictationUrlCategory::kWeb, 1);
}

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordSessionUrlCategoryOnlyOnceForExistingSession) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  // Trigger context menu again during existing session.
  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(456));

  // Category is only recorded once for the session.
  histogram_tester.ExpectUniqueSample(kSessionUrlCategoryHistogramName,
                                      DictationUrlCategory::kWeb, 1);
  histogram_tester.ExpectTotalCount(kSessionUrlCategoryHistogramName, 1);
}

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordMetricsOnStreamExitUserDone) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  session_controller()->UiRequestEndActiveStream();

  histogram_tester.ExpectUniqueSample(kStreamExitReasonHistogramName,
                                      DictationStreamExitStatus::kUserDone, 1);
}

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordMetricsOnStreamExitUserDoneHotkeyToggle) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  ExtensionWaitForStreamStart(profile(),
                              attached_stream()->stream_id_for_testing());
  ExtensionSendStreamStateUpdate(
      profile(), attached_stream()->stream_id_for_testing(),
      extensions::api::dictation_private::StreamState::kTranscribing);
  WaitForSessionState(SessionState::kTranscribing);

  dictation_service().ToggleHotkeyHandler();

  histogram_tester.ExpectUniqueSample(kStreamExitReasonHistogramName,
                                      DictationStreamExitStatus::kUserDone, 1);
}

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordMetricsOnStreamExitUserDoneEscapeKey) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  blink::WebKeyboardEvent esc_event(blink::WebInputEvent::Type::kRawKeyDown,
                                    blink::WebInputEvent::kNoModifiers,
                                    base::TimeTicks::Now());
  esc_event.windows_key_code = ui::VKEY_ESCAPE;
  session_controller()->DidGetUserInteraction(esc_event);

  histogram_tester.ExpectUniqueSample(kStreamExitReasonHistogramName,
                                      DictationStreamExitStatus::kUserDone, 1);
}

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordMetricsOnStreamExitUserCancel) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  session_controller()->UiRequestEndSession();

  histogram_tester.ExpectUniqueSample(kStreamExitReasonHistogramName,
                                      DictationStreamExitStatus::kUserCancelled,
                                      1);
}

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordMetricsOnStreamExitAutoDoneTabSwitch) {
  base::HistogramTester histogram_tester;

  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  browser()->tab_strip_model()->ActivateTabAt(1);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return session_controller() == nullptr ||
           session_controller()->attached_stream_provider() == nullptr;
  }));

  histogram_tester.ExpectUniqueSample(kStreamExitReasonHistogramName,
                                      DictationStreamExitStatus::kAutoDone, 1);
}

IN_PROC_BROWSER_TEST_F(
    DictationMetricsBrowserTest,
    RecordMetricsOnStreamExitAutoDoneFocusChangeToNonEditable) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  content::FocusedNodeDetails details;
  details.focus_type = blink::mojom::FocusType::kMouse;
  details.editable_level = content::EditableLevel::kNotEditable;
  details.global_dom_node_id = content::GlobalDOMNodeId(
      web_contents()->GetPrimaryMainFrame()->GetWeakDocumentPtr(),
      blink::DOMNodeIdType(456));

  session_controller()->OnFocusChangedInPage(details);

  histogram_tester.ExpectUniqueSample(kStreamExitReasonHistogramName,
                                      DictationStreamExitStatus::kAutoDone, 1);
}

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordMetricsOnStreamExitAutoDoneStreamTimeout) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  ExtensionWaitForStreamStart(profile(),
                              attached_stream()->stream_id_for_testing());
  ExtensionSendStreamStateUpdate(
      profile(), attached_stream()->stream_id_for_testing(),
      extensions::api::dictation_private::StreamState::kComplete);

  WaitForSessionState(SessionState::kInactive);

  histogram_tester.ExpectUniqueSample(kStreamExitReasonHistogramName,
                                      DictationStreamExitStatus::kAutoDone, 1);
}

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordMetricsOnStreamExitSpeechError) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  ExtensionWaitForStreamStart(profile(),
                              attached_stream()->stream_id_for_testing());
  ExtensionSendStreamStateUpdate(
      profile(), attached_stream()->stream_id_for_testing(),
      extensions::api::dictation_private::StreamState::kFailed);

  WaitForSessionState(SessionState::kInactive);

  histogram_tester.ExpectUniqueSample(kStreamExitReasonHistogramName,
                                      DictationStreamExitStatus::kSpeechError,
                                      1);
}

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordMetricsOnStreamExitAutoDoneNewSession) {
  base::HistogramTester histogram_tester;

  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  // Trigger context menu again during existing session to stop previous stream.
  SimulateInvokeViaContextMenu(web_contents()->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(456));

  histogram_tester.ExpectBucketCount(kStreamExitReasonHistogramName,
                                     DictationStreamExitStatus::kAutoDone, 1);
}

IN_PROC_BROWSER_TEST_F(DictationMetricsBrowserTest,
                       RecordMetricsOnStreamExitAutoCancelTabClosed) {
  base::HistogramTester histogram_tester;

  // Open a second tab and activate it.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(1);

  content::WebContents* tab2_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(tab2_contents, nullptr);

  SimulateInvokeViaContextMenu(tab2_contents->GetPrimaryMainFrame(),
                               blink::DOMNodeIdType(123));

  // Close the tab while the stream is active.
  browser()->tab_strip_model()->CloseWebContentsAt(
      1, TabCloseTypes::CLOSE_USER_GESTURE);

  WaitForSessionState(SessionState::kInactive);

  histogram_tester.ExpectUniqueSample(kStreamExitReasonHistogramName,
                                      DictationStreamExitStatus::kAutoCancelled,
                                      1);
}

}  // namespace dictation
