// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_list.h"
#include "base/notreached.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic.mojom-shared.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom-test-utils.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace glic::test {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTabId);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kScrollToRequestReceived);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kScrollStarted);

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
      const std::string& serialized_selector) override {
    if (agent_receiver_.is_bound()) {
      agent_disconnected_ = false;
      agent_receiver_.reset();
      host_remote_.reset();
    }

    host_remote_.Bind(std::move(pending_host_remote));
    agent_receiver_.Bind(std::move(agent_receiver));
    agent_receiver_.set_disconnect_handler(
        base::BindLambdaForTesting([&]() { agent_disconnected_ = true; }));

    auto* const el =
        ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
            kBrowserViewElementId);
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
        el, kScrollToRequestReceived);
  }

  // blink::mojom::AnnotationAgent overrides

  void ScrollIntoView() override {
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

  void NotifyAttachment(gfx::Rect rect) {
    host_remote_->DidFinishAttachment(rect);
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

class GlicAnnotationManagerUiTest : public test::InteractiveGlicTest {
 public:
  GlicAnnotationManagerUiTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicScrollTo);
  }
  ~GlicAnnotationManagerUiTest() override = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    test::InteractiveGlicTest::SetUpOnMainThread();
  }

  // Calls scrollTo() and waits until the promise resolves and succeeds.
  auto ScrollTo(base::Value::Dict selector) {
    return Steps(CheckJsResult(kGlicContentsElementId,
                               content::JsReplace(R"js(
                        () => {
                          return client.browser.scrollTo({selector: $1});
                        }
                      )js",
                                                  std::move(selector))));
  }

  // Calls scrollTo() and waits until the promise rejects with an error.
  // Note: This will fail the test if the promise succeeds.
  auto ScrollToExpectingError(base::Value::Dict selector,
                              glic::mojom::ScrollToErrorReason error_reason) {
    return Steps(CheckJsResult(kGlicContentsElementId,
                               content::JsReplace(R"js(
                        async () => {
                          try {
                            await client.browser.scrollTo({selector: $1});
                          } catch (err) {
                            return err.reason;
                          }
                        }
                      )js",
                                                  std::move(selector)),
                               ::testing::Eq(static_cast<int>(error_reason))));
  }

  // Calls scrollTo() and returns immediately.
  auto ScrollToAsync(base::Value::Dict selector) {
    return Steps(
        ExecuteJs(kGlicContentsElementId,
                  content::JsReplace(
                      R"js(
      () => {
        window.scrollToError = null;
        client.browser.scrollTo({selector: $1}).catch(e => {
          window.scrollToError = e.reason;
        });
      }
    )js",
                      std::move(selector)),
                  InteractiveBrowserTestApi::ExecuteJsMode::kFireAndForget));
  }

  // Should be used in combination with ScrollToAsync() above. Waits until
  // the scrollTo call is rejected with an error other than kNotSupported.
  auto WaitForScrollToError(glic::mojom::ScrollToErrorReason error_reason) {
    return Steps(WaitForJsResult(
        kGlicContentsElementId, "() => window.scrollToError",
        ::testing::AllOf(IsTruthy(),
                         ::testing::Eq(static_cast<int>(error_reason)))));
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

  // Checks if the currently focused tab (according to GlicFocusedTabManager) is
  // `web_contents_id`, or waits until it is. Set `web_contents_id` to
  // std::nullopt to wait until no tab is in focus.
  auto WaitUntilGlicFocusedTabIs(
      std::optional<ui::ElementIdentifier> web_contents_id) {
    return Check([&, web_contents_id]() {
      GlicKeyedService* glic_service =
          glic::GlicKeyedServiceFactory::GetGlicKeyedService(
              browser()->GetProfile());
      content::WebContents* web_contents = nullptr;
      if (web_contents_id) {
        auto* tracked_element =
            ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
                web_contents_id.value());
        web_contents =
            InteractiveBrowserTest::AsInstrumentedWebContents(tracked_element)
                ->web_contents();
      }
      if (glic_service->GetFocusedTabData().focus() == web_contents) {
        return true;
      }
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      auto subscription =
          glic_service->AddFocusedTabChangedCallback(base::BindLambdaForTesting(
              [&run_loop, glic_service, web_contents](FocusedTabData) {
                if (glic_service->GetFocusedTabData().focus() == web_contents) {
                  run_loop.Quit();
                  return;
                }
              }));
      run_loop.Run();
      return true;
    });
  }

  base::Value::Dict ExactTextSelector(std::string exact_text) {
    return base::Value::Dict().Set("exactText",
                                   base::Value::Dict()  //
                                       .Set("text", exact_text));
  }

  base::Value::Dict TextFragmentSelector(std::string text_start,
                                         std::string text_end) {
    return base::Value::Dict().Set("textFragment",
                                   base::Value::Dict()  //
                                       .Set("textStart", text_start)
                                       .Set("textEnd", text_end));
  }

  FakeAnnotationAgentContainer* fake_service() { return fake_service_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeAnnotationAgentContainer> fake_service_;
  base::CallbackListSubscription focused_tab_change_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, ScrollToExactText) {
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ScrollTo(ExactTextSelector("Some text")),
                  WaitForJsResult(kActiveTabId, "() => did_scroll"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, ScrollToTextFragment) {
  RunTestSequence(InstrumentTab(kActiveTabId),
                  NavigateWebContents(
                      kActiveTabId, embedded_test_server()->GetURL(
                                        "/scrollable_page_with_content.html")),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  ScrollTo(TextFragmentSelector("Some", "text")),
                  WaitForJsResult(kActiveTabId, "() => did_scroll"));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, NoMatchFound) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),
      NavigateWebContents(
          kActiveTabId,
          embedded_test_server()->GetURL("/scrollable_page_with_content.html")),
      OpenGlicWindow(GlicWindowMode::kAttached),
      ScrollToExpectingError(ExactTextSelector("Text does not exist"),
                             glic::mojom::ScrollToErrorReason::kNoMatchFound));
}

// Runs a navigation while a scrollTo() request is being processed.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       NavigationAfterScrollToRequest) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),                //
      OpenGlicWindow(GlicWindowMode::kAttached),  //
      InsertFakeAnnotationService(),
      ScrollToAsync(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived),
      NavigateWebContents(kActiveTabId,
                          embedded_test_server()->GetURL("/title.html")),
      WaitForScrollToError(
          glic::mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated));
}

// Opens a new tab while a scrollTo() request is being processed (which results
// in the previous tab losing focus).
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       NewTabOpenedAfterScrollToRequest) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),                //
      OpenGlicWindow(GlicWindowMode::kAttached),  //
      InsertFakeAnnotationService(),              //
      ScrollToAsync(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived),
      PressButton(kNewTabButtonElementId),
      WaitForScrollToError(
          glic::mojom::ScrollToErrorReason::kFocusedTabChangedOrNavigated));
}

// Opens a new window while the GlicWindow is attached to the previous window.
// This results in no tab being considered as focused by Glic.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, NoFocusedTab) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kAttached),  //
      InsertFakeAnnotationService(),              //
      Do([&]() { CreateBrowser(browser()->profile()); }),
      WaitUntilGlicFocusedTabIs(std::nullopt),
      ScrollToExpectingError(ExactTextSelector("does not matter"),
                             glic::mojom::ScrollToErrorReason::kNoFocusedTab));
}

// Sends a second scrollTo() request before the first request finishes
// processing.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest, SecondScrollToRequest) {
  RunTestSequence(InstrumentTab(kActiveTabId),
                  OpenGlicWindow(GlicWindowMode::kAttached),
                  InsertFakeAnnotationService(),
                  ScrollToAsync(ExactTextSelector("Some text")),
                  WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived),
                  ScrollToAsync(ExactTextSelector("Some text again")),
                  WaitForScrollToError(
                      glic::mojom::ScrollToErrorReason::kNewerScrollToCall));
}

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       HighlightKeptAliveAfterScrollToRequestIsComplete) {
  RunTestSequence(
      InstrumentTab(kActiveTabId),                //
      OpenGlicWindow(GlicWindowMode::kAttached),  //
      InsertFakeAnnotationService(),              //
      ScrollToAsync(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived),
      Do([&]() { fake_service()->NotifyAttachment(gfx::Rect(20, 20)); }),
      WaitForEvent(kBrowserViewElementId, kScrollStarted),
      Check([&]() { return fake_service()->HighlightIsActive(); },
            "Agent connection should still be alive."));
}

// Switches focus from the Glic window to the active tab after the scroll
// request completes. The highlight should remain active.
IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerUiTest,
                       HighlightKeptAfterFocusSwitchesFromGlicWindow) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      InstrumentTab(kActiveTabId),                //
      OpenGlicWindow(GlicWindowMode::kAttached),  //
      FocusWebContents(kGlicContentsElementId),   //
      InsertFakeAnnotationService(),              //
      ScrollToAsync(ExactTextSelector("does not matter")),
      WaitForEvent(kBrowserViewElementId, kScrollToRequestReceived),
      Do([&]() { fake_service()->NotifyAttachment(gfx::Rect(20, 20)); }),
      WaitForEvent(kBrowserViewElementId, kScrollStarted),
      FocusWebContents(kActiveTabId),  //
      WaitUntilGlicFocusedTabIs(kActiveTabId),
      Check([&]() { return fake_service()->HighlightIsActive(); },
            "Agent connection should still be alive."));
}

class GlicAnnotationManagerWithScrollToDisabledUiTest
    : public test::InteractiveGlicTest {
 public:
  GlicAnnotationManagerWithScrollToDisabledUiTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kGlicScrollTo);
  }
  ~GlicAnnotationManagerWithScrollToDisabledUiTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicAnnotationManagerWithScrollToDisabledUiTest,
                       ScrollToNotAvailable) {
  RunTestSequence(OpenGlicWindow(GlicWindowMode::kAttached),
                  InAnyContext(CheckJsResult(
                      kGlicContentsElementId,
                      "() => { return !(client.browser.scrollTo); }")));
}
}  // namespace glic::test
