// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
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

using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::StartsWith;

// Returns the value the script produced, or nullopt if it failed.
std::optional<std::string> GetResponse(const mojom::ActionResult& result) {
  if (!IsOk(result) || !result.execute_js_response) {
    return std::nullopt;
  }
  return result.execute_js_response->result;
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

// Blink reports synchronous exceptions to the console rather than to the tool,
// so the result doesn't include the exception's message.
IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, ExceptionHandled) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("throw new Error('test_error');");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsScriptFailed);
  EXPECT_FALSE(result->message.empty());
  EXPECT_EQ(GetJsErrorMessage(*result), std::nullopt);
}

IN_PROC_BROWSER_TEST_F(ActorToolsExecuteJsTest, SyntaxError) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GetTestPageUrl()));

  auto result = RunExecuteJsTool("2 +");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->code, mojom::ActionResultCode::kExecuteJsScriptFailed);
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

// Unlike a synchronous exception, an exception thrown in an async function
// rejects the promise it returns, so the error is reported.
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
}

}  // namespace actor
