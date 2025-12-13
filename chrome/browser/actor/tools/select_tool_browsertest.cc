// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using base::test::TestFuture;
using content::EvalJs;
using content::GetDOMNodeId;

namespace actor {

namespace {

std::string GetSelectElementCurrentValue(content::WebContents* web_contents,
                                         std::string_view query_selector) {
  return content::EvalJs(web_contents,
                         content::JsReplace("document.querySelector($1).value",
                                            query_selector))
      .ExtractString();
}

class ActorSelectToolBrowserTest : public ActorToolsTest {
 public:
  ActorSelectToolBrowserTest() = default;
  ~ActorSelectToolBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
};

// Test that the SelectTool can select an ordinary <option> in a <select>
// element.
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest, SelectTool_OptionSelected) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string plain_select_id = "#plainSelect";
  const int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), plain_select_id).value();

  ASSERT_EQ(GetSelectElementCurrentValue(web_contents(), plain_select_id),
            "alpha");

  {
    std::unique_ptr<ToolRequest> action =
        MakeSelectRequest(*main_frame(), plain_select_dom_node_id, "beta");
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), plain_select_id),
            "beta");

  {
    std::unique_ptr<ToolRequest> action =
        MakeSelectRequest(*main_frame(), plain_select_dom_node_id, "gamma");
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());

    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), plain_select_id),
            "gamma");

  // Test selecting by value. The option with value last has text "omega".
  {
    std::unique_ptr<ToolRequest> action =
        MakeSelectRequest(*main_frame(), plain_select_dom_node_id, "last");
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());

    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), plain_select_id),
            "last");
}

// Test that attempting to select in an offscreen <select> succeeds.
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest, SelectTool_Offscreen) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Page starts unscrolled.
  ASSERT_EQ(0, EvalJs(web_contents(), "window.scrollY"));

  const std::string offscreen_select_id = "#offscreenSelect";
  int32_t offscreen_select_dom_node_id =
      GetDOMNodeId(*main_frame(), offscreen_select_id).value();

  const std::string initial_value =
      GetSelectElementCurrentValue(web_contents(), offscreen_select_id);
  ASSERT_EQ(initial_value, "alpha");
  const std::string new_value = "beta";

  std::unique_ptr<ToolRequest> action =
      MakeSelectRequest(*main_frame(), offscreen_select_dom_node_id, new_value);
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_GT(EvalJs(web_contents(), "window.scrollY"), 0);
  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), offscreen_select_id),
            new_value);
}

// Test that the SelectTool causes the change and input events to fire on the
// <select> element.
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest, SelectTool_Events) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string plain_select_id = "#plainSelect";
  const int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), plain_select_id).value();

  ASSERT_EQ(GetSelectElementCurrentValue(web_contents(), plain_select_id),
            "alpha");
  ASSERT_EQ("", EvalJs(web_contents(), "select_event_log.join(',')"));

  {
    std::unique_ptr<ToolRequest> action =
        MakeSelectRequest(*main_frame(), plain_select_dom_node_id, "beta");
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ("input,change",
              EvalJs(web_contents(), "select_event_log.join(',')"));
  }
}

// Test that attempting to select a value that does not exist in the <option>
// list fails and does not change the current selection.
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest,
                       SelectTool_NonExistentValueFails) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string plain_select_id = "#plainSelect";
  int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), plain_select_id).value();

  const std::string initial_value =
      GetSelectElementCurrentValue(web_contents(), plain_select_id);
  ASSERT_EQ(initial_value, "alpha");

  std::unique_ptr<ToolRequest> action = MakeSelectRequest(
      *main_frame(), plain_select_dom_node_id, "nonexistentValue");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kSelectNoSuchOption);

  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), plain_select_id),
            initial_value);
}

// Test that attempting to select a value corresponding to a non-<option>
// element fails. The select tool should only target valid options.
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest,
                       SelectTool_NonOptionNodeValueFails) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string non_options_select_id = "#nonOptionsSelect";
  int32_t non_options_select_dom_node_id =
      GetDOMNodeId(*main_frame(), non_options_select_id).value();

  const std::string initial_value =
      GetSelectElementCurrentValue(web_contents(), non_options_select_id);
  ASSERT_EQ(initial_value, "alpha");

  // Attempt to select "beta", which is the text of a <span>, not an <option>
  // value.  Expect the action to fail.
  {
    std::unique_ptr<ToolRequest> action = MakeSelectRequest(
        *main_frame(), non_options_select_dom_node_id, "beta");
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kSelectNoSuchOption);
  }

  // Expect the value to remain unchanged
  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), non_options_select_id),
            initial_value);

  // Attempt to select "gamma", which is the value property of a <button>
  // element, not an <option> value.  Expect the action to fail.
  {
    std::unique_ptr<ToolRequest> action = MakeSelectRequest(
        *main_frame(), non_options_select_dom_node_id, "gamma");
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kSelectNoSuchOption);
  }

  // Expect the value to remain unchanged
  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), non_options_select_id),
            initial_value);

  // Attempt to select "epsilon". This should succeed as there is an <option>
  // with value epsilon, despite there also being a <button> with value
  // "epsilon".
  {
    std::unique_ptr<ToolRequest> action = MakeSelectRequest(
        *main_frame(), non_options_select_dom_node_id, "epsilon");
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
    EXPECT_EQ(
        GetSelectElementCurrentValue(web_contents(), non_options_select_id),
        "epsilon");
  }
}

// Test that matching option values is case-sensitive.
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest,
                       SelectTool_ValueIsCaseSensitive) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string plain_select_id = "#plainSelect";
  int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), plain_select_id).value();
  const std::string initial_value =
      GetSelectElementCurrentValue(web_contents(), plain_select_id);

  ASSERT_EQ(initial_value, "alpha");

  // Attempt to select "BETA" which has different casing than the option "beta"
  // Expect the action to fail due to case mismatch.
  std::unique_ptr<ToolRequest> action =
      MakeSelectRequest(*main_frame(), plain_select_dom_node_id, "BETA");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kSelectNoSuchOption);

  // The select value should be unchanged.
  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), plain_select_id),
            initial_value);
}

// Test that attempting to select a disabled <option> fails.
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest,
                       SelectTool_DisabledOptionFails) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string plain_select_id = "#plainSelect";
  int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), plain_select_id).value();
  const std::string initial_value =
      GetSelectElementCurrentValue(web_contents(), plain_select_id);

  ASSERT_EQ(initial_value, "alpha");

  // Attempt to select the value of the disabled option. Expect the action to
  // fail and the select's value to be unchanged.
  std::unique_ptr<ToolRequest> action = MakeSelectRequest(
      *main_frame(), plain_select_dom_node_id, "disabledOption");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kSelectOptionDisabled);
  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), plain_select_id),
            initial_value);
}

// Test that attempting to select a <option> in a disabled <optgroup> fails.
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest,
                       SelectTool_DisabledOptGroupFails) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string group_select_id = "#groupedSelect";
  int32_t plain_select_dom_node_id =
      GetDOMNodeId(*main_frame(), group_select_id).value();
  const std::string initial_value =
      GetSelectElementCurrentValue(web_contents(), group_select_id);

  ASSERT_EQ(initial_value, "alpha");

  // Attempt to select the option with value "foobar". The option itself is
  // enabled but is in a disabled optgroup. Expect the action to fail and the
  // select's value to be unchanged.
  std::unique_ptr<ToolRequest> action =
      MakeSelectRequest(*main_frame(), plain_select_dom_node_id, "foobar");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kSelectOptionDisabled);
  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), group_select_id),
            initial_value);
}

// Test that attempting to select any option in a disabled <select> element
// fails.
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest,
                       SelectTool_DisabledSelectFails) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string disabled_select_id = "#disabledSelect";
  int32_t disabled_select_dom_node_id =
      GetDOMNodeId(*main_frame(), disabled_select_id).value();
  const std::string initial_value =
      GetSelectElementCurrentValue(web_contents(), disabled_select_id);

  ASSERT_EQ(initial_value, "alpha");

  // Attempt to select an otherwise valid option value ("beta"). Expect the
  // action to fail without affecting the <select>.
  std::unique_ptr<ToolRequest> action =
      MakeSelectRequest(*main_frame(), disabled_select_dom_node_id, "beta");
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kElementDisabled);
  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), disabled_select_id),
            initial_value);
}

// Test that options within <optgroup> elements can be selected.
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest,
                       SelectTool_GroupedOptionSelected) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string grouped_select_id = "#groupedSelect";
  int32_t grouped_select_dom_node_id =
      GetDOMNodeId(*main_frame(), grouped_select_id).value();

  ASSERT_EQ(GetSelectElementCurrentValue(web_contents(), grouped_select_id),
            "alpha");

  // Select an option from the first group
  {
    std::unique_ptr<ToolRequest> action =
        MakeSelectRequest(*main_frame(), grouped_select_dom_node_id, "gamma");
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), grouped_select_id),
            "gamma");

  // Select an option from the second group
  {
    std::unique_ptr<ToolRequest> action =
        MakeSelectRequest(*main_frame(), grouped_select_dom_node_id, "b");
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), grouped_select_id),
            "b");
}

// Test that an option can be selected in a <select> element rendered as a
// listbox (size attribute > 1).
IN_PROC_BROWSER_TEST_F(ActorSelectToolBrowserTest,
                       SelectTool_ListboxOptionSelected) {
  const GURL url = embedded_test_server()->GetURL("/actor/select_tool.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const std::string listbox_select_id = "#listboxSelect";
  int32_t listbox_select_dom_node_id =
      GetDOMNodeId(*main_frame(), listbox_select_id).value();

  // List box starts with no element selected.
  ASSERT_EQ(GetSelectElementCurrentValue(web_contents(), listbox_select_id),
            "");

  {
    std::unique_ptr<ToolRequest> action =
        MakeSelectRequest(*main_frame(), listbox_select_dom_node_id, "beta");
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), listbox_select_id),
            "beta");

  {
    std::unique_ptr<ToolRequest> action =
        MakeSelectRequest(*main_frame(), listbox_select_dom_node_id, "delta");
    ActResultFuture result;
    actor_task().Act(ToRequestList(action), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_EQ(GetSelectElementCurrentValue(web_contents(), listbox_select_id),
            "delta");
}

}  // namespace
}  // namespace actor
