// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic_actor_interactive_uitest_common.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/rect.h"

namespace glic::test {

namespace {

namespace apc = ::optimization_guide::proto;
using apc::Actions;
using apc::SelectAction;

class GlicActorSelectToolUiTest : public GlicActorUiTest {
 public:
  MultiStep SelectAction(std::string_view select_label,
                         std::string_view option_value,
                         ExpectedErrorResult expected_result = {});

  MultiStep CheckElementValueAttribute(ui::ElementIdentifier tab_id,
                                       std::string_view element_id_str,
                                       std::string_view expected_value);

  MultiStep CheckElementIsOnscreen(ui::ElementIdentifier tab_id,
                                   std::string_view element_id_str,
                                   bool expected_onscreen);
};

GlicActorSelectToolUiTest::MultiStep GlicActorSelectToolUiTest::SelectAction(
    std::string_view select_label,
    std::string_view option_value,
    ExpectedErrorResult expected_result) {
  auto select_provider =
      base::BindLambdaForTesting([this, select_label, option_value]() {
        int32_t node_id = SearchAnnotatedPageContent(select_label);
        content::RenderFrameHost* frame =
            tab_handle_.Get()->GetContents()->GetPrimaryMainFrame();

        Actions action = actor::MakeSelect(*frame, node_id, option_value);
        action.set_task_id(task_id_.value());
        return EncodeActionProto(action);
      });
  return ExecuteAction(std::move(select_provider), std::move(expected_result));
}

GlicActorSelectToolUiTest::MultiStep
GlicActorSelectToolUiTest::CheckElementValueAttribute(
    ui::ElementIdentifier tab_id,
    std::string_view element_id_str,
    std::string_view expected_value) {
  return WaitForJsResult(
      tab_id,
      base::StringPrintf("() => document.getElementById('%s').value",
                         std::string(element_id_str).c_str()),
      expected_value);
}

// Checks if the element's bounding client rect is fully contained within the
// viewport.
GlicActorSelectToolUiTest::MultiStep
GlicActorSelectToolUiTest::CheckElementIsOnscreen(
    ui::ElementIdentifier tab_id,
    std::string_view element_id_str,
    bool expected_onscreen) {
  const std::string js_snippet = base::StringPrintf(
      R"js(
        () => {
          const elem = document.getElementById('%s');
          if (!elem) return 'false';
          const rect = elem.getBoundingClientRect();
          const isOnscreen = rect.top >= 0 && rect.left >= 0 &&
                 rect.bottom <= window.innerHeight &&
                 rect.right <= window.innerWidth;
          return isOnscreen.toString();
        }
      )js",
      std::string(element_id_str).c_str());

  return WaitForJsResult(tab_id, js_snippet,
                         expected_onscreen ? "true" : "false");
}

// Test that the SelectAction can select an ordinary <option> in a <select>
// element by both its text content and its 'value' attribute.
IN_PROC_BROWSER_TEST_F(GlicActorSelectToolUiTest, SelectActionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTestTabId),
      GetPageContextForActorTab(),

      // Verify initial state.
      CheckElementValueAttribute(kTestTabId, "plainSelect", "alpha"),

      // Select "beta" by its value, which is implicitly its text content.
      SelectAction("plain-select", "beta"),

      // Verify new state.
      CheckElementValueAttribute(kTestTabId, "plainSelect", "beta"),

      // Select the option with text "omega" by its explicit value "last".
      SelectAction("plain-select", "last"),

      // Verify new state.
      CheckElementValueAttribute(kTestTabId, "plainSelect", "last"),

      // Attempting to select by text content "omega" should fail, as its
      // explicit value "last" must be used instead. The tool will not find an
      // option with the value "omega".
      SelectAction("plain-select", "omega",
                   actor::mojom::ActionResultCode::kSelectNoSuchOption),

      // Verify the selection has not changed from the previous successful step.
      CheckElementValueAttribute(kTestTabId, "plainSelect", "last"));
  // clang-format on
}

// Test that options within <optgroup> elements can be selected.
IN_PROC_BROWSER_TEST_F(GlicActorSelectToolUiTest,
                       SelectActionGroupedOptionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTestTabId),
      GetPageContextForActorTab(),
      // Verify initial state.
      CheckElementValueAttribute(kTestTabId, "groupedSelect", "alpha"),

      // Select an option from the second (Latin) optgroup.
      SelectAction("grouped-select", "b"),

      // Verify new state.
      CheckElementValueAttribute(kTestTabId, "groupedSelect", "b"));
  // clang-format on
}

// Test that an option can be selected in a <select> element rendered as a
// listbox.
IN_PROC_BROWSER_TEST_F(GlicActorSelectToolUiTest,
                       SelectActionListboxOptionSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTestTabId),
      GetPageContextForActorTab(),
      // Listbox starts with no value.
      CheckElementValueAttribute(kTestTabId, "listboxSelect", ""),

      // Select "beta".
      SelectAction("listbox-select", "beta"),

      // Verify new state.
      CheckElementValueAttribute(kTestTabId, "listboxSelect", "beta"));
  // clang-format on
}

// Test that selecting an option in an off-screen <select> element succeeds.
// (The underlying tool is responsible for scrolling it into view).
IN_PROC_BROWSER_TEST_F(GlicActorSelectToolUiTest,
                       SelectActionOffscreenSucceeds) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTestTabId),
      GetPageContextForActorTab(),
      // Verify initial state.
      CheckElementValueAttribute(kTestTabId, "offscreenSelect", "alpha"),

      // Verify the element starts off-screen.
      CheckElementIsOnscreen(kTestTabId, "offscreenSelect",
                             /*expected_onscreen=*/false),

      // Select the option. This should scroll the element into view before
      // interacting with it.
      SelectAction("offscreen-select", "beta"),

      // Verify the element is now on-screen.
      CheckElementIsOnscreen(kTestTabId, "offscreenSelect",
                             /*expected_onscreen=*/true),

      // Verify new state.
      CheckElementValueAttribute(kTestTabId, "offscreenSelect", "beta"));
  // clang-format on
}

// Test that the SelectTool correctly fires 'input' and 'change' events.
IN_PROC_BROWSER_TEST_F(GlicActorSelectToolUiTest, SelectActionFiresEvents) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTestTabId), GetPageContextForActorTab(),
      // Event log should be empty initially.
      WaitForJsResult(kTestTabId, "() => select_event_log.join(',')", ""),

      // Perform the action.
      SelectAction("plain-select", "beta"),

      // Check that the JS listeners for 'input' and 'change' both fired.
      WaitForJsResult(kTestTabId, "() => select_event_log.join(',')",
                      "input,change"));
  // clang-format on
}

// Test that matching option values is case-sensitive and fails on mismatch.
IN_PROC_BROWSER_TEST_F(GlicActorSelectToolUiTest,
                       SelectActionValueIsCaseSensitive) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTestTabId),
      GetPageContextForActorTab(),
      // Attempt to select "BETA" which does not match the option "beta".
      SelectAction("plain-select", "BETA",
                   actor::mojom::ActionResultCode::kSelectNoSuchOption));
  // clang-format on
}

// Test that attempting to select a value that does not exist fails.
IN_PROC_BROWSER_TEST_F(GlicActorSelectToolUiTest,
                       SelectActionNonExistentValueFails) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTestTabId),
      GetPageContextForActorTab(),

      // Verify initial state.
      CheckElementValueAttribute(kTestTabId, "plainSelect", "alpha"),

      // Attempt to select a value that doesn't exist in any option.
      SelectAction("plain-select", "nonexistentValue",
                   actor::mojom::ActionResultCode::kSelectNoSuchOption));
  // clang-format on
}

// Test that attempting to select a disabled <option> fails.
IN_PROC_BROWSER_TEST_F(GlicActorSelectToolUiTest,
                       SelectActionDisabledOptionFails) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTestTabId),
      GetPageContextForActorTab(),
      // Verify initial state.
      CheckElementValueAttribute(kTestTabId, "plainSelect", "alpha"),

      // Attempt to select the option that has the 'disabled' attribute.
      SelectAction("plain-select", "disabledOption",
                   actor::mojom::ActionResultCode::kSelectOptionDisabled));
  // clang-format on
}

// Test that attempting to select any option in a disabled <select> element
// fails.
IN_PROC_BROWSER_TEST_F(GlicActorSelectToolUiTest,
                       SelectActionDisabledSelectFails) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTestTabId),
      GetPageContextForActorTab(),
      // Attempt to select a valid option within a select that is disabled.
      SelectAction("disabled-select", "beta",
                   actor::mojom::ActionResultCode::kElementDisabled));
  // clang-format on
}

// Test that attempting to select an <option> in a disabled <optgroup> fails.
IN_PROC_BROWSER_TEST_F(GlicActorSelectToolUiTest,
                       SelectActionDisabledOptGroupFails) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTestTabId),
      GetPageContextForActorTab(),
      // Attempt to select "foobar", which is in a disabled optgroup.
      SelectAction("grouped-select", "foobar",
                   actor::mojom::ActionResultCode::kSelectOptionDisabled));
  // clang-format on
}

// Test that attempting to select a value that matches a non-<option> child
// (like a <span> or <button>) fails.
IN_PROC_BROWSER_TEST_F(GlicActorSelectToolUiTest,
                       SelectActionNonOptionNodeValueFails) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestTabId);
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/select_tool.html");

  RunTestSequence(
      // clang-format off
      InitializeWithOpenGlicWindow(),
      StartActorTaskInNewTab(task_url, kTestTabId),
      GetPageContextForActorTab(),
      // Verify initial state.
      CheckElementValueAttribute(kTestTabId, "nonOptionsSelect", "alpha"),

      // Attempt to select "beta", which is the value of a <span>, not an
      // <option>.
      SelectAction("non-options-select", "beta",
                   actor::mojom::ActionResultCode::kSelectNoSuchOption),

      // Attempt to select "gamma", which is the value of a <button>, not an
      // <option>.
      SelectAction("non-options-select", "gamma",
                   actor::mojom::ActionResultCode::kSelectNoSuchOption),

      // Verify value remains unchanged.
      CheckElementValueAttribute(kTestTabId, "nonOptionsSelect", "alpha"),

      // NOW, select "epsilon", which exists as both a button AND an option.
      // This should SUCCEED because the tool only looks at options.
      SelectAction("non-options-select", "epsilon"),

      // Verify the value changed correctly.
      CheckElementValueAttribute(kTestTabId, "nonOptionsSelect", "epsilon"));
  // clang-format on
}

}  // namespace

}  // namespace glic::test
