// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/actor/glic_actor_functional_browsertest.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace glic::actor {
namespace {

class GlicActorTabFunctionalBrowserTest
    : public GlicActorFunctionalBrowserTestBase,
      public ::testing::WithParamInterface<GURL> {
 public:
  GlicActorTabFunctionalBrowserTest() = default;
  ~GlicActorTabFunctionalBrowserTest() override = default;

  GURL GetInitiatorTabUrl() { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(GlicActorTabFunctionalBrowserTest, CreateActorTab) {
  // Navigate the current tab to the initiator URL.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetInitiatorTabUrl()));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  SessionID initiator_window_id = browser()->session_id();
  tabs::TabHandle initiator_tab = active_tab()->GetHandle();

  base::expected<TaskId, std::string> task_id = CreateTask();
  ASSERT_TRUE(task_id.has_value()) << task_id.error();

  // Create a new tab for the task.
  base::expected<tabs::TabHandle, std::string> new_tab_handler =
      CreateActorTab(task_id.value(), /*open_in_background=*/false,
                     base::ToString(initiator_tab.raw_value()),
                     base::ToString(initiator_window_id.id()));
  ASSERT_TRUE(new_tab_handler.has_value()) << new_tab_handler.error();

  // Verify it is bound to the task.
  EXPECT_TRUE(actor_keyed_service()
                  ->GetTask(task_id.value())
                  ->GetTabs()
                  .contains(new_tab_handler.value()));
}

IN_PROC_BROWSER_TEST_P(GlicActorTabFunctionalBrowserTest,
                       CreateActorTabWithInvalidTask) {
  // Navigate the current tab to the initiator URL.
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetInitiatorTabUrl()));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);
  SessionID initiator_window_id = browser()->session_id();
  tabs::TabHandle initiator_tab = active_tab()->GetHandle();

  base::expected<TaskId, std::string> task_id = CreateTask();
  ASSERT_TRUE(task_id.has_value()) << task_id.error();

  TaskId invalid_task_id = actor::TaskId(task_id.value().value() + 100);

  // Create a new tab with an invalid task id.
  base::expected<tabs::TabHandle, std::string> new_tab_handler =
      CreateActorTab(invalid_task_id, /*open_in_background=*/false,
                     base::ToString(initiator_tab.raw_value()),
                     base::ToString(initiator_window_id.id()));

  // CreateActorTab should have returned an error;
  EXPECT_FALSE(new_tab_handler.has_value());

  // Verify it is bound to the task.
  EXPECT_TRUE(
      actor_keyed_service()->GetTask(task_id.value())->GetTabs().empty());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GlicActorTabFunctionalBrowserTest,
    ::testing::Values(GURL(chrome::kChromeUINewTabURL),
                      GURL(url::kAboutBlankURL)));

}  // namespace
}  // namespace glic::actor
