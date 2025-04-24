// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string_view>

#include "base/base64.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_identifier.h"

namespace glic::test {

namespace {

using ::optimization_guide::proto::AnnotatedPageContent;
using ::optimization_guide::proto::BrowserAction;
using ::optimization_guide::proto::ClickAction;
using ::optimization_guide::proto::ContentAttributes;
using ::optimization_guide::proto::ContentNode;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTabId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewActorTabId);

std::string EncodeActionProto(const BrowserAction& action) {
  return base::Base64Encode(action.SerializeAsString());
}

class GlicActorControllerUiTest : public test::InteractiveGlicTest {
 public:
  using ActionProtoProvider = base::OnceCallback<std::string()>;

  GlicActorControllerUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicActor, optimization_guide::features::
                                   kAnnotatedPageContentWithActionableElements},
        {});
  }
  ~GlicActorControllerUiTest() override = default;

  void SetUpOnMainThread() override {
    test::InteractiveGlicTest::SetUpOnMainThread();

    // TODO(crbug.com/409564704): Mock the delay so that tests can run at
    // reasonable speed. Remove once there is a more permanent approach.
    actor::OverrideActionObservationDelay(base::Milliseconds(10));
  }

  // This (and the below ExpectingError) step takes a proto "provider" which is
  // a callback that returns a string which is the base-64 representation of the
  // BrowserAction proto to invoke. This is a callback rather than a string
  // parameter since, in some cases, the parameters in the proto may depend on
  // test steps (such as extracting the AnnotatedPageContent, so that the
  // provider can then find the content node id from the APC). See the Provider
  // methods below (e.g. ClickActionProvider).
  InteractiveTestApi::MultiStep ExecuteAction(
      ActionProtoProvider proto_provider,
      base::Value::Dict context_options) {
    return Steps(InAnyContext(WithElement(
        kGlicContentsElementId, [&, proto_provider = std::move(proto_provider),
                                 context_options = std::move(context_options)](
                                    ui::TrackedElement* el) mutable {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          std::string script = content::JsReplace(
              R"js(
                        (() => {
                          const base64ToArrayBuffer = (base64) => {
                            const bytes = window.atob(base64);
                            const len = bytes.length;
                            const ret = new Uint8Array(len);
                            for (var i = 0; i < len; i++) {
                              ret[i] = bytes.charCodeAt(i);
                            }
                            return ret.buffer;
                          }
                          return client.browser.actInFocusedTab({
                            actionProto: base64ToArrayBuffer($1),
                            tabContextOptions: $2
                          });
                        })();
                      )js",
              std::move(proto_provider).Run(), std::move(context_options));
          ASSERT_TRUE(content::ExecJs(glic_contents, std::move(script)));
        })));
  }

  // Helper to allow passing in a BrowserAction if it doesn't depend on prior
  // test steps.
  InteractiveTestApi::MultiStep ExecuteAction(
      const BrowserAction& action,
      base::Value::Dict context_options) {
    return ExecuteAction(PassthroughProvider(action),
                         std::move(context_options));
  }

  // Calls actInFocusedTab() and waits until the promise rejects with an error.
  // Note: This will fail the test if the promise succeeds.
  InteractiveTestApi::MultiStep ExecuteActionExpectingError(
      ActionProtoProvider proto_provider,
      base::Value::Dict context_options,
      glic::mojom::ActInFocusedTabErrorReason error_reason) {
    return Steps(
        CheckJsResult(kGlicContentsElementId,
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
                                         std::move(proto_provider).Run(),
                                         std::move(context_options)),
                      ::testing::Eq(static_cast<int>(error_reason))));
  }

  // Helper to allow passing in a BrowserAction if it doesn't depend on prior
  // test steps. Calls actInFocusedTab() and waits until the promise rejects
  // with an error.  Note: This will fail the test if the promise succeeds.
  InteractiveTestApi::MultiStep ExecuteActionExpectingError(
      const BrowserAction& action,
      base::Value::Dict context_options,
      glic::mojom::ActInFocusedTabErrorReason error_reason) {
    return ExecuteActionExpectingError(
        PassthroughProvider(action), std::move(context_options), error_reason);
  }

  // Returns a callback that builds an encoded proto for a click action on a
  // ContentNode that matches the passed in predicate.
  ActionProtoProvider ClickActionProvider(std::string_view label) {
    return base::BindLambdaForTesting([this, label]() {
      int32_t node_id = this->SearchAnnotatedPageContent(label);
      return EncodeActionProto(actor::MakeClick(node_id));
    });
  }

  // Returns a callback that simply encodes the given action.
  ActionProtoProvider PassthroughProvider(const BrowserAction& action) {
    return base::BindLambdaForTesting(
        [action]() { return EncodeActionProto(action); });
  }

  // Returns a callback that returns the given string as the action proto. Meant
  // for testing error handling since this allows providing an invalid proto.
  ActionProtoProvider ArbitraryStringProvider(std::string_view str) {
    return base::BindLambdaForTesting([str]() { return std::string(str); });
  }

  // Gets the context options to capture a new observation after completing an
  // action. This includes both annotations (i.e. AnnotatedPageContent) and a
  // screenshot.
  base::Value::Dict UpdatedContextOptions() {
    return base::Value::Dict()
        .Set("annotatedPageContent", true)
#if BUILDFLAG(IS_LINUX)
        // TODO(https://crbug.com/40191775): Tests on Linux aren't producing
        // graphical output so requesting a screenshot hangs forever.
        .Set("viewportScreenshot", false);
#else
        .Set("viewportScreenshot", true);
#endif
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
    const GURL start_url =
        embedded_test_server()->GetURL("/actor/blank.html?start");
    BrowserAction start_navigate = actor::MakeNavigate(task_url.spec());

    return Steps(InstrumentTab(kActiveTabId),
                 NavigateWebContents(kActiveTabId, start_url),
                 OpenGlicWindow(GlicWindowMode::kAttached),
                 InstrumentNextTab(kNewActorTabId),
                 ExecuteAction(start_navigate, AnnotationsOnlyContextOptions()),
                 WaitForWebContentsReady(kNewActorTabId, task_url));
  }

  // Retrieves AnnotatedPageContent for the currently focused tab (and caches
  // it in `annotated_page_content_`).
  auto GetPageContextFromFocusedTab() {
    return Steps(Do([&]() {
      GlicKeyedService* glic_service =
          GlicKeyedServiceFactory::GetGlicKeyedService(browser()->GetProfile());
      ASSERT_TRUE(glic_service);

      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      auto fetcher = std::make_unique<GlicPageContextFetcher>();

      auto options = mojom::GetTabContextOptions::New();
      options->include_annotated_page_content = true;

      fetcher->Fetch(
          glic_service->GetFocusedTabData(), *options,
          base::BindLambdaForTesting([&](mojom::GetContextResultPtr result) {
            mojo_base::ProtoWrapper& serialized_apc =
                *result->get_tab_context()
                     ->annotated_page_data->annotated_page_content;
            annotated_page_content_ = std::make_unique<AnnotatedPageContent>(
                serialized_apc.As<AnnotatedPageContent>().value());
            run_loop.Quit();
          }));

      run_loop.Run();
    }));
  }

 private:
  int32_t SearchAnnotatedPageContent(std::string_view label) {
    CHECK(annotated_page_content_)
        << "An observation must be made with GetPageContextFromFocusedTab "
           "before searching annotated page content.";

    // Traverse the APC in depth-first preorder, returning the first node that
    // matches the given label.
    std::stack<const ContentNode*> nodes;
    nodes.push(&annotated_page_content_->root_node());

    while (!nodes.empty()) {
      const ContentNode* current = nodes.top();
      nodes.pop();

      if (current->content_attributes().label() == label) {
        return current->content_attributes().common_ancestor_dom_node_id();
      }

      for (const auto& child : current->children_nodes()) {
        nodes.push(&child);
      }
    }

    // Tests must pass a label that matches one of the content nodes.
    NOTREACHED();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AnnotatedPageContent> annotated_page_content_;
};

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, OpensNewTabOnFirstNavigate) {
  const GURL start_url =
      embedded_test_server()->GetURL("/actor/blank.html?start");
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  BrowserAction navigate = actor::MakeNavigate(task_url.spec());

  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(kActiveTabId, start_url),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  InstrumentNextTab(kNewActorTabId),
                  ExecuteAction(navigate, UpdatedContextOptions()),
                  WaitForWebContentsReady(kNewActorTabId, task_url));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest,
                       UsesExistingActorTabOnSubsequentNavigate) {
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  const GURL second_navigate_url =
      embedded_test_server()->GetURL("/actor/blank.html?second");
  BrowserAction second_navigate =
      actor::MakeNavigate(second_navigate_url.spec());

  RunTestSequence(StartTaskInNewTab(task_url),
                  // Now that the task is started in a new tab, do the
                  // second navigation.
                  ExecuteAction(second_navigate, UpdatedContextOptions()),
                  WaitForWebContentsReady(kNewActorTabId, second_navigate_url));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ActionSucceeds) {
  constexpr std::string_view kClickableButtonLabel = "clickable";

  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");

  RunTestSequence(StartTaskInNewTab(task_url), GetPageContextFromFocusedTab(),
                  ExecuteAction(ClickActionProvider(kClickableButtonLabel),
                                UpdatedContextOptions()),
                  WaitForJsResult(kNewActorTabId, "() => button_clicked"));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ActionProtoInvalid) {
  std::string encodedProto = base::Base64Encode("invalid serialized bytes");
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(kActiveTabId,
                          embedded_test_server()->GetURL(
                              "/actor/page_with_clickable_element.html")),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ExecuteActionExpectingError(
          ArbitraryStringProvider(encodedProto), UpdatedContextOptions(),
          glic::mojom::ActInFocusedTabErrorReason::kInvalidActionProto));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, ActionTargetNotFound) {
  constexpr int32_t kNonExistentContentNodeId =
      std::numeric_limits<int32_t>::max();
  const GURL task_url =
      embedded_test_server()->GetURL("/actor/page_with_clickable_element.html");
  BrowserAction click = actor::MakeClick(kNonExistentContentNodeId);

  RunTestSequence(
      StartTaskInNewTab(task_url),
      ExecuteActionExpectingError(
          click, UpdatedContextOptions(),
          glic::mojom::ActInFocusedTabErrorReason::kTargetNotFound));
}

IN_PROC_BROWSER_TEST_F(GlicActorControllerUiTest, HistoryTool) {
  const GURL url_1 = embedded_test_server()->GetURL("/actor/blank.html?1");
  const GURL url_2 = embedded_test_server()->GetURL("/actor/blank.html?2");
  BrowserAction navigate_url_2 = actor::MakeNavigate(url_2.spec());
  BrowserAction back = actor::MakeHistoryBack();
  BrowserAction forward = actor::MakeHistoryForward();

  RunTestSequence(StartTaskInNewTab(url_1),
                  ExecuteAction(navigate_url_2, UpdatedContextOptions()),
                  ExecuteAction(back, UpdatedContextOptions()),
                  WaitForWebContentsReady(kNewActorTabId, url_1),
                  ExecuteAction(forward, UpdatedContextOptions()),
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
