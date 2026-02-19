// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/glic/actor/glic_actor_functional_browsertest.h"
#include "content/public/test/browser_test.h"

namespace glic::actor {
namespace {

using ::base::test::ValueIs;
using ::optimization_guide::proto::Actions;
using ::optimization_guide::proto::ActionsResult;
using ::optimization_guide::proto::ClickAction;

class GlicActorContextFetchFunctionalBrowserTest
    : public GlicActorFunctionalBrowserTestBase {
 public:
  GlicActorContextFetchFunctionalBrowserTest() = default;
  ~GlicActorContextFetchFunctionalBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicActorContextFetchFunctionalBrowserTest,
                       RetryFailedContextFetchAfterPerformActions) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  ASSERT_NE(task_id, TaskId());

  // Perform a click action.
  Actions action =
      ::actor::MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                         ClickAction::LEFT, ClickAction::SINGLE, task_id);

  // Mock the context fetch so that the first time the TabObservationResult is a
  // failure. This should result in a retry which then succeeds.
  int num_calls = 0;
  ScopedMockTabObservationResult mock_result(base::BindLambdaForTesting(
      [&](TabObservation* observation, const FetchPageContextResult&) {
        ++num_calls;
        if (num_calls == 1) {
          observation->set_result(
              TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);
        } else {
          observation->set_result(TabObservation::TAB_OBSERVATION_OK);
          observation->set_annotated_page_content_result(
              TabObservation::ANNOTATED_PAGE_CONTENT_OK);
          observation->set_screenshot_result(TabObservation::SCREENSHOT_OK);
        }
      }));

  EXPECT_THAT(PerformActions(action),
              ValueIs(HasResultCode(::actor::mojom::ActionResultCode::kOk)));
  EXPECT_EQ(num_calls, 2);
}

IN_PROC_BROWSER_TEST_F(GlicActorContextFetchFunctionalBrowserTest,
                       FailedContextFetchOnlyRetriesOnce) {
  ASSERT_OK_AND_ASSIGN(TaskId task_id, CreateTask());
  ASSERT_NE(task_id, TaskId());

  // Perform a click action.
  Actions action =
      ::actor::MakeClick(active_tab()->GetHandle(), gfx::Point(1, 1),
                         ClickAction::LEFT, ClickAction::SINGLE, task_id);

  int num_calls = 0;
  ScopedMockTabObservationResult mock_result(base::BindLambdaForTesting(
      [&](TabObservation* observation, const FetchPageContextResult&) {
        ++num_calls;
        observation->set_result(
            TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);
      }));

  ActionsResult result = PerformActions(action).value();
  EXPECT_THAT(result, HasResultCode(::actor::mojom::ActionResultCode::kOk));
  ASSERT_EQ(result.tabs_size(), 1);
  ASSERT_TRUE(result.tabs().at(0).has_result());
  EXPECT_EQ(result.tabs().at(0).result(),
            TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE);

  EXPECT_EQ(num_calls, 2);
}

}  // namespace
}  // namespace glic::actor
