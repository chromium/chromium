// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

class ActorClickToolInteractiveUiTest : public ActorToolsTest {
 public:
  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

// An actor click must not take focus from the browser UI.
IN_PROC_BROWSER_TEST_F(ActorClickToolInteractiveUiTest,
                       ClickDoesNotStealFocus) {
  const GURL url = embedded_https_test_server().GetURL(
      "example.com", "/actor/page_with_clickable_element.html");
  WaitForInitialWebUIToolbar(browser());
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // Use the browser command rather than focusing a views::View directly, since
  // the WebUI toolbar's omnibox is not a views::View with VIEW_ID_OMNIBOX.
  chrome::FocusLocationBar(browser());
  ui_test_utils::WaitForViewFocus(browser(), VIEW_ID_OMNIBOX, true);
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  std::optional<int> button_id =
      content::GetDOMNodeId(*main_frame(), "button#clickable");
  ASSERT_TRUE(button_id);
  std::unique_ptr<ToolRequest> action =
      MakeClickRequest(*main_frame(), button_id.value());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  // The WebUI omnibox reports its focus state over the toolbar's own Mojo
  // pipes, which are not ordered against the click IPC. Settle them so a focus
  // change caused by the click is observed before checking.
  if (GetWebUIToolbarWebView(browser())) {
    ASSERT_TRUE(FlushWebUIToolbarPipes(browser()));
  }
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
}

}  // namespace
}  // namespace actor
