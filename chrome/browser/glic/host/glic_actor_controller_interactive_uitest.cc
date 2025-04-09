// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/base64.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_identifier.h"

namespace glic::test {

namespace {

using optimization_guide::proto::BrowserAction;
using optimization_guide::proto::ClickAction;

// TODO(https://crbug.com/402086021): Get the actual target details for the
// button in the test page.
constexpr int32_t kContentNodeId = 123;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

class GlicActorControllerUiTest : public test::InteractiveGlicTest {
 public:
  GlicActorControllerUiTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicActor);
  }
  ~GlicActorControllerUiTest() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        actor::kActorTestDataPath);
    test::InteractiveGlicTest::SetUpOnMainThread();
  }

  // Calls actInFocusedTab() and waits until the promise resolves and succeeds.
  // TODO(https://crbug.com/402086021): Change script to just return the promise
  // instead of waiting. This needs to be done when changing ActionSucceeds test
  // to explicitly check that the action worked in the page.
  auto ExecuteAction(std::string_view encodedActionProto,
                     base::Value::Dict contextOptions) {
    return Steps(CheckJsResult(
        kGlicContentsElementId,
        content::JsReplace(R"js(
                        async () => {
                          const base64ToArrayBuffer = (base64) => {
                            const bytes = window.atob(base64);
                            const len = bytes.length;
                            const ret = new Uint8Array(len);
                            for (var i = 0; i < len; i++) {
                              ret[i] = bytes.charCodeAt(i);
                            }
                            return ret.buffer;
                          }
                          // As in TODO above, remove the async from the
                          // function and return the promise directly
                          // return client.browser.actInFocusedTab({
                          await client.browser.actInFocusedTab({
                            actionProto: base64ToArrayBuffer($1),
                            tabContextOptions: $2
                          });
                          return true;
                        }
                      )js",
                           encodedActionProto, std::move(contextOptions))));
  }

  // Calls actInFocusedTab() and waits until the promise rejects with an error.
  // Note: This will fail the test if the promise succeeds.
  auto ExecuteActionExpectingError(
      std::string_view encodedActionProto,
      base::Value::Dict contextOptions,
      glic::mojom::ActInFocusedTabErrorReason error_reason) {
    return Steps(CheckJsResult(
        kGlicContentsElementId,
        content::JsReplace(R"js(
                        async () => {
                          const base64ToArrayBuffer = (base64) => {
                            const bytes = window.atob(base64);
                            const len = bytes.length;
                            const ret = new Uint8Array(len);
                            for (var i = 0; i < len; i++) {
                              ret[i] = bytes.charCodeAt(i);
                            }
                            return ret.buffer;
                          }
                          try {
                            await client.browser.actInFocusedTab({
                              actionProto: base64ToArrayBuffer($1),
                              tabContextOptions: $2
                            });
                          } catch (err) {
                            return err.reason;
                          }
                        }
                      )js",
                           encodedActionProto, std::move(contextOptions)),
        ::testing::Eq(static_cast<int>(error_reason))));
  }

  std::string EncodeActionProto(const BrowserAction& action) {
    std::string serialized_message = action.SerializeAsString();
    return base::Base64Encode(serialized_message);
  }

  // Gets the context options to capture a new observation after completing an
  // action. This includes both annotations (i.e. AnnotatedPageContent) and a
  // screenshot.
  base::Value::Dict UpdatedContextOptions() {
    return base::Value::Dict()
        .Set("annotatedPageContent", true)
        .Set("viewportScreenshot", true);
  }

  // Gets the context options to capture a new observation that only has the
  // annotations (AnnotatedPageContent). Taking screenshots can be slow or flaky
  // in the test. This is intended to be used for interim steps, *before*
  // returning the final context of an action.
  base::Value::Dict AnnotationsOnlyContextOptions() {
    return base::Value::Dict()
        .Set("annotatedPageContent", true)
        .Set("viewportScreenshot", false);
  }

  // Starts a new task by executing the initial navigate to `task_url` to create
  // a new tab. The new tab can then be referenced by `kNewActorTabId`.
  auto StartTaskInNewTab(const GURL& task_url) {
    const GURL start_url = embedded_test_server()->GetURL("/blank.html?start");
    std::string startNavigateProto =
        EncodeActionProto(actor::MakeNavigate(task_url.spec()));

    return Steps(
        InstrumentTab(kActiveTabId),
        NavigateWebContents(kActiveTabId, start_url),
        OpenGlicWindow(GlicWindowMode::kAttached),
        InstrumentNextTab(kNewActorTabId),
        ExecuteAction(startNavigateProto, AnnotationsOnlyContextOptions()),
        WaitForWebContentsReady(kNewActorTabId, task_url));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, OpensNewTabOnFirstNavigate) {
  const GURL start_url = embedded_test_server()->GetURL("/blank.html?start");
  const GURL task_url =
      embedded_test_server()->GetURL("/page_with_clickable_element.html");
  std::string navigateProto =
      EncodeActionProto(actor::MakeNavigate(task_url.spec()));

  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(kActiveTabId, start_url),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  InstrumentNextTab(kNewActorTabId),
                  ExecuteAction(navigateProto, UpdatedContextOptions()),
                  WaitForWebContentsReady(kNewActorTabId, task_url));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       UsesExistingActorTabOnSubsequentNavigate) {
  const GURL task_url =
      embedded_test_server()->GetURL("/page_with_clickable_element.html");
  const GURL second_navigate_url =
      embedded_test_server()->GetURL("/blank.html?second");
  std::string secondNavigateProto =
      EncodeActionProto(actor::MakeNavigate(second_navigate_url.spec()));

  RunTestSequence(StartTaskInNewTab(task_url),
                  // Now that the task is started in a new tab, do the
                  // second navigation.
                  ExecuteAction(secondNavigateProto, UpdatedContextOptions()),
                  WaitForWebContentsReady(kNewActorTabId, second_navigate_url));
}

// TODO(https://crbug.com/402086021): Enable test after using real nodeId in
// proto.
IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, DISABLED_ActionSucceeds) {
  const GURL task_url =
      embedded_test_server()->GetURL("/page_with_clickable_element.html");
  std::string encodedProto =
      EncodeActionProto(actor::MakeClick(kContentNodeId));

  RunTestSequence(StartTaskInNewTab(task_url),
                  ExecuteAction(encodedProto, UpdatedContextOptions()));
  // TODO(https://crbug.com/402086021): Check result after implementing tool
  // calling to do the action.
  // WaitForJsResult(kActiveTabId, "() => button_clicked"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ActionProtoInvalid) {
  std::string encodedProto = base::Base64Encode("invalid serialized bytes");
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/page_with_clickable_element.html")),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ExecuteActionExpectingError(
          encodedProto, UpdatedContextOptions(),
          glic::mojom::ActInFocusedTabErrorReason::kInvalidActionProto));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ActionTargetNotFound) {
  const GURL task_url =
      embedded_test_server()->GetURL("/page_with_clickable_element.html");
  std::string encodedProto =
      EncodeActionProto(actor::MakeClick(kContentNodeId));

  RunTestSequence(
      StartTaskInNewTab(task_url),
      ExecuteActionExpectingError(
          encodedProto, UpdatedContextOptions(),
          glic::mojom::ActInFocusedTabErrorReason::kTargetNotFound));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, HistoryTool) {
  const GURL url_1 = embedded_test_server()->GetURL("/blank.html?1");
  const GURL url_2 = embedded_test_server()->GetURL("/blank.html?2");
  std::string navigateUrl2Proto =
      EncodeActionProto(actor::MakeNavigate(url_2.spec()));
  std::string backProto = EncodeActionProto(actor::MakeHistoryBack());
  std::string forwardProto = EncodeActionProto(actor::MakeHistoryForward());

  RunTestSequence(
      StartTaskInNewTab(url_1),
      ExecuteAction(navigateUrl2Proto, AnnotationsOnlyContextOptions()),
      ExecuteAction(backProto, AnnotationsOnlyContextOptions()),
      WaitForWebContentsReady(kNewActorTabId, url_1),
      // TODO(crbug.com/402086021): Test hangs flakily in a CopyOutputRequest
      // for the observation, unrelated to the HistoryTool code. Use
      // UpdatedContextOptions() once that's resolved.
      ExecuteAction(forwardProto, AnnotationsOnlyContextOptions()),
      WaitForWebContentsReady(kNewActorTabId, url_2));
}

class GlicActorControllerWithActorDisabledUiTest
    : public test::InteractiveGlicTest {
 public:
  GlicActorControllerWithActorDisabledUiTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kGlicActor);
  }
  ~GlicActorControllerWithActorDisabledUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicActorControllerWithActorDisabledUiTest,
                       ActorNotAvailable) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  InAnyContext(CheckJsResult(
                      kGlicContentsElementId,
                      "() => { return !(client.browser.actInFocusedTab); }")));
}

}  //  namespace

}  // namespace glic::test
