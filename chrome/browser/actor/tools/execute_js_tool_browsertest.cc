// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/actor/tools/execute_js_tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace actor {

namespace {

using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::StartsWith;

// Returns the value the script produced, or nullopt if it failed.
std::optional<std::string> GetResponse(const mojom::ActionResult& result) {
  if (!IsOk(result) || !result.execute_js_response) {
    return std::nullopt;
  }
  return result.execute_js_response->result;
}

std::optional<base::Value> ParseJsonResponse(
    const mojom::ActionResult& result) {
  std::optional<std::string> response = GetResponse(result);
  if (!response) {
    return std::nullopt;
  }
  return base::JSONReader::Read(*response, base::JSON_PARSE_RFC);
}

std::optional<std::string> GetJsErrorMessage(
    const mojom::ActionResult& result) {
  if (!result.execute_js_response) {
    return std::nullopt;
  }
  return result.execute_js_response->js_error_message;
}

}  // namespace

class ActorToolsExecuteJsTest : public ActorToolsTest {
 public:
  ActorToolsExecuteJsTest() {
    features_.InitAndEnableFeature(features::kGlicActor);
  }

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  // Page actions on localhost are gated as sensitive (see
  // kGlicActorLocalhostIsSensitive), so act on an HTTPS origin with a
  // non-localhost host name instead.
  GURL GetTestPageUrl(std::string_view path = "/title1.html") {
    return embedded_https_test_server().GetURL("example.com", path);
  }

  // Returns a request to run `script_code` in the current document of `frame`,
  // or of the main frame if `frame` is null.
  std::unique_ptr<ToolRequest> MakeRequest(
      const std::string& script_code,
      uint32_t timeout_ms = 5000,
      content::RenderFrameHost* frame = nullptr) {
    auto document_id =
        optimization_guide::DocumentIdentifierUserData::
            GetOrCreateForCurrentDocument(frame ? frame : main_frame())
                ->token();
    return std::make_unique<ExecuteJsToolRequest>(
        ActorSurfaceHandle::From(active_tab()->GetHandle()), document_id,
        script_code, timeout_ms);
  }

  // Runs `request` and returns its result, or null if there isn't exactly one.
  mojom::ActionResultPtr Run(std::unique_ptr<ToolRequest> request) {
    ActResultFuture result;
    actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());
    const auto& action_results = result.Get();
    if (action_results.size() != 1u || !action_results[0].result) {
      ADD_FAILURE() << "Expected exactly one action result, got "
                    << action_results.size();
      return nullptr;
    }
    return action_results[0].result->Clone();
  }

  mojom::ActionResultPtr RunExecuteJsTool(const std::string& script_code,
                                          uint32_t timeout_ms = 5000) {
    return Run(MakeRequest(script_code, timeout_ms));
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, SimpleReturn) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("2 + 2");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetResponse(*result), "4");
  EXPECT_FALSE(result->execute_js_response->truncated);
}

// Scripts can change the page, so they don't opt out of observations, whether
// they succeed or fail.
IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, ObservationsRequested) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));
  const ActorSurfaceHandle surface =
      ActorSurfaceHandle::From(active_tab()->GetHandle());

  for (const char* script : {"2 + 2", "throw new Error('failure')"}) {
    SCOPED_TRACE(script);
    ActResultFuture result;
    actor_task().Act(ToRequestList(MakeRequest(script)), result.GetCallback());
    ASSERT_TRUE(result.Wait());
    EXPECT_EQ(result.GetStrategy().GetScreenshotPolicy(surface),
              ScreenshotPolicy::kRequested);
    EXPECT_EQ(result.GetStrategy().GetPageContentExtractionPolicy(surface),
              PageContentExtractionPolicy::kRequested);
  }
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, ClickButton) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  ASSERT_TRUE(
      content::ExecJs(web_contents(),
                      "const btn = document.createElement('button');"
                      "btn.id = 'test-button';"
                      "btn.textContent = 'Not Clicked';"
                      "btn.onclick = (e) => { btn.textContent = 'Clicked'; };"
                      "document.body.appendChild(btn);"));

  auto result =
      RunExecuteJsTool("document.getElementById('test-button').click();");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);

  // Expect the button text is changed by calling click().
  EXPECT_EQ(
      "Clicked",
      content::EvalJs(web_contents(),
                      "document.getElementById('test-button').textContent"));
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, ClickButtonNotTrusted) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "const btn = document.createElement('button');"
                              "btn.id = 'test-button';"
                              "btn.textContent = 'Not Clicked';"
                              "btn.onclick = (e) => { if (e.isTrusted) "
                              "{btn.textContent = 'Clicked';} };"
                              "document.body.appendChild(btn);"));

  auto result =
      RunExecuteJsTool("document.getElementById('test-button').click();");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);

  // Expect the button text is not changed by calling click() because
  // e.IsTrusted is false.
  EXPECT_EQ(
      "Not Clicked",
      content::EvalJs(web_contents(),
                      "document.getElementById('test-button').textContent"));
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, IsolatedWorldScope) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("window.actor_isolated_var = 'isolated';");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);

  // Verify variable is NOT present in main world
  EXPECT_EQ(base::Value(),
            content::EvalJs(web_contents(), "window.actor_isolated_var"));
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, ExceptionHandled) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("throw new Error('test_error');");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsScriptFailed);
  EXPECT_THAT(result->message, HasSubstr("test_error"));
  EXPECT_THAT(result->message, HasSubstr("line 1"));
  EXPECT_EQ(GetJsErrorMessage(*result), result->message);
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, SyncTypeErrorMessageAndLine) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("const x = null;\nx.foo;");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsScriptFailed);
  EXPECT_THAT(result->message, HasSubstr("TypeError"));
  EXPECT_THAT(result->message, HasSubstr("line 2"));
  EXPECT_EQ(GetJsErrorMessage(*result), result->message);
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, SyntaxError) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("2 +");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsScriptFailed);
  EXPECT_THAT(result->message, HasSubstr("SyntaxError"));
  EXPECT_THAT(result->message, HasSubstr("line 1"));
  EXPECT_EQ(GetJsErrorMessage(*result), result->message);
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, BareReturnAutoWrapped) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("const a = 1;\nreturn a + 2; // trailing");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetResponse(*result), "3");
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, BareAwaitAutoWrapped) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto expr_result = RunExecuteJsTool(
      "await new Promise(r => setTimeout(() => r('awaited_expr'), 50))");
  ASSERT_TRUE(expr_result);
  EXPECT_EQ(expr_result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetResponse(*expr_result), "\"awaited_expr\"");

  auto semi_result = RunExecuteJsTool(
      "await new Promise(r => setTimeout(() => r('awaited_semi'), 50));");
  ASSERT_TRUE(semi_result);
  EXPECT_EQ(semi_result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetResponse(*semi_result), "\"awaited_semi\"");

  auto stmt_result = RunExecuteJsTool(
      "const v = await new Promise(r => setTimeout(() => r('awaited_stmt'), "
      "50));\nreturn v;");
  ASSERT_TRUE(stmt_result);
  EXPECT_EQ(stmt_result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetResponse(*stmt_result), "\"awaited_stmt\"");
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, BareObjectLiteralAutoWrapped) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("{ a: 1, b: 2 }");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetResponse(*result), "{\"a\":1,\"b\":2}");

  auto semi_result = RunExecuteJsTool("{ a: 1, b: 2 };");
  ASSERT_TRUE(semi_result);
  EXPECT_EQ(semi_result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetResponse(*semi_result), "{\"a\":1,\"b\":2}");
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, BareReturnThrowsReportsError) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("const x = null;\nreturn x.foo;");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsScriptFailed);
  EXPECT_THAT(GetJsErrorMessage(*result), Optional(HasSubstr("TypeError")));
  EXPECT_EQ(GetJsErrorMessage(*result), result->message);
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest,
                       BareReturnWithInnerSyntaxError) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("return 1;\nconst x = 1; const x = 2;");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsScriptFailed);
  EXPECT_THAT(result->message, HasSubstr("has already been declared"));
  EXPECT_THAT(result->message, HasSubstr("line 2"));
  EXPECT_EQ(GetJsErrorMessage(*result), result->message);
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, TimeoutTriggered) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("while(true) {}", /*timeout_ms=*/100);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsTimeout);

  // Subsequent execution in the same frame should execute normally and not time
  // out.
  auto follow_up_result = RunExecuteJsTool("2 + 2");
  ASSERT_TRUE(follow_up_result);
  EXPECT_EQ(follow_up_result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetResponse(*follow_up_result), "4");
}

// Microtasks queued by the script run before script execution returns, so the
// timeout covers them too.
IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, TimeoutInMicrotask) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  // The script itself returns a value rather than a promise, so only the
  // microtask can make it time out.
  auto result = RunExecuteJsTool("queueMicrotask(() => { while (true) {} }); 1",
                                 /*timeout_ms=*/100);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsTimeout);

  auto follow_up_result = RunExecuteJsTool("2 + 2");
  ASSERT_TRUE(follow_up_result);
  EXPECT_EQ(GetResponse(*follow_up_result), "4");
}

// Converting the result to JSON runs page JavaScript, so it is bounded by the
// timeout too.
IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, TimeoutConvertingResult) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result =
      RunExecuteJsTool("({toJSON() { while (true) {} }})", /*timeout_ms=*/200);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsTimeout);

  auto follow_up_result = RunExecuteJsTool("2 + 2");
  ASSERT_TRUE(follow_up_result);
  EXPECT_EQ(GetResponse(*follow_up_result), "4");
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, TimeoutWaitingForPromise) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("new Promise(() => {})", /*timeout_ms=*/200);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsTimeout);
  // The renderer reports the timeout before the browser's own timer fires.
  EXPECT_THAT(result->message, HasSubstr("promise"));

  auto follow_up_result = RunExecuteJsTool("2 + 2");
  ASSERT_TRUE(follow_up_result);
  EXPECT_EQ(GetResponse(*follow_up_result), "4");
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, WaitForPromiseFulfilled) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool(
      "new Promise(r => setTimeout(() => r('async_result'), 50))");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetResponse(*result), "\"async_result\"");
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, WaitForPromiseRejected) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool(
      "new Promise((_, r) => "
      "setTimeout(() => r(new Error('async_fail')), 50))");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsScriptFailed);
  EXPECT_THAT(result->message, HasSubstr("async_fail"));
  EXPECT_EQ(GetJsErrorMessage(*result), result->message);
}

// An exception thrown in an async function rejects the promise it returns, so
// the error is reported.
IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, AsyncFunctionThrows) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result =
      RunExecuteJsTool("(async () => { throw new Error('thrown_error'); })()");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsScriptFailed);
  EXPECT_THAT(GetJsErrorMessage(*result), Optional(HasSubstr("thrown_error")));
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, AsyncFunctionWithAwait) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool(
      "(async () => { await new Promise(r => setTimeout(r, 50)); return "
      "'awaited_val'; })()");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetResponse(*result), "\"awaited_val\"");
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, LongResultTruncated) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool(
      base::StringPrintf("'x'.repeat(%zu)", kExecuteJsMaxResultBytes + 1));
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);
  ASSERT_TRUE(result->execute_js_response);
  EXPECT_TRUE(result->execute_js_response->truncated);
  EXPECT_EQ(result->execute_js_response->result.size(),
            kExecuteJsMaxResultBytes);
  EXPECT_THAT(result->execute_js_response->result, StartsWith("\"xxx"));
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, MultipleScriptsInOneAct) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  ActResultFuture result;
  actor_task().Act(ToRequestList(MakeRequest("1"), MakeRequest("'two'")),
                   result.GetCallback());
  const auto& action_results = result.Get();
  ASSERT_EQ(action_results.size(), 2u);
  EXPECT_EQ(GetResponse(*action_results[0].result), "1");
  EXPECT_EQ(GetResponse(*action_results[1].result), "\"two\"");
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, ScriptNavigatesPage) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  // Keep the script waiting so the navigation starts before it finishes.
  content::TestNavigationObserver navigation_observer(web_contents());
  auto result =
      RunExecuteJsTool("location.href = '/title2.html'; new Promise(() => {})");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsNavigated);

  // The tool in the old document was cancelled, so the frame accepts a new one
  // right away.
  navigation_observer.Wait();
  ASSERT_EQ(web_contents()->GetLastCommittedURL(),
            GetTestPageUrl("/title2.html"));
  auto follow_up_result = RunExecuteJsTool("2 + 2");
  ASSERT_TRUE(follow_up_result);
  EXPECT_EQ(GetResponse(*follow_up_result), "4");
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest,
                       StopTaskWhileWaitingForPromise) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  // The promise settles only when window.settle() is called in the actor's
  // isolated world.
  content::TitleWatcher title_watcher(web_contents(), u"waiting");
  ActResultFuture result;
  actor_task().Act(ToRequestList(MakeRequest("new Promise(resolve => {"
                                             "  window.settle = resolve;"
                                             "  document.title = 'waiting';"
                                             "})",
                                             /*timeout_ms=*/30000)),
                   result.GetCallback());
  ASSERT_EQ(title_watcher.WaitAndGetTitle(), u"waiting");

  actor_keyed_service().StopTask(task_id_,
                                 ActorTask::StoppedReason::kStoppedByUser);
  ExpectErrorResult(result, mojom::ActionResultCode::kTaskWentAway);

  // Settling the promise after the tool is gone is harmless.
  EXPECT_EQ(true, content::EvalJs(web_contents(), "window.settle(1); true",
                                  content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  ISOLATED_WORLD_ID_ACTOR));
  EXPECT_EQ(2, content::EvalJs(web_contents(), "1 + 1"));
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, SubframeDocumentRejected) {
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetTestPageUrl("/iframe.html")));
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(main_frame(), 0);
  ASSERT_TRUE(child_frame);

  auto result = Run(MakeRequest("2 + 2", /*timeout_ms=*/5000, child_frame));
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code,
            mojom::ActionResultCode::kExecuteJsTargetNotMainFrame);
}

// Pages can't stop the script with their Content Security Policy.
IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, StrictCsp) {
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      GetTestPageUrl(
          "/set-header?Content-Security-Policy: script-src 'none'")));

  auto result = RunExecuteJsTool("1 + 1");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kOk);
  EXPECT_EQ(GetResponse(*result), "2");

  // Result normalization should also succeed under a strict page CSP.
  auto body_result = RunExecuteJsTool("document.body");
  ASSERT_TRUE(body_result);
  ASSERT_EQ(body_result->code, mojom::ActionResultCode::kOk);
  std::optional<base::Value> parsed = ParseJsonResponse(*body_result);
  ASSERT_TRUE(parsed && parsed->is_dict());
  EXPECT_THAT(parsed->GetDict().FindString("tag"), Pointee(Eq("body")));
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, SerializeElement) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "document.body.innerHTML = `"
      "  <a id='nav-link' role='menuitem' aria-label='Go to docs'"
      "     href='/docs' style='display:inline-block;width:100px;height:40px;'>"
      "    Open   Docs"
      "  </a>"
      "  <input id='search-box' aria-label='Search' value='hello world'>"
      "  <input id='secret-box' type='password' value='s3cr3t'>"
      "  <input id='bare-checkbox' type='checkbox'>"
      "  <input id='valued-checkbox' type='checkbox' value='opt1'>"
      "  <button id='icon-btn' aria-label='Close'"
      "          style='width:24px;height:24px;'>"
      "    <svg><style>.st0{fill:#fff;}</style><path/></svg>"
      "  </button>"
      "  <svg width='20' height='20'>"
      "    <a id='svg-link' href='/svg-target'>"
      "      <rect width='20' height='20'/>"
      "    </a>"
      "  </svg>"
      "  <sp-menu-item id='custom-item' role='menuitem'>Settings</sp-menu-item>"
      "  <div id='hidden-el' style='display:none'>Hidden Text</div>"
      "  <form id='clobbered-form' role='form' aria-label='Edit Form'>"
      "    <input type='hidden' name='id' value='42'>"
      "    <input name='tagName'>"
      "    <input name='role'>"
      "    <input name='ariaLabel'>"
      "    <input name='getAttribute'>"
      "    <input name='getBoundingClientRect'>"
      "  </form>"
      "  <div id='shadow-host'></div>"
      "`;"
      "const shadow = document.getElementById('shadow-host')"
      "    .attachShadow({mode: 'open'});"
      "shadow.innerHTML = "
      "    '<button id=\"shadow-btn\">Inside Shadow</button>';"));

  // Link element with metadata and 0-999 center coordinates.
  {
    auto result = RunExecuteJsTool("document.getElementById('nav-link')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    const base::DictValue& dict = parsed->GetDict();
    EXPECT_THAT(dict.FindString("tag"), Pointee(Eq("a")));
    EXPECT_THAT(dict.FindString("id"), Pointee(Eq("nav-link")));
    EXPECT_THAT(dict.FindString("role"), Pointee(Eq("menuitem")));
    EXPECT_THAT(dict.FindString("ariaLabel"), Pointee(Eq("Go to docs")));
    EXPECT_THAT(dict.FindString("text"), Pointee(Eq("Open Docs")));
    EXPECT_THAT(dict.FindString("href"),
                Pointee(Eq(GetTestPageUrl("/docs").spec())));
    EXPECT_EQ(dict.Find("value"), nullptr);

    const base::DictValue* center = dict.FindDict("center_0_999");
    ASSERT_TRUE(center);
    int expected_x =
        content::EvalJs(
            web_contents(),
            "(() => {"
            "  const r = "
            "document.getElementById('nav-link').getBoundingClientRect();"
            "  return Math.round(((r.left + r.width / 2) / window.innerWidth) "
            "* 999);"
            "})()")
            .ExtractInt();
    int expected_y =
        content::EvalJs(
            web_contents(),
            "(() => {"
            "  const r = "
            "document.getElementById('nav-link').getBoundingClientRect();"
            "  return Math.round(((r.top + r.height / 2) / window.innerHeight) "
            "* 999);"
            "})()")
            .ExtractInt();
    EXPECT_EQ(center->FindInt("x"), expected_x);
    EXPECT_EQ(center->FindInt("y"), expected_y);
  }

  // Input element includes its value; password and bare checkbox inputs omit
  // value, while checkboxes with an explicit value attribute keep it.
  {
    auto result = RunExecuteJsTool("document.getElementById('search-box')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    const base::DictValue& dict = parsed->GetDict();
    EXPECT_THAT(dict.FindString("tag"), Pointee(Eq("input")));
    EXPECT_THAT(dict.FindString("id"), Pointee(Eq("search-box")));
    EXPECT_THAT(dict.FindString("ariaLabel"), Pointee(Eq("Search")));
    EXPECT_THAT(dict.FindString("value"), Pointee(Eq("hello world")));
    EXPECT_TRUE(dict.FindDict("center_0_999"));
  }
  {
    auto result = RunExecuteJsTool("document.getElementById('secret-box')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    EXPECT_EQ(parsed->GetDict().Find("value"), nullptr);
  }
  {
    auto result = RunExecuteJsTool("document.getElementById('bare-checkbox')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    EXPECT_EQ(parsed->GetDict().Find("value"), nullptr);
  }
  {
    auto result =
        RunExecuteJsTool("document.getElementById('valued-checkbox')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    EXPECT_THAT(parsed->GetDict().FindString("value"), Pointee(Eq("opt1")));
  }

  // Visible element containing only <svg><style> does not leak CSS into text.
  {
    auto result = RunExecuteJsTool("document.getElementById('icon-btn')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    EXPECT_THAT(parsed->GetDict().FindString("ariaLabel"),
                Pointee(Eq("Close")));
    EXPECT_EQ(parsed->GetDict().Find("text"), nullptr);
  }

  // SVG <a> falls back from SVGAnimatedString .href to the href attribute.
  {
    auto result = RunExecuteJsTool("document.getElementById('svg-link')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    EXPECT_THAT(parsed->GetDict().FindString("href"),
                Pointee(Eq("/svg-target")));
  }

  // Custom element queried via querySelectorAll returns serialized metadata.
  {
    auto result = RunExecuteJsTool("document.querySelectorAll('sp-menu-item')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_list());
    const base::ListValue& list = parsed->GetList();
    ASSERT_EQ(list.size(), 1u);
    ASSERT_TRUE(list[0].is_dict());
    const base::DictValue& dict = list[0].GetDict();
    EXPECT_THAT(dict.FindString("tag"), Pointee(Eq("sp-menu-item")));
    EXPECT_THAT(dict.FindString("id"), Pointee(Eq("custom-item")));
    EXPECT_THAT(dict.FindString("role"), Pointee(Eq("menuitem")));
    EXPECT_THAT(dict.FindString("text"), Pointee(Eq("Settings")));
    EXPECT_TRUE(dict.FindDict("center_0_999"));
  }

  // Open Shadow DOM element serializes metadata and coordinates.
  {
    auto result = RunExecuteJsTool(
        "document.getElementById('shadow-host').shadowRoot"
        "    .querySelector('button')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    const base::DictValue& dict = parsed->GetDict();
    EXPECT_THAT(dict.FindString("tag"), Pointee(Eq("button")));
    EXPECT_THAT(dict.FindString("id"), Pointee(Eq("shadow-btn")));
    EXPECT_THAT(dict.FindString("text"), Pointee(Eq("Inside Shadow")));
    EXPECT_TRUE(dict.FindDict("center_0_999"));
  }

  // Hidden element omits center_0_999 because it has zero layout size.
  {
    auto result = RunExecuteJsTool("document.getElementById('hidden-el')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    const base::DictValue& dict = parsed->GetDict();
    EXPECT_THAT(dict.FindString("tag"), Pointee(Eq("div")));
    EXPECT_THAT(dict.FindString("id"), Pointee(Eq("hidden-el")));
    EXPECT_THAT(dict.FindString("text"), Pointee(Eq("Hidden Text")));
    EXPECT_EQ(dict.Find("center_0_999"), nullptr);
  }

  // Form with named child controls that clobber DOM properties still
  // serializes its own Element metadata cleanly.
  {
    auto result = RunExecuteJsTool("document.getElementById('clobbered-form')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    const base::DictValue& dict = parsed->GetDict();
    EXPECT_THAT(dict.FindString("tag"), Pointee(Eq("form")));
    EXPECT_THAT(dict.FindString("id"), Pointee(Eq("clobbered-form")));
    EXPECT_THAT(dict.FindString("role"), Pointee(Eq("form")));
    EXPECT_THAT(dict.FindString("ariaLabel"), Pointee(Eq("Edit Form")));
  }

  // XHTML <input> redacts password and default checkbox values even though
  // tagName is lowercase in XML documents.
  {
    auto result = RunExecuteJsTool(
        "(() => {"
        "  const doc = document.implementation.createDocument("
        "      'http://www.w3.org/1999/xhtml', 'html', null);"
        "  const pw = doc.createElement('input');"
        "  pw.type = 'password';"
        "  pw.value = 's3cr3t';"
        "  const cb = doc.createElement('input');"
        "  cb.type = 'checkbox';"
        "  return { pw, cb };"
        "})()");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    const base::DictValue* pw = parsed->GetDict().FindDict("pw");
    ASSERT_TRUE(pw);
    EXPECT_THAT(pw->FindString("tag"), Pointee(Eq("input")));
    EXPECT_EQ(pw->Find("value"), nullptr);
    const base::DictValue* cb = parsed->GetDict().FindDict("cb");
    ASSERT_TRUE(cb);
    EXPECT_THAT(cb->FindString("tag"), Pointee(Eq("input")));
    EXPECT_EQ(cb->Find("value"), nullptr);
  }
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest,
                       SerializeNodeListAndHtmlCollection) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "const ul = document.createElement('ul');"
                              "for (let i = 0; i < 120; ++i) {"
                              "  const li = document.createElement('li');"
                              "  li.className = 'row-item';"
                              "  li.textContent = 'Item ' + i;"
                              "  ul.appendChild(li);"
                              "}"
                              "document.body.appendChild(ul);"));

  // NodeList capped at 100 items.
  {
    auto result = RunExecuteJsTool("document.querySelectorAll('li.row-item')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_list());
    const base::ListValue& list = parsed->GetList();
    ASSERT_EQ(list.size(), 100u);
    ASSERT_TRUE(list[0].is_dict());
    ASSERT_TRUE(list[99].is_dict());
    EXPECT_THAT(list[0].GetDict().FindString("tag"), Pointee(Eq("li")));
    EXPECT_THAT(list[0].GetDict().FindString("text"), Pointee(Eq("Item 0")));
    // HTMLLIElement has numeric .value = 0 by default; should not leak as
    // value.
    EXPECT_EQ(list[0].GetDict().Find("value"), nullptr);
    EXPECT_THAT(list[99].GetDict().FindString("text"), Pointee(Eq("Item 99")));
  }

  // HTMLCollection capped at 100 items.
  {
    auto result =
        RunExecuteJsTool("document.getElementsByClassName('row-item')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_list());
    EXPECT_EQ(parsed->GetList().size(), 100u);
  }

  // Empty NodeList serializes to [] instead of {}.
  {
    auto result = RunExecuteJsTool("document.querySelectorAll('.nonexistent')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    EXPECT_EQ(GetResponse(*result), "[]");
  }
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, SerializeErrorMapSetAndNested) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "const btn = document.createElement('button');"
                              "btn.id = 'ok-btn';"
                              "btn.textContent = 'OK';"
                              "document.body.appendChild(btn);"));

  // Error object.
  {
    auto result = RunExecuteJsTool("new TypeError('bad arg')");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    EXPECT_THAT(parsed->GetDict().FindString("name"), Pointee(Eq("TypeError")));
    EXPECT_THAT(parsed->GetDict().FindString("message"),
                Pointee(Eq("bad arg")));
  }

  // Top-level BigInt serializes as a lossless JSON string.
  {
    auto result = RunExecuteJsTool("12345678901234567890n");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    EXPECT_EQ(GetResponse(*result), "\"12345678901234567890\"");
  }

  // Nested Map, Set, Element, RegExp, Date, DOMRect, BigInt, boxed primitives,
  // throwing getters, and shared non-circular references.
  {
    auto result = RunExecuteJsTool(
        "(() => {"
        "  const btn = document.getElementById('ok-btn');"
        "  const shared = { k: 7 };"
        "  return {"
        "    map: new Map([['btn', btn], ['count', 2]]),"
        "    set: new Set(['a', btn]),"
        "    err: new Error('warn'),"
        "    re: /foo/gi,"
        "    big: 12345678901234567890n,"
        "    boxed: [new Number(0), new String(''), new Boolean(false), "
        "            Object(99n)],"
        "    date: new Date('2026-11-11T00:00:00.000Z'),"
        "    rect: btn.getBoundingClientRect(),"
        "    pair: [shared, shared],"
        "    get throwing() { throw new Error('getter'); }"
        "  };"
        "})()");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    const base::DictValue& dict = parsed->GetDict();

    const base::ListValue* map_list = dict.FindList("map");
    ASSERT_TRUE(map_list);
    ASSERT_EQ(map_list->size(), 2u);
    ASSERT_TRUE((*map_list)[0].is_list() &&
                (*map_list)[0].GetList().size() == 2u);
    ASSERT_TRUE((*map_list)[0].GetList()[1].is_dict());
    EXPECT_THAT((*map_list)[0].GetList()[0].GetIfString(), Pointee(Eq("btn")));
    EXPECT_THAT((*map_list)[0].GetList()[1].GetDict().FindString("id"),
                Pointee(Eq("ok-btn")));
    ASSERT_TRUE((*map_list)[1].is_list() &&
                (*map_list)[1].GetList().size() == 2u);
    EXPECT_THAT((*map_list)[1].GetList()[0].GetIfString(),
                Pointee(Eq("count")));
    EXPECT_EQ((*map_list)[1].GetList()[1].GetIfInt(), 2);

    const base::ListValue* set_list = dict.FindList("set");
    ASSERT_TRUE(set_list);
    ASSERT_EQ(set_list->size(), 2u);
    ASSERT_TRUE((*set_list)[1].is_dict());
    EXPECT_THAT((*set_list)[0].GetIfString(), Pointee(Eq("a")));
    EXPECT_THAT((*set_list)[1].GetDict().FindString("id"),
                Pointee(Eq("ok-btn")));

    const base::DictValue* err = dict.FindDict("err");
    ASSERT_TRUE(err);
    EXPECT_THAT(err->FindString("name"), Pointee(Eq("Error")));
    EXPECT_THAT(err->FindString("message"), Pointee(Eq("warn")));

    EXPECT_THAT(dict.FindString("re"), Pointee(Eq("/foo/gi")));
    EXPECT_THAT(dict.FindString("big"), Pointee(Eq("12345678901234567890")));
    const base::ListValue* boxed = dict.FindList("boxed");
    ASSERT_TRUE(boxed);
    ASSERT_EQ(boxed->size(), 4u);
    EXPECT_EQ((*boxed)[0].GetIfInt(), 0);
    EXPECT_THAT((*boxed)[1].GetIfString(), Pointee(Eq("")));
    EXPECT_EQ((*boxed)[2].GetIfBool(), false);
    EXPECT_THAT((*boxed)[3].GetIfString(), Pointee(Eq("99")));
    EXPECT_THAT(dict.FindString("date"),
                Pointee(Eq("2026-11-11T00:00:00.000Z")));
    const base::DictValue* rect = dict.FindDict("rect");
    ASSERT_TRUE(rect);
    EXPECT_TRUE(rect->FindDouble("width").has_value());

    const base::ListValue* pair = dict.FindList("pair");
    ASSERT_TRUE(pair);
    ASSERT_EQ(pair->size(), 2u);
    ASSERT_TRUE((*pair)[0].is_dict() && (*pair)[1].is_dict());
    EXPECT_EQ((*pair)[0].GetDict().FindInt("k"), 7);
    EXPECT_EQ((*pair)[1].GetDict().FindInt("k"), 7);

    EXPECT_THAT(dict.FindString("throwing"), Pointee(Eq("[Unserializable]")));
  }

  // Circular reference is handled cleanly instead of failing JSON.stringify.
  {
    auto result = RunExecuteJsTool(
        "(() => { const a = {x: 1}; a.self = a; return a; })()");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    EXPECT_EQ(parsed->GetDict().FindInt("x"), 1);
    EXPECT_THAT(parsed->GetDict().FindString("self"),
                Pointee(Eq("[Circular]")));
  }

  // Spoofed prototypes, Proxies, overridden array methods, throwing array
  // element getters, and huge sparse arrays do not abort normalization.
  {
    auto result = RunExecuteJsTool(
        "(() => {"
        "  const btn = document.getElementById('ok-btn');"
        "  const badArr = [1, 2];"
        "  Object.defineProperty(badArr, 1, "
        "      { enumerable: true, get() { throw new Error('boom'); } });"
        "  badArr.map = () => { throw new Error('map'); };"
        "  return {"
        "    btn,"
        "    fakeEl: Object.create(Element.prototype),"
        "    protoEl: HTMLElement.prototype,"
        "    protoNl: NodeList.prototype,"
        "    proxyEl: new Proxy(btn, {}),"
        "    spoofRe: { [Symbol.toStringTag]: 'RegExp', a: 1 },"
        "    spoofNl: { [Symbol.toStringTag]: 'NodeList', b: 2 },"
        "    badArr,"
        "    sparse: new Array(100000000)"
        "  };"
        "})()");
    ASSERT_TRUE(result);
    ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
    std::optional<base::Value> parsed = ParseJsonResponse(*result);
    ASSERT_TRUE(parsed && parsed->is_dict());
    const base::DictValue& dict = parsed->GetDict();
    const base::DictValue* btn = dict.FindDict("btn");
    ASSERT_TRUE(btn);
    EXPECT_THAT(btn->FindString("id"), Pointee(Eq("ok-btn")));
    const base::DictValue* spoof_re = dict.FindDict("spoofRe");
    ASSERT_TRUE(spoof_re);
    EXPECT_EQ(spoof_re->FindInt("a"), 1);
    const base::DictValue* spoof_nl = dict.FindDict("spoofNl");
    ASSERT_TRUE(spoof_nl);
    EXPECT_EQ(spoof_nl->FindInt("b"), 2);
    const base::ListValue* bad_arr = dict.FindList("badArr");
    ASSERT_TRUE(bad_arr);
    ASSERT_EQ(bad_arr->size(), 2u);
    EXPECT_EQ((*bad_arr)[0].GetIfInt(), 1);
    EXPECT_THAT((*bad_arr)[1].GetIfString(), Pointee(Eq("[Unserializable]")));
    const base::ListValue* sparse = dict.FindList("sparse");
    ASSERT_TRUE(sparse);
    ASSERT_FALSE(sparse->empty());
    EXPECT_THAT(sparse->back().GetIfString(), Pointee(Eq("[Truncated]")));
  }

  // Infinite loop inside a getter during normalization is bounded by timeout.
  {
    auto result = RunExecuteJsTool("({get hang() { while (true) {} }})",
                                   /*timeout_ms=*/200);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsTimeout);

    auto follow_up = RunExecuteJsTool("2 + 2");
    ASSERT_TRUE(follow_up);
    EXPECT_EQ(GetResponse(*follow_up), "4");
  }
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, SerializeElementInPromise) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  ASSERT_TRUE(content::ExecJs(web_contents(),
                              "const btn = document.createElement('button');"
                              "btn.id = 'async-btn';"
                              "btn.textContent = 'Async';"
                              "document.body.appendChild(btn);"));

  auto result = RunExecuteJsTool(
      "new Promise(r => setTimeout(() => "
      "r(document.getElementById('async-btn')), 20))");
  ASSERT_TRUE(result);
  ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
  std::optional<base::Value> parsed = ParseJsonResponse(*result);
  ASSERT_TRUE(parsed && parsed->is_dict());
  EXPECT_THAT(parsed->GetDict().FindString("tag"), Pointee(Eq("button")));
  EXPECT_THAT(parsed->GetDict().FindString("id"), Pointee(Eq("async-btn")));
  EXPECT_THAT(parsed->GetDict().FindString("text"), Pointee(Eq("Async")));
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest,
                       SerializeSameOriginIframeElement) {
  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GetTestPageUrl("/iframe.html")));

  ASSERT_TRUE(content::ExecJs(
      web_contents(),
      "const iframe = document.querySelector('iframe');"
      "iframe.style.position = 'fixed';"
      "iframe.style.left = '100px';"
      "iframe.style.top = '80px';"
      "iframe.style.border = '5px solid black';"
      "iframe.style.padding = '10px';"
      "const btn = iframe.contentDocument.createElement('button');"
      "btn.id = 'inner-btn';"
      "btn.style.position = 'fixed';"
      "btn.style.left = '20px';"
      "btn.style.top = '10px';"
      "btn.style.width = '40px';"
      "btn.style.height = '20px';"
      "btn.textContent = 'Inner';"
      "iframe.contentDocument.body.appendChild(btn);"));

  auto result = RunExecuteJsTool(
      "document.querySelector('iframe').contentDocument"
      "    .getElementById('inner-btn')");
  ASSERT_TRUE(result);
  ASSERT_EQ(result->code, mojom::ActionResultCode::kOk);
  std::optional<base::Value> parsed = ParseJsonResponse(*result);
  ASSERT_TRUE(parsed && parsed->is_dict());
  const base::DictValue& dict = parsed->GetDict();
  EXPECT_THAT(dict.FindString("tag"), Pointee(Eq("button")));
  EXPECT_THAT(dict.FindString("id"), Pointee(Eq("inner-btn")));
  EXPECT_THAT(dict.FindString("text"), Pointee(Eq("Inner")));

  const base::DictValue* center = dict.FindDict("center_0_999");
  ASSERT_TRUE(center);
  // 100 (iframe left) + 5 (border) + 10 (padding) + 20 (btn left) + 20 (w/2)
  // = 155; 80 + 5 + 10 + 10 + 10 = 115.
  int expected_x =
      content::EvalJs(web_contents(),
                      "Math.round((155 / window.innerWidth) * 999)")
          .ExtractInt();
  int expected_y =
      content::EvalJs(web_contents(),
                      "Math.round((115 / window.innerHeight) * 999)")
          .ExtractInt();
  EXPECT_EQ(center->FindInt("x"), expected_x);
  EXPECT_EQ(center->FindInt("y"), expected_y);
}

}  // namespace actor
