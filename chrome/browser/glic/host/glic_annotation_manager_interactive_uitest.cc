// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_list.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "pdf/buildflags.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom-test-utils.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

namespace glic::test {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTabId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAnnotationAgentDisconnectedByRemote);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kScrollStarted);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kScrollToRequestReceived);

#if BUILDFLAG(IS_MAC)
bool kTestDisabledForVirtualMachineMac =
    (base::mac::MacOSMajorVersion() == 15) && base::mac::IsVirtualMachine();
#endif  // BUILDFLAG(IS_MAC)

constexpr char kActivateSurfaceIncompatibilityNotice[] =
    "Programmatic window activation does not work on the Weston reference "
    "implementation of Wayland used on Linux testbots. It also doesn't work "
    "reliably on Linux in general. For this reason, some of these tests which "
    "use ActivateSurface() (which is also called by FocusWebContents()) may be "
    "skipped on machine configurations which do not reliably support them.";

// A fake service that can be used for more fine-grained control and timing
// around when selector matching completes.
class FakeAnnotationAgentContainer
    : public blink::mojom::AnnotationAgentContainerInterceptorForTesting,
      public blink::mojom::AnnotationAgent {
 public:
  FakeAnnotationAgentContainer() : receiver_(this), agent_receiver_(this) {}
  ~FakeAnnotationAgentContainer() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(
        mojo::PendingReceiver<blink::mojom::AnnotationAgentContainer>(
            std::move(handle)));
  }

  // blink::mojom::AnnotationAgentContainer overrides

  void CreateAgent(
      mojo::PendingRemote<blink::mojom::AnnotationAgentHost>
          pending_host_remote,
      mojo::PendingReceiver<blink::mojom::AnnotationAgent> agent_receiver,
      blink::mojom::AnnotationType type,
      const blink::mojom::SelectorPtr selector,
      std::optional<int> search_range_start_node_id) override {
    if (agent_receiver_.is_bound()) {
      agent_disconnected_ = false;
      agent_receiver_.reset();
      host_remote_.reset();
    }

    host_remote_.Bind(std::move(pending_host_remote));
    agent_receiver_.Bind(std::move(agent_receiver));
    agent_receiver_.set_disconnect_handler(base::BindLambdaForTesting([&]() {
      agent_disconnected_ = true;
      ui::TrackedElement* el =
          ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
              kBrowserViewElementId);
      if (el) {
        ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
            el, kAnnotationAgentDisconnectedByRemote);
      }
    }));

    auto* const el =
        ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
            kBrowserViewElementId);
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
        el, kScrollToRequestReceived);
  }

  // blink::mojom::AnnotationAgent overrides

  void ScrollIntoView(bool applies_focus) override {
    auto* const el =
        ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
            kBrowserViewElementId);
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
        el, kScrollStarted);
  }

  // blink::mojom::AnnotationAgentContainerInterceptorForTesting overrides

  blink::mojom::AnnotationAgentContainer* GetForwardingInterface() override {
    NOTREACHED();
  }

  void NotifyAttachment(gfx::Rect rect, blink::mojom::AttachmentResult result) {
    host_remote_->DidFinishAttachment(rect, result);
  }

  bool HighlightIsActive() {
    return agent_receiver_.is_bound() && !agent_disconnected_;
  }

 private:
  mojo::Remote<blink::mojom::AnnotationAgentHost> host_remote_;
  mojo::Receiver<blink::mojom::AnnotationAgentContainer> receiver_;
  mojo::Receiver<blink::mojom::AnnotationAgent> agent_receiver_;
  bool agent_disconnected_ = false;
};

class GlicAnnotationManagerUiTest : public InteractiveGlicTest {
 public:
  GlicAnnotationManagerUiTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicScrollTo);
    // TODO(b/453696965): These tests need fixed to work with
    // kGlicMultiInstance. The permission tests also rely on the pref, so
    // disable the default setting feature.
    no_multi_instance_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kGlicMultiInstance,
                               features::kGlicDefaultTabContextSetting});
  }
  ~GlicAnnotationManagerUiTest() override = default;

  void SetUpOnMainThread() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    InteractiveGlicTest::SetUpOnMainThread();
  }

  // Retrieves AnnotatedPageContent for the currently focused tab (and caches
  // it in `annotated_page_content_`).
  auto GetPageContextFromFocusedTab() {
    return Steps(Do([&]() {
      GlicKeyedService* glic_service =
          GlicKeyedServiceFactory::GetGlicKeyedService(browser()->GetProfile());
      ASSERT_TRUE(glic_service);

      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

      auto options = mojom::GetTabContextOptions::New();
      options->include_annotated_page_content = true;

      FocusedTabData data = glic_service->sharing_manager().GetFocusedTabData();
      if (data.focus()) {
        FetchPageContext(
            data.focus(), *options,
            base::BindLambdaForTesting(
                [&](base::expected<
                    glic::mojom::GetContextResultPtr,
                    page_content_annotations::FetchPageContextErrorDetails>
                        result) {
                  mojo_base::ProtoWrapper& serialized_apc =
                      *result.value()
                           ->get_tab_context()
                           ->annotated_page_data->annotated_page_content;
                  annotated_page_content_ = std::make_unique<
                      optimization_guide::proto::AnnotatedPageContent>(
                      serialized_apc
                          .As<optimization_guide::proto::AnnotatedPageContent>()
                          .value());
                  run_loop.Quit();
                }));

        run_loop.Run();
      }
    }));
  }

  using Selector = base::OnceCallback<base::Value::Dict()>;
  using DocumentIdGetter = base::OnceCallback<std::string()>;
  using NodeIdCallback = base::OnceCallback<int()>;
  using URLGetter = base::OnceCallback<GURL()>;

  // Calls scrollTo() and waits until the promise resolves and succeeds.
  auto ScrollTo(Selector selector) {
    return ScrollToImpl(std::move(selector), /*document_id=*/std::nullopt,
                        /*url=*/std::nullopt);
  }

  // Similar to ScrollTo(), but also includes documentId in the params.
  // If `document_id` is not set, it uses a value retrieved from
  // `annotated_page_content_`.
  auto ScrollToWithDocumentId(
      Selector selector,
      std::optional<DocumentIdGetter> document_id = std::nullopt) {
    return ScrollToImpl(std::move(selector),
                        DocumentIdOrDefault(std::move(document_id)),
                        /*url=*/std::nullopt);
  }

  // Similar to ScrollTo(), but also includes url in the params. If `url` is
  // not set, it uses the active tab's primary main frame's URL.
  auto ScrollToWithURL(Selector selector,
                       std::optional<URLGetter> url = std::nullopt) {
    return ScrollToImpl(std::move(selector), /*document_id=*/std::nullopt,
                        URLOrDefault(std::move(url)));
  }

  // Similar to ScrollTo(), but also includes both documentId and url. Uses
  // defaults for `document_id`/`url` if not set, see above methods for what the
  // default values used are.
  auto ScrollToWithDocumentIdAndURL(
      Selector selector,
      std::optional<DocumentIdGetter> document_id = std::nullopt,
      std::optional<URLGetter> url = std::nullopt) {
    return ScrollToImpl(std::move(selector),
                        DocumentIdOrDefault(std::move(document_id)),
                        URLOrDefault(std::move(url)));
  }

  // Calls scrollTo() and waits until the promise rejects with an error.
  // Note: This will fail the test if the promise succeeds.
  auto ScrollToExpectingError(Selector selector,
                              mojom::ScrollToErrorReason error_reason) {
    return ScrollToExpectingErrorImpl(std::move(selector),
                                      /*document_id=*/std::nullopt,
                                      /*url=*/std::nullopt, error_reason);
  }

  // Similar to ScrollToExpectingError(), but also includes documentId in the
  // params. If `document_id` is not set, it uses a value retrieved from
  // `annotated_page_content_`.
  auto ScrollToWithDocumentIdExpectingError(
      Selector selector,
      mojom::ScrollToErrorReason error_reason,
      std::optional<DocumentIdGetter> document_id = std::nullopt) {
    return ScrollToExpectingErrorImpl(
        std::move(selector), DocumentIdOrDefault(std::move(document_id)),
        /*url=*/std::nullopt, error_reason);
  }

  // Similar to ScrollToExpectingError(), but also includes url in the params.
  // If `url` is not set, it uses the active tab's primary main frame's URL.
  auto ScrollToWithURLExpectingError(
      Selector selector,
      mojom::ScrollToErrorReason error_reason,
      std::optional<URLGetter> url = std::nullopt) {
    return ScrollToExpectingErrorImpl(
        std::move(selector), /*document_id=*/std::nullopt,
        URLOrDefault(std::move(url)), error_reason);
  }

  // Similar to ScrollToExpectingError(), but also includes both documentId and
  // url. Uses defaults for `document_id`/`url` if not set, see above methods
  // for what the default values used are.
  auto ScrollToWithDocumentIdAndURLExpectingError(
      Selector selector,
      mojom::ScrollToErrorReason error_reason,
      std::optional<DocumentIdGetter> document_id = std::nullopt,
      std::optional<URLGetter> url = std::nullopt) {
    return ScrollToExpectingErrorImpl(
        std::move(selector), DocumentIdOrDefault(std::move(document_id)),
        URLOrDefault(std::move(url)), error_reason);
  }

  // Calls scrollTo() and returns immediately.
  auto ScrollToAsync(Selector selector) {
    return ScrollToAsyncImpl(std::move(selector), /*document_id=*/std::nullopt,
                             /*url=*/std::nullopt);
  }

  // Similar to ScrollToAsync(), but also includes documentId in the params. If
  // `document_id` is not set, it uses a value retrieved from
  // `annotated_page_content_`.
  auto ScrollToAsyncWithDocumentId(
      Selector selector,
      std::optional<DocumentIdGetter> document_id = std::nullopt) {
    return ScrollToAsyncImpl(std::move(selector),
                             DocumentIdOrDefault(std::move(document_id)),
                             /*url=*/std::nullopt);
  }

  // Similar to ScrollToAsync(), but also includes url in the params.
  // If `url` is not set, it uses the active tab's primary main frame's URL.
  auto ScrollToAsyncWithURL(Selector selector,
                            std::optional<URLGetter> url = std::nullopt) {
    return ScrollToAsyncImpl(std::move(selector), /*document_id=*/std::nullopt,
                             URLOrDefault(std::move(url)));
  }

  // Should be used in combination with ScrollToAsync*() above.
  auto WaitForScrollToError(mojom::ScrollToErrorReason error_reason) {
    return Steps(CheckJsResult(kGlicContentsElementId, R"js(
          () => {
            return new Promise(resolve => {
              window.scrollToPromise.catch(e => {
                resolve(e.reason);
              });
            });
          }
        )js",
                               ::testing::Eq(static_cast<int>(error_reason))),
                 ExpectErrorRecorded(error_reason));
  }

  // Creates a new FakeAnnotationAgentContainer, and updates the remote
  // interface registry with a method to bind to it instead of the real service.
  auto InsertFakeAnnotationService() {
    return Steps(Do([&]() {
      service_manager::InterfaceProvider::TestApi test_api(
          browser()
              ->tab_strip_model()
              ->GetActiveWebContents()
              ->GetPrimaryMainFrame()
              ->GetRemoteInterfaces());
      fake_service_ = std::make_unique<FakeAnnotationAgentContainer>();
      test_api.SetBinderForName(
          blink::mojom::AnnotationAgentContainer::Name_,
          base::BindRepeating(&FakeAnnotationAgentContainer::Bind,
                              base::Unretained(fake_service_.get())));
    }));
  }

  auto SetTabContextPermission(bool enable) {
    return Steps(Do([this, enable]() {
      browser()->profile()->GetPrefs()->SetBoolean(
          glic::prefs::kGlicTabContextEnabled, enable);
    }));
  }

  // Checks if the currently focused tab (according to GlicFocusedTabManager) is
  // `web_contents_id`, or waits until it is. Set `web_contents_id` to
  // std::nullopt to wait until no tab is in focus.
  auto WaitUntilGlicFocusedTabIs(
      std::optional<ui::ElementIdentifier> web_contents_id) {
    return Check([&, web_contents_id]() {
      GlicKeyedService* glic_service =
          GlicKeyedServiceFactory::GetGlicKeyedService(browser()->GetProfile());
      content::WebContents* web_contents = nullptr;
      if (web_contents_id) {
        auto* tracked_element =
            ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
                web_contents_id.value());
        web_contents =
            InteractiveBrowserTest::AsInstrumentedWebContents(tracked_element)
                ->web_contents();
      }
      content::WebContents* focused_web_contents =
          glic_service->sharing_manager().GetFocusedTabData().focus()
              ? glic_service->sharing_manager()
                    .GetFocusedTabData()
                    .focus()
                    ->GetContents()
              : nullptr;
      if (focused_web_contents == web_contents) {
        return true;
      }
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      auto subscription =
          glic_service->sharing_manager().AddFocusedTabChangedCallback(
              base::BindLambdaForTesting([&run_loop, glic_service,
                                          web_contents](const FocusedTabData&) {
                content::WebContents* focused_web_contents =
                    glic_service->sharing_manager().GetFocusedTabData().focus()
                        ? glic_service->sharing_manager()
                              .GetFocusedTabData()
                              .focus()
                              ->GetContents()
                        : nullptr;
                if (focused_web_contents == web_contents) {
                  run_loop.Quit();
                  return;
                }
              }));
      run_loop.Run();
      return true;
    });
  }

  InteractiveTestApi::MultiStep ExpectErrorRecorded(
      mojom::ScrollToErrorReason reason) {
    return Steps(Do([this, reason]() {
      histogram_tester_->ExpectUniqueSample("Glic.ScrollTo.ErrorReason", reason,
                                            1u);
    }));
  }

  auto UserSwitchesConversation() {
    const DeepQuery kOnActiveThreadChanged{{"#dropScrollToHighlightBtn"}};
    static constexpr char kClickFn[] = "el => el.click()";
    return ExecuteJsAt(test::kGlicContentsElementId, kOnActiveThreadChanged,
                       kClickFn);
  }

  Selector ExactTextSelector(
      std::string text,
      std::optional<NodeIdCallback> node_id_cb = std::nullopt) {
    return base::BindOnce(
        [](std::string text, std::optional<NodeIdCallback> node_id_cb) {
          base::Value::Dict dict;
          dict.Set("text", text);
          if (node_id_cb.has_value()) {
            dict.Set("searchRangeStartNodeId",
                     std::move(node_id_cb.value()).Run());
          }
          return base::Value::Dict().Set("exactText", std::move(dict));
        },
        std::move(text), std::move(node_id_cb));
  }

  Selector TextFragmentSelector(
      std::string text_start,
      std::string text_end,
      std::optional<NodeIdCallback> node_id_cb = std::nullopt) {
    return base::BindOnce(
        [](std::string text_start, std::string text_end,
           std::optional<NodeIdCallback> node_id_cb) {
          base::Value::Dict dict;
          dict.Set("textStart", text_start);
          dict.Set("textEnd", text_end);
          if (node_id_cb.has_value()) {
            dict.Set("searchRangeStartNodeId",
                     std::move(node_id_cb.value()).Run());
          }
          return base::Value::Dict().Set("textFragment", std::move(dict));
        },
        std::move(text_start), std::move(text_end), std::move(node_id_cb));
  }

  Selector NodeIdSelector(NodeIdCallback node_id_cb) {
    return base::BindOnce(
        [](NodeIdCallback node_id_cb) {
          return base::Value::Dict().Set(
              "node",
              base::Value::Dict().Set("nodeId", std::move(node_id_cb).Run()));
        },
        std::move(node_id_cb));
  }

  FakeAnnotationAgentContainer* fake_service() { return fake_service_.get(); }

  // Returns the main frame's document identifier in `annotated_page_content_`.
  std::string GetDocumentIdFromAnnotatedPageContent() {
    CHECK(annotated_page_content_);
    return annotated_page_content_->main_frame_data()
        .document_identifier()
        .serialized_token();
  }

  int GetRootDomNodeIdFromAnnotatedPageContent() {
    CHECK(annotated_page_content_);
    return annotated_page_content_->root_node()
        .content_attributes()
        .common_ancestor_dom_node_id();
  }

  int GetInvalidDomNodeIdFromAnnotatedPageContent() {
    CHECK(annotated_page_content_);
    return annotated_page_content_->root_node()
               .content_attributes()
               .common_ancestor_dom_node_id() +
           9999;
  }

  base::HistogramTester* histogram_tester() const {
    return histogram_tester_.get();
  }

 private:
  base::Value::Dict CreateScrollToParams(
      Selector selector,
      std::optional<DocumentIdGetter> document_id,
      std::optional<URLGetter> url) {
    base::Value::Dict scroll_to_params;
    scroll_to_params.Set("selector", std::move(selector).Run());
    if (document_id) {
      scroll_to_params.Set("documentId", std::move(*document_id).Run());
    }
    if (url) {
      scroll_to_params.Set("url", content::JsLiteralHelper<GURL>::Convert(
                                      std::move(*url).Run()));
    }
    return scroll_to_params;
  }

  DocumentIdGetter DocumentIdOrDefault(
      std::optional<DocumentIdGetter> document_id) {
    return base::BindLambdaForTesting(
        [&, document_id_getter = std::move(document_id)]() mutable {
          if (!document_id_getter.has_value()) {
            return GetDocumentIdFromAnnotatedPageContent();
          }
          return std::move(*document_id_getter).Run();
        });
  }

  URLGetter URLOrDefault(std::optional<URLGetter> url) {
    return base::BindLambdaForTesting(
        [&, url_getter = std::move(url)]() mutable {
          if (!url_getter) {
            return browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetPrimaryMainFrame()
                ->GetLastCommittedURL();
          }
          return std::move(*url_getter).Run();
        });
  }

  InteractiveGlicTest::MultiStep ScrollToImpl(
      Selector selector,
      std::optional<DocumentIdGetter> document_id,
      std::optional<URLGetter> url) {
    return Steps(
        Do([&]() {
          histogram_tester_ = std::make_unique<base::HistogramTester>();
        }),
        InAnyContext(WithElement(
            kGlicContentsElementId,
            [&, selector = std::move(selector),
             document_id = std::move(document_id),
             url = std::move(url)](ui::TrackedElement* el) mutable {
              content::WebContents* glic_contents =
                  AsInstrumentedWebContents(el)->web_contents();
              base::Value::Dict scroll_to_params = CreateScrollToParams(
                  std::move(selector), std::move(document_id), std::move(url));
              std::string script = content::JsReplace(
                  R"js(
                  (() => {
                    return client.browser.scrollTo($1);
                  })();
                )js",
                  std::move(scroll_to_params));
              ASSERT_TRUE(content::ExecJs(glic_contents, std::move(script)));
            })),
        Do([&]() {
          histogram_tester_->ExpectTotalCount(
              "Glic.ScrollTo.MatchDuration.Success", 1);
        }));
  }

  InteractiveGlicTest::MultiStep ScrollToExpectingErrorImpl(
      Selector selector,
      std::optional<DocumentIdGetter> document_id,
      std::optional<URLGetter> url,
      mojom::ScrollToErrorReason error_reason) {
    return Steps(
        Do([&]() {
          histogram_tester_ = std::make_unique<base::HistogramTester>();
        }),
        InAnyContext(WithElement(
            kGlicContentsElementId,
            [&, selector = std::move(selector),
             document_id = std::move(document_id), url = std::move(url),
             error_reason](ui::TrackedElement* el) mutable {
              content::WebContents* glic_contents =
                  AsInstrumentedWebContents(el)->web_contents();
              base::Value::Dict scroll_to_params = CreateScrollToParams(
                  std::move(selector), std::move(document_id), std::move(url));
              std::string script = content::JsReplace(
                  R"js(
                    (async () => {
                      try {
                        await client.browser.scrollTo($1);
                      } catch (err) {
                        return err.reason;
                      }
                    })();
                  )js",
                  std::move(scroll_to_params));
              EXPECT_EQ(content::EvalJs(glic_contents, std::move(script)),
                        static_cast<int>(error_reason));
            })),
        ExpectErrorRecorded(error_reason));
  }

  InteractiveGlicTest::MultiStep ScrollToAsyncImpl(
      Selector selector,
      std::optional<DocumentIdGetter> document_id,
      std::optional<URLGetter> url) {
    return Steps(InAnyContext(WithElement(
        kGlicContentsElementId,
        [&, selector = std::move(selector),
         document_id = std::move(document_id),
         url = std::move(url)](ui::TrackedElement* el) mutable {
          content::WebContents* glic_contents =
              AsInstrumentedWebContents(el)->web_contents();
          auto scroll_to_params = CreateScrollToParams(
              std::move(selector), std::move(document_id), std::move(url));
          std::string script = content::JsReplace(
              R"js(
                (() => {
                  window.scrollToPromise = client.browser.scrollTo($1);
                })();
              )js",
              std::move(scroll_to_params));
          ASSERT_TRUE(content::ExecJs(glic_contents, script));
        })));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList no_multi_instance_feature_list_;
  std::unique_ptr<FakeAnnotationAgentContainer> fake_service_;
  base::CallbackListSubscription focused_tab_change_subscription_;
  std::unique_ptr<optimization_guide::proto::AnnotatedPageContent>
      annotated_page_content_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, ScrollToExactText) {
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentId(ExactTextSelector("Some text")),
                  WaitForJsResult(kActiveTabId, "() => did_scroll"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, ScrollToTextFragment) {
  // TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoia for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentId(TextFragmentSelector("Some", "text")),
                  WaitForJsResult(kActiveTabId, "() => did_scroll"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, NoMatchFound) {
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentIdExpectingError(
                      ExactTextSelector("Text does not exist"),
                      mojom::ScrollToErrorReason::kNoMatchFound));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       FailsWhenNoDocumentIdIsProvided) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      ScrollToExpectingError(ExactTextSelector("Some text"),
                             mojom::ScrollToErrorReason::kNotSupported));
}

// Runs a navigation while a scrollTo() request is being processed.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       NavigationAfterScrollToRequest) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived),
      NavigateWebContents(kActiveTabId,
                          embedded_test_server()->GetURL("/title.html")),
      WaitForScrollToError(
          mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated));
}

// Opens a new tab while a scrollTo() request is being processed (which results
// in the previous tab losing focus).
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       NewTabOpenedAfterScrollToRequest) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived),
      PressButton(kNewTabButtonElementId),
      WaitForScrollToError(
          mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated));
}

// This tests a state where GlicFocusedTabManager has no focused tab. It
// relies on chrome://settings not being considered as a valid URL by the class.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, NoFocusedTab) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(kActiveTabId, GURL("chrome://settings")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      WaitUntilGlicFocusedTabIs(std::nullopt), InsertFakeAnnotationService(),
      ScrollToWithDocumentIdExpectingError(
          ExactTextSelector("does not matter"),
          mojom::ScrollToErrorReason::kNoFocusedTab,
          base::BindLambdaForTesting(
              []() { return base::UnguessableToken().Create().ToString(); })));
}

// Sends a second scrollTo() request before the first request finishes
// processing.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, SecondScrollToRequest) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("Some text")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived),
      // Stores the first window.scrollToPromise in a new variable (because we
      // set it again below when we call ScrollToAsync again).
      ExecuteJs(kGlicContentsElementId,
                "() => { window.firstPromise = window.scrollToPromise; }"),
      ScrollToAsyncWithDocumentId(ExactTextSelector("Some text again")),
      CheckJsResult(
          kGlicContentsElementId, R"js(
            () => {
              return new Promise(resolve => {
                window.firstPromise.catch(e => { resolve(e.reason); });
              });
            }
          )js",
          static_cast<int>(mojom::ScrollToErrorReason::kNewerScrollToCall)),
      ExpectErrorRecorded(mojom::ScrollToErrorReason::kNewerScrollToCall));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       HighlightKeptAliveAfterScrollToRequestIsComplete) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived), Do([&]() {
        fake_service()->NotifyAttachment(
            gfx::Rect(20, 20), blink::mojom::AttachmentResult::kSuccess);
      }),
      WaitForEvent(kBrowserViewElementId, kScrollStarted),
      Check([&]() { return fake_service()->HighlightIsActive(); },
            "Agent connection should still be alive."));
}

// Switches focus from the Glic window to the active tab after the scroll
// request completes. The highlight should remain active.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       HighlightKeptAfterFocusSwitchesFromGlicWindow) {
  // TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoia for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), FocusWebContents(kGlicContentsElementId),
      InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived), Do([&]() {
        fake_service()->NotifyAttachment(
            gfx::Rect(20, 20), blink::mojom::AttachmentResult::kSuccess);
      }),
      WaitForEvent(kBrowserViewElementId, kScrollStarted),
      FocusWebContents(kActiveTabId), WaitUntilGlicFocusedTabIs(kActiveTabId),
      Check([&]() { return fake_service()->HighlightIsActive(); },
            "Agent connection should still be alive."));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       HighlightKeptAfterFocusSwitchesToNewTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(kActiveTabId,
                          embedded_test_server()->GetURL("/title1.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived), Do([&]() {
        fake_service()->NotifyAttachment(
            gfx::Rect(20, 20), blink::mojom::AttachmentResult::kSuccess);
      }),
      WaitForEvent(kBrowserViewElementId, kScrollStarted),
      AddInstrumentedTab(kNewTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
      WaitUntilGlicFocusedTabIs(kNewTabId),
      Check([&]() { return fake_service()->HighlightIsActive(); }),
      SelectTab(kTabStripElementId, 0), WaitUntilGlicFocusedTabIs(kActiveTabId),
      Check([&]() { return fake_service()->HighlightIsActive(); }));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       HighlightDroppedAfterScrollToInNewTab) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTabId);
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(kActiveTabId,
                          embedded_test_server()->GetURL("/title1.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived), Do([&]() {
        fake_service()->NotifyAttachment(
            gfx::Rect(20, 20), blink::mojom::AttachmentResult::kSuccess);
      }),
      WaitForEvent(kBrowserViewElementId, kScrollStarted),
      AddInstrumentedTab(kNewTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
      WaitUntilGlicFocusedTabIs(kNewTabId), GetPageContextFromFocusedTab(),
      Check([&]() { return fake_service()->HighlightIsActive(); }),
      ScrollToWithDocumentId(ExactTextSelector("Some text")),
      Check([&]() { return !fake_service()->HighlightIsActive(); }));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       TwoSuccessfulScrollToCalls) {
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentId(ExactTextSelector("Some text")),
                  WaitForJsResult(kActiveTabId, "() => did_scroll"),
                  ExecuteJs(kActiveTabId, "() => { did_scroll = false; }"),
                  ScrollToWithDocumentId(ExactTextSelector("Go Down")),
                  WaitForJsResult(kActiveTabId, "() => did_scroll"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       HighlightDroppedAfterPageIsNavigatedFrom) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived), Do([&]() {
        fake_service()->NotifyAttachment(
            gfx::Rect(20, 20), blink::mojom::AttachmentResult::kSuccess);
      }),
      WaitForEvent(kBrowserViewElementId, kScrollStarted),
      Check([&]() { return fake_service()->HighlightIsActive(); },
            "Agent connection should still be alive."),
      NavigateWebContents(kActiveTabId,
                          embedded_test_server()->GetURL("/title2.html")),
      Check([&]() { return !fake_service()->HighlightIsActive(); }));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, WithDocumentId) {
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentId(ExactTextSelector("Some text")),
                  WaitForJsResult(kActiveTabId, "() => did_scroll"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, WithUnknownDocumentId) {
  DocumentIdGetter unknown_document_id = base::BindLambdaForTesting(
      []() { return base::UnguessableToken().Create().ToString(); });
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentIdExpectingError(
                      ExactTextSelector("Some text"),
                      mojom::ScrollToErrorReason::kNoMatchingDocument,
                      std::move(unknown_document_id)));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, WithIframeDocumentId) {
  DocumentIdGetter iframe_document_id = base::BindLambdaForTesting([&]() {
    content::RenderFrameHost* iframe_rfh = content::ChildFrameAt(
        browser()->tab_strip_model()->GetActiveWebContents(), /*index=*/0u);
    return optimization_guide::DocumentIdentifierUserData::
        GetForCurrentDocument(iframe_rfh)
            ->serialized_token();
  });
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(kActiveTabId,
                                      embedded_test_server()->GetURL(
                                          "/scrollable_page_with_iframe.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentIdExpectingError(
                      ExactTextSelector("Some text"),
                      mojom::ScrollToErrorReason::kNoMatchingDocument,
                      std::move(iframe_document_id)));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       WithPreviousDocumentIdAfterNavigation) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(),
      NavigateWebContents(kActiveTabId,
                          embedded_test_server()->GetURL("/title1.html")),
      ScrollToWithDocumentIdExpectingError(
          ExactTextSelector("Some text"),
          mojom::ScrollToErrorReason::kNoMatchingDocument));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, TextFocusedAfterScroll) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(),
      ExecuteJs(kActiveTabId,
                "() => { document.getElementById('text').tabIndex = 0; }"),
      ScrollToWithDocumentId(ExactTextSelector("Some text")),
      WaitForJsResult(kActiveTabId, "() => did_scroll"),
      CheckJsResult(kActiveTabId, "() => { return document.activeElement.id; }",
                    ::testing::Eq("text")));
}

// Search the exact text from the range with the start node id which is
// extracted from `annotated_page_content_`.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       ScrollToExactTextWithStartDomNodeId) {
  NodeIdCallback range_start_id_cb = base::BindOnce(
      &GlicAnnotationManagerUiTest::GetRootDomNodeIdFromAnnotatedPageContent,
      base::Unretained(this));
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentId(ExactTextSelector(
                      "Some text", std::move(range_start_id_cb))),
                  WaitForJsResult(kActiveTabId, "() => did_scroll"));
}

// Search the text fragment from the range with the start node id which is
// extracted from `annotated_page_content_`.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       ScrollToTextFragmentWithStartDomNodeId) {
  NodeIdCallback range_start_id_cb = base::BindOnce(
      &GlicAnnotationManagerUiTest::GetRootDomNodeIdFromAnnotatedPageContent,
      base::Unretained(this));
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentId(TextFragmentSelector(
                      "Some", "text", std::move(range_start_id_cb))),
                  WaitForJsResult(kActiveTabId, "() => did_scroll"));
}

// If the start node id is not from `annotated_page_content_`, throw an invalid
// range error.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       NoMatchFoundWithStartDomNodeId) {
  NodeIdCallback invalid_id_cb = base::BindOnce(
      &GlicAnnotationManagerUiTest::GetInvalidDomNodeIdFromAnnotatedPageContent,
      base::Unretained(this));
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentIdExpectingError(
                      ExactTextSelector("Some text", std::move(invalid_id_cb)),
                      mojom::ScrollToErrorReason::kSearchRangeInvalid));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, NodeIdSelector) {
  NodeIdCallback text_node = base::BindLambdaForTesting([&]() {
    return content::GetDOMNodeId(*browser()
                                      ->tab_strip_model()
                                      ->GetActiveWebContents()
                                      ->GetPrimaryMainFrame(),
                                 "p#text")
        .value();
  });
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentId(NodeIdSelector(std::move(text_node))));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       NodeIdSelectorWithInvalidNode) {
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentIdExpectingError(
                      NodeIdSelector(base::BindOnce([]() { return -1; })),
                      mojom::ScrollToErrorReason::kNoMatchFound));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       HighlightIsDroppedWhenPanelIsClosed) {
  // TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoia for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived), Do([&]() {
        fake_service()->NotifyAttachment(
            gfx::Rect(20, 20), blink::mojom::AttachmentResult::kSuccess);
      }),
      CloseGlicWindow(),
      Check([&]() { return !fake_service()->HighlightIsActive(); },
            "Annotations should be dropped"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       ScrollToFailsWhenPanelIsClosedBeforeAttachment) {
  // TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoia for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived),
      CloseGlicWindow(),
      // We cannot use `WaitForScrollError()` here as `kGlicContentsElementId`
      // is already hidden and `CheckJsResult` doesn't work when the provided
      // contents isn't visible.
      CheckResult(
          [&]() {
            return content::EvalJs(glic_service()
                                       ->GetInstanceForActiveTab(browser())
                                       ->host()
                                       .webui_contents()
                                       ->GetInnerWebContents()[0],
                                   R"js(
              new Promise(resolve => {
                window.scrollToPromise.catch(e => {
                  resolve(e.reason);
                });
              });
            )js")
                .ExtractInt();
          },
          static_cast<int>(
              mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated)),
      ExpectErrorRecorded(
          mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       HighlightIsDroppedWhenWebClientClosed) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived), Do([&]() {
        fake_service()->NotifyAttachment(
            gfx::Rect(20, 20), blink::mojom::AttachmentResult::kSuccess);
      }),
      Do([&]() { glic_service()->window_controller().Close(); }),
      WaitForHide(kGlicViewElementId),
      Check([&]() { return !fake_service()->HighlightIsActive(); },
            "Annotations should be dropped"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       TabContextPermissionDisabledBeforeRequest) {
  // TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoia for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

  RunTestSequence(  //
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), SetTabContextPermission(false),
      ScrollToWithDocumentIdExpectingError(
          ExactTextSelector("Text does not exist"),
          mojom::ScrollToErrorReason::kTabContextPermissionDisabled));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       TabContextPermissionDisabledDuringScrollToRequest) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived),
      SetTabContextPermission(false),
      WaitForScrollToError(
          mojom::ScrollToErrorReason::kTabContextPermissionDisabled));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       HighlightIsDroppedWhenTabContextPermissionIsDisabled) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived), Do([&]() {
        fake_service()->NotifyAttachment(
            gfx::Rect(20, 20), blink::mojom::AttachmentResult::kSuccess);
      }),
      SetTabContextPermission(false),
      WaitForEvent(kBrowserViewElementId, kAnnotationAgentDisconnectedByRemote),
      Check([&]() { return !fake_service()->HighlightIsActive(); },
            "Annotations should be dropped"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       HighlightIsDroppedWhenActiveConversationChanged) {
  // TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoia for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived), Do([&]() {
        fake_service()->NotifyAttachment(
            gfx::Rect(20, 20), blink::mojom::AttachmentResult::kSuccess);
      }),
      UserSwitchesConversation(),
      WaitForEvent(kBrowserViewElementId, kAnnotationAgentDisconnectedByRemote),
      Check([&]() { return !fake_service()->HighlightIsActive(); },
            "Annotations should be dropped"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       ActiveConversationChangedDuringScrollToRequest) {
  // TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoi for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived),
      UserSwitchesConversation(),
      WaitForScrollToError(mojom::ScrollToErrorReason::kDroppedByWebClient));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, RecordsSessionCount) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(),
      ScrollToWithDocumentIdExpectingError(
          ExactTextSelector("missing text"),
          mojom::ScrollToErrorReason::kNoMatchFound),
      ScrollToWithDocumentId(ExactTextSelector("Some text")), Do([&]() {
        histogram_tester()->ExpectTotalCount("Glic.ScrollTo.SessionCount",
                                             /*expected_count=*/0);
      }),
      CloseGlicWindow(), Do([&]() {
        histogram_tester()->ExpectUniqueSample("Glic.ScrollTo.SessionCount",
                                               /*sample=*/2,
                                               /*expected_bucket_count=*/1);
      }));
}

// Tests that "Glic.ScrollTo.UserPromptToScrollTime" is:
//  - not recorded if scrolling fails
//  - recorded after scrolling starts
//
// This test manually calls `GlicMetrics` methods like `OnUserInputSubmitted`,
// `OnResponseStarted` and `OnResponseStopped` instead of doing it through
// the test client for convenience and better control of timing. The order of
// the method calls reflect the order of expected calls in practice.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       RecordsUserPromptToScrollTime) {
  GlicMetrics* glic_metrics;
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      GetPageContextFromFocusedTab(), InsertFakeAnnotationService(), Do([&]() {
        glic_metrics = GlicKeyedServiceFactory::GetGlicKeyedService(
                           browser()->GetProfile())
                           ->metrics();
        glic_metrics->OnUserInputSubmitted(mojom::WebClientMode::kAudio);
      }),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived), Do([&]() {
        glic_metrics->OnResponseStarted();
        glic_metrics->OnResponseStopped(mojom::ResponseStopCause::kUnknown);
      }),
      Do([&]() {
        fake_service()->NotifyAttachment(
            gfx::Rect(), blink::mojom::AttachmentResult::kSelectorNotMatched);
      }),
      WaitForScrollToError(mojom::ScrollToErrorReason::kNoMatchFound),
      Do([&]() {
        // Metric shouldn't be recorded if scrolling wasn't triggered.
        histogram_tester()->ExpectTotalCount(
            "Glic.ScrollTo.UserPromptToScrollTime.Audio",
            /*expected_count=*/0);
      }),
      Do([&]() {
        glic_metrics->OnUserInputSubmitted(mojom::WebClientMode::kAudio);
      }),
      ScrollToAsyncWithDocumentId(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived), Do([&]() {
        glic_metrics->OnResponseStarted();
        glic_metrics->OnResponseStopped(mojom::ResponseStopCause::kUnknown);
      }),
      Do([&]() {
        fake_service()->NotifyAttachment(
            gfx::Rect(20, 20), blink::mojom::AttachmentResult::kSuccess);
      }),
      WaitForEvent(kBrowserViewElementId, kScrollStarted), Do([&]() {
        histogram_tester()->ExpectTotalCount(
            "Glic.ScrollTo.UserPromptToScrollTime.Audio",
            /*expected_count=*/1);
      }));
}

class GlicAnnotationManagerWithScrollToDisabledUiTest
    : public InteractiveGlicTest {
 public:
  GlicAnnotationManagerWithScrollToDisabledUiTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kGlicScrollTo);
    // TODO(b/453696965): These tests need fixed to work with
    // kGlicMultiInstance. The permission tests also rely on the pref, so
    // disable the default setting feature.
    no_multi_instance_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kGlicMultiInstance,
                               features::kGlicDefaultTabContextSetting});
  }
  ~GlicAnnotationManagerWithScrollToDisabledUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList no_multi_instance_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerWithScrollToDisabledUiTest,
                       ScrollToNotAvailable) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  InAnyContext(CheckJsResult(
                      kGlicContentsElementId,
                      "() => { return !(client.browser.scrollTo); }")));
}

#if BUILDFLAG(ENABLE_PDF)
// To test the scrollTo for PDFs, the tests should not use the fake annotation
// service. Instead the test should exercise on a real renderer with a real PDF
// document to make sure the correct frame host is targeted.
class GlicAnnotationManagerTestForPDF
    : public GlicAnnotationManagerUiTest,
      public ::testing::WithParamInterface<bool> {
 public:
  GlicAnnotationManagerTestForPDF() {
    InitFeatureParams(/*enable_scroll_to_pdf=*/true,
                      /*enforce_url_for_pdf=*/true);
  }
  ~GlicAnnotationManagerTestForPDF() override = default;

  bool UseOopif() const { return GetParam(); }

  void InitFeatureParams(bool enable_scroll_to_pdf, bool enforce_url_for_pdf) {
    scoped_feature_list_.Reset();
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicScrollTo,
         {{"glic-scroll-to-pdf", base::ToString(enable_scroll_to_pdf)},
          {"glic-scroll-to-enforce-url-for-pdf",
           base::ToString(enforce_url_for_pdf)}}}};
    std::vector<base::test::FeatureRef> disabled_features = {};
    if (UseOopif()) {
      enabled_features.push_back({chrome_pdf::features::kPdfOopif, {}});
    } else {
      disabled_features.push_back(chrome_pdf::features::kPdfOopif);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  // Note: `EnsurePDFHasLoaded()` uses RunLoop(s) with type kDefault. This
  // method is not safe to be embedded inside other RunLoops, for example,
  // inside Kombucha's `RunTestSequence()`.
  void NavigateToPDF(const GURL& pdf_url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_url));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));
  }

  auto InjectEmbeddedPDF(const GURL& pdf_url) {
    return Do([this, pdf_url = GURL(pdf_url)]() {
      constexpr char kAddIFrame[] = R"({
          (()=>{
              return new Promise((resolve) => {
                const frame = document.createElement('embed');
                frame.addEventListener('load', resolve);
                frame.id = 'embed';
                frame.src = $1;
                document.body.appendChild(frame);
              });
          })();
        })";
      content::WebContents* web_contents =
          browser()->tab_strip_model()->GetActiveWebContents();
      ASSERT_TRUE(web_contents);
      ASSERT_TRUE(
          ExecJs(web_contents, content::JsReplace(kAddIFrame, pdf_url)));
    });
  }

  static std::string PrintTestVariant(
      const ::testing::TestParamInfo<bool>& info) {
    return info.param ? "OOPIF" : "InnerWebContents";
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDF, TextFragmentFound) {
  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      // At the end of `ScrollTo`, "Glic.ScrollTo.MatchDuration.Success" is
      // asserted to have one sample. The histogram is only recorded with a
      // successful `DidFinishAttachment()`.
      ScrollToWithURL(ExactTextSelector("test")));
}

// TODO(crbug.com/455834776): Disabled on Mac due to failures.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TwoScrolls DISABLED_TwoScrolls
#else
#define MAYBE_TwoScrolls TwoScrolls
#endif
IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDF, MAYBE_TwoScrolls) {
  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true),
                  ScrollToWithURL(ExactTextSelector("test")),
                  ScrollToWithURL(ExactTextSelector("Result")));
}

// TODO(crbug.com/455834776): Disabled on Mac due to failures.
#if BUILDFLAG(IS_MAC)
#define MAYBE_FirstFoundSecondNotFound DISABLED_FirstFoundSecondNotFound
#else
#define MAYBE_FirstFoundSecondNotFound FirstFoundSecondNotFound
#endif
IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDF,
                       MAYBE_FirstFoundSecondNotFound) {
  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      ScrollToWithURL(ExactTextSelector("test")),
      ScrollToWithURLExpectingError(ExactTextSelector("not_found"),
                                    mojom::ScrollToErrorReason::kNoMatchFound));
}

IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDF, TextFragmentNotFound) {
  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      ScrollToWithURLExpectingError(ExactTextSelector("not_found"),
                                    mojom::ScrollToErrorReason::kNoMatchFound));
}

IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDF,
                       FirstNotFoundSecondFound) {
  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      ScrollToWithURLExpectingError(ExactTextSelector("not_found"),
                                    mojom::ScrollToErrorReason::kNoMatchFound),
      ScrollToWithURL(ExactTextSelector("test")));
}

IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDF, EmptyTextFragment) {
  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      ScrollToWithURLExpectingError(ExactTextSelector(""),
                                    mojom::ScrollToErrorReason::kNotSupported));
}

IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDF,
                       NodeIdSelectorNotSupported) {
  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true),
                  ScrollToWithDocumentIdAndURLExpectingError(
                      NodeIdSelector(base::BindOnce([]() { return -1; })),
                      mojom::ScrollToErrorReason::kNotSupported,
                      base::BindLambdaForTesting([]() {
                        return base::UnguessableToken::Create().ToString();
                      })));
}

// Test that scrollTo works after the page is navigated away from the PDF to a
// regular web page.
IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDF,
                       AnnotationAgentContainerIPCEndPoint) {
  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(
      InstrumentTab(kActiveTabId), OpenGlicWindow(GlicWindowMode::kDetached),
      SetTabContextPermission(true),
      // Blocks until "test" is found.
      ScrollToWithURL(ExactTextSelector("test")),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      GetPageContextFromFocusedTab(),
      // Blocks until "Some text" is found.
      ScrollToWithDocumentId(ExactTextSelector("Some text")));
}

// Asserts that the annotation is not dispatched to embedded PDFs.
IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDF,
                       EmbeddedPDFNotSupported) {
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  InjectEmbeddedPDF(
                      embedded_test_server()->GetURL("/find_in_pdf_page.pdf")),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true), GetPageContextFromFocusedTab(),
                  ScrollToWithDocumentIdAndURL(ExactTextSelector("Some text")));
}

IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDF, NoURLProvided) {
  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(
      InstrumentTab(kActiveTabId), OpenGlicWindow(GlicWindowMode::kDetached),
      SetTabContextPermission(true),
      ScrollToExpectingError(ExactTextSelector("Some text"),
                             mojom::ScrollToErrorReason::kNotSupported));
}

IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDF,
                       NonMatchingURLProvided) {
// TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoia for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(InstrumentTab(kActiveTabId),
                  OpenGlicWindow(GlicWindowMode::kDetached),
                  SetTabContextPermission(true),
                  ScrollToWithURLExpectingError(
                      ExactTextSelector("Some text"),
                      mojom::ScrollToErrorReason::kNoMatchingDocument,
                      base::BindLambdaForTesting(
                          [] { return GURL("https://www.google.com"); })));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GlicAnnotationManagerTestForPDF,
    ::testing::Bool(),
    &GlicAnnotationManagerTestForPDF::PrintTestVariant);

class GlicAnnotationManagerTestForPDFFeatureDisabled
    : public GlicAnnotationManagerTestForPDF {
 public:
  GlicAnnotationManagerTestForPDFFeatureDisabled() {
    InitFeatureParams(/*enable_scroll_to_pdf=*/false,
                      /*enforce_url_for_pdf=*/false);
  }
  ~GlicAnnotationManagerTestForPDFFeatureDisabled() override = default;
};

IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDFFeatureDisabled,
                       NotSupported) {
  // TODO(crbug.com/445214951): Flaky on mac-vm builder for macOS 15.
#if BUILDFLAG(IS_MAC)
  if (kTestDisabledForVirtualMachineMac) {
    GTEST_SKIP() << "Disabled on macOS Sequoi for virtual machines.";
  }
#endif  // BUILDFLAG(IS_MAC)

  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached), SetTabContextPermission(true),
      ScrollToWithURLExpectingError(ExactTextSelector("test"),
                                    mojom::ScrollToErrorReason::kNotSupported));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GlicAnnotationManagerTestForPDFFeatureDisabled,
    ::testing::Bool(),
    &GlicAnnotationManagerTestForPDF::PrintTestVariant);

class GlicAnnotationManagerTestForPDFWithEnforceURLDisabled
    : public GlicAnnotationManagerTestForPDF {
 public:
  GlicAnnotationManagerTestForPDFWithEnforceURLDisabled() {
    InitFeatureParams(/*enable_scroll_to_pdf=*/true,
                      /*enforce_url_for_pdf=*/false);
  }
  ~GlicAnnotationManagerTestForPDFWithEnforceURLDisabled() override = default;
};

IN_PROC_BROWSER_TEST_P(GlicAnnotationManagerTestForPDFWithEnforceURLDisabled,
                       ScrollToSucceedsWithoutURL) {
  NavigateToPDF(embedded_test_server()->GetURL("/find_in_pdf_page.pdf"));
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kDetached),  //
                  SetTabContextPermission(true),              //
                  ScrollTo(ExactTextSelector("test")));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    GlicAnnotationManagerTestForPDFWithEnforceURLDisabled,
    ::testing::Bool(),
    &GlicAnnotationManagerTestForPDF::PrintTestVariant);

#endif  // BUILDFLAG(ENABLE_PDF)

}  // namespace glic::test
