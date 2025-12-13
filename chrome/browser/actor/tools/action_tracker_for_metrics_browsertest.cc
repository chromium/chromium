// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/actor.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace actor {
namespace {

using ::content::GetDOMNodeId;
using ::content::NavigateToURL;

constexpr char kActorTaskSubsequentWaitsMetricName[] =
    "Actor.Task.SubsequentWaits";

class ActionTrackerForMetricsTest : public ActorToolsTest {
 public:
  ActionTrackerForMetricsTest() = default;
  ~ActionTrackerForMetricsTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  ActorKeyedService* actor_keyed_service() {
    return ActorKeyedService::Get(browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(ActionTrackerForMetricsTest, WaitAfterClick_Recorded) {
  base::HistogramTester histogram_tester;

  const GURL url1 = embedded_test_server()->GetURL("/actor/blank.html");
  const GURL url2 =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));

  ActResultFuture result1;
  actor_task().Act(
      ToRequestList(MakeNavigateRequest(*active_tab(), url2.spec())),
      result1.GetCallback());
  ExpectOkResult(result1);

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);

  ActResultFuture result2;
  actor_task().Act(
      ToRequestList(MakeClickRequest(*main_frame(), button_id.value())),
      result2.GetCallback());
  ExpectOkResult(result2);

  ActResultFuture result3;
  actor_task().Act(ToRequestList(MakeWaitRequest()), result3.GetCallback());
  ExpectOkResult(result3);

  actor_keyed_service()->ResetForTesting();

  histogram_tester.ExpectUniqueSample("Actor.Task.SubsequentWaits.Click",
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  // Only count Waits immediately after the action.
  histogram_tester.ExpectTotalCount("Actor.Task.SubsequentWaits.Navigate",
                                    /*expected_count=*/0);
  histogram_tester.ExpectUniqueSample(kActorTaskSubsequentWaitsMetricName,
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(ActionTrackerForMetricsTest, TwoWaits_Recorded) {
  base::HistogramTester histogram_tester;

  const GURL url1 = embedded_test_server()->GetURL("/actor/blank.html");
  const GURL url2 =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));

  ActResultFuture result1;
  actor_task().Act(
      ToRequestList(MakeNavigateRequest(*active_tab(), url2.spec())),
      result1.GetCallback());
  ExpectOkResult(result1);

  ActResultFuture result2;
  actor_task().Act(ToRequestList(MakeWaitRequest()), result2.GetCallback());
  ExpectOkResult(result2);

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);

  ActResultFuture result3;
  actor_task().Act(
      ToRequestList(MakeClickRequest(*main_frame(), button_id.value())),
      result3.GetCallback());
  ExpectOkResult(result3);

  ActResultFuture result4;
  actor_task().Act(ToRequestList(MakeWaitRequest()), result4.GetCallback());
  ExpectOkResult(result4);

  actor_keyed_service()->ResetForTesting();

  histogram_tester.ExpectUniqueSample("Actor.Task.SubsequentWaits.Click",
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample("Actor.Task.SubsequentWaits.Navigate",
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(kActorTaskSubsequentWaitsMetricName,
                                      /*sample=*/2,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(ActionTrackerForMetricsTest,
                       WaitAfterMultipleActions_Recorded) {
  base::HistogramTester histogram_tester;

  const GURL url1 =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  const GURL url2 = embedded_test_server()->GetURL("/actor/blank.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));

  std::vector<std::unique_ptr<ToolRequest>> actions;

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);
  actions.push_back(MakeClickRequest(*main_frame(), button_id.value()));

  actions.push_back(MakeNavigateRequest(*active_tab(), url2.spec()));

  ActResultFuture result;
  actor_task().Act(std::move(actions), result.GetCallback());
  ExpectOkResult(result);

  ActResultFuture result2;
  actor_task().Act(ToRequestList(MakeWaitRequest()), result2.GetCallback());
  ExpectOkResult(result2);

  actor_keyed_service()->ResetForTesting();

  histogram_tester.ExpectUniqueSample("Actor.Task.SubsequentWaits.Navigate",
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  // Only tracks the last action in the sequence.
  histogram_tester.ExpectTotalCount("Actor.Task.SubsequentWaits.Click",
                                    /*expected_count=*/0);
  histogram_tester.ExpectUniqueSample(kActorTaskSubsequentWaitsMetricName,
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(ActionTrackerForMetricsTest,
                       WaitAfterCreated_NotRecorded) {
  base::HistogramTester histogram_tester;

  const GURL url = embedded_test_server()->GetURL("/actor/blank.html");
  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  ActResultFuture result;
  actor_task().Act(ToRequestList(MakeWaitRequest()), result.GetCallback());
  ExpectOkResult(result);

  actor_keyed_service()->ResetForTesting();

  histogram_tester.ExpectUniqueSample(kActorTaskSubsequentWaitsMetricName,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(ActionTrackerForMetricsTest,
                       WaitAfterPaused_NotRecorded) {
  base::HistogramTester histogram_tester;

  const GURL url1 = embedded_test_server()->GetURL("/actor/blank.html");
  const GURL url2 =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));

  ActResultFuture result1;
  actor_task().Act(
      ToRequestList(MakeNavigateRequest(*active_tab(), url2.spec())),
      result1.GetCallback());
  ExpectOkResult(result1);

  actor_task().Pause(/*from_actor=*/true);
  actor_task().Resume();

  ActResultFuture result2;
  actor_task().Act(ToRequestList(MakeWaitRequest()), result2.GetCallback());
  ExpectOkResult(result2);

  actor_keyed_service()->ResetForTesting();

  histogram_tester.ExpectUniqueSample(kActorTaskSubsequentWaitsMetricName,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(ActionTrackerForMetricsTest, ZeroDurationWait_Recorded) {
  base::HistogramTester histogram_tester;

  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);

  ActResultFuture result1;
  actor_task().Act(
      ToRequestList(MakeClickRequest(*main_frame(), button_id.value())),
      result1.GetCallback());
  ExpectOkResult(result1);

  ActResultFuture result2;
  std::unique_ptr<ToolRequest> wait_action = std::make_unique<WaitToolRequest>(
      /*wait_duration=*/base::TimeDelta(), active_tab()->GetHandle());
  actor_task().Act(ToRequestList(std::move(wait_action)),
                   result2.GetCallback());
  ExpectOkResult(result2);

  actor_keyed_service()->ResetForTesting();

  histogram_tester.ExpectUniqueSample(kActorTaskSubsequentWaitsMetricName,
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(ActionTrackerForMetricsTest,
                       WaitInMultipleActions_NotRecorded) {
  base::HistogramTester histogram_tester;

  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);

  std::vector<std::unique_ptr<ToolRequest>> actions;
  actions.push_back(MakeClickRequest(*main_frame(), button_id.value()));
  actions.push_back(MakeWaitRequest());

  ActResultFuture result;
  actor_task().Act(std::move(actions), result.GetCallback());
  ExpectOkResult(result);

  actor_keyed_service()->ResetForTesting();

  histogram_tester.ExpectUniqueSample(kActorTaskSubsequentWaitsMetricName,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(ActionTrackerForMetricsTest,
                       WaitAfterFailure_NotRecorded) {
  base::HistogramTester histogram_tester;

  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);

  ActResultFuture result1;
  actor_task().Act(
      ToRequestList(MakeClickRequest(*main_frame(), kNonExistentContentNodeId)),
      result1.GetCallback());
  ExpectErrorResult(result1, mojom::ActionResultCode::kInvalidDomNodeId);

  ActResultFuture result2;
  actor_task().Act(ToRequestList(MakeWaitRequest()), result2.GetCallback());
  ExpectOkResult(result2);

  actor_keyed_service()->ResetForTesting();

  histogram_tester.ExpectUniqueSample(kActorTaskSubsequentWaitsMetricName,
                                      /*sample=*/0,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(ActionTrackerForMetricsTest,
                       TwoWaitsAfterClick_Recorded) {
  base::HistogramTester histogram_tester;

  const GURL url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url));

  std::optional<int> button_id =
      GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);

  ActResultFuture result1;
  actor_task().Act(
      ToRequestList(MakeClickRequest(*main_frame(), button_id.value())),
      result1.GetCallback());
  ExpectOkResult(result1);

  ActResultFuture result2;
  actor_task().Act(ToRequestList(MakeWaitRequest()), result2.GetCallback());
  ExpectOkResult(result2);

  ActResultFuture result3;
  actor_task().Act(ToRequestList(MakeWaitRequest()), result3.GetCallback());
  ExpectOkResult(result3);

  actor_keyed_service()->ResetForTesting();

  histogram_tester.ExpectUniqueSample("Actor.Task.SubsequentWaits.Click",
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample("Actor.Task.SubsequentWaits.Wait",
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
  // The second wait is not counted towards the total.
  histogram_tester.ExpectUniqueSample(kActorTaskSubsequentWaitsMetricName,
                                      /*sample=*/1,
                                      /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace actor
