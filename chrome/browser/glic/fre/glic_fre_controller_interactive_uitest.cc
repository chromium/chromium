// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/fre/glic_fre_dialog_view.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "net/test/embedded_test_server/connection_tracker.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/views/controls/button/button.h"
#include "url/gurl.h"

namespace glic {
namespace {

using glic::test::internal::kGlicFreShowingDialogState;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);

const InteractiveBrowserTestApi::DeepQuery kMockFreClientNoThanksButton = {
    "#noThanks"};
const InteractiveBrowserTestApi::DeepQuery kMockFreClientContinueButton = {
    "#continue"};

class FreWebUiStateObserver
    : public ui::test::StateObserver<mojom::FreWebUiState> {
 public:
  explicit FreWebUiStateObserver(GlicFreController* controller)
      : subscription_(controller->AddWebUiStateChangedCallback(
            base::BindRepeating(&FreWebUiStateObserver::OnWebUiStateChanged,
                                base::Unretained(this)))),
        controller_(controller) {}

  void OnWebUiStateChanged(mojom::FreWebUiState new_state) {
    OnStateObserverStateChanged(new_state);
  }

 private:
  base::CallbackListSubscription subscription_;
  raw_ptr<GlicFreController> controller_;
};

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(FreWebUiStateObserver, kFreWebUiState);

// Test base class for tests that need to control the FRE.
class GlicFreControllerUiTestBase : public test::InteractiveGlicTest {
 public:
  void SetUp() override { test::InteractiveGlicTest::SetUp(); }

  void SetUpOnMainThread() override {
    test::InteractiveGlicTest::SetUpOnMainThread();
    SetFRECompletion(browser()->profile(), prefs::FreStatus::kNotStarted);
    EXPECT_TRUE(GetFreController()->ShouldShowFreDialog());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kGlicFreURL, fre_url().spec());
  }

  GlicFreController* GetFreController() {
    return glic_service()->window_controller().fre_controller();
  }

  net::EmbeddedTestServer& fre_server() { return fre_server_; }
  const GURL& fre_url() { return fre_url_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  base::UserActionTester& user_action_tester() { return user_action_tester_; }

 protected:
  base::test::ScopedFeatureList features_;
  net::EmbeddedTestServer fre_server_;
  GURL fre_url_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
};

class GlicFreControllerUiTest : public GlicFreControllerUiTestBase {
 public:
  void SetUp() override {
    // TODO(b/399666689): Warming chrome://glic/ seems to allow that page to
    // interfere with chrome://glic-fre/'s <webview>, too, depending which loads
    // first. It's also unclear whether it ought to happen at all before FRE
    // completion. Disable that feature until that can be sorted out.
    features_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kGlicWarming,
                               features::kGlicFreWarming});

    fre_server_.AddDefaultHandlers();
    fre_server_.ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));
    ASSERT_TRUE(fre_server_.InitializeAndListen());

    fre_url_ = fre_server_.GetURL("/glic/test_client/fre.html");

    GlicFreControllerUiTestBase::SetUp();
  }

  auto WaitForAndInstrumentGlicFre() {
    MultiStep steps =
        Steps(UninstrumentWebContents(test::kGlicFreContentsElementId, false),
              UninstrumentWebContents(test::kGlicFreHostElementId, false),
              ObserveState(kGlicFreShowingDialogState, GetFreController()),
              InAnyContext(Steps(
                  InstrumentNonTabWebView(
                      test::kGlicFreHostElementId,
                      GlicFreDialogView::kWebViewElementIdForTesting),
                  InstrumentInnerWebContents(test::kGlicFreContentsElementId,
                                             test::kGlicFreHostElementId, 0),
                  WaitForWebContentsReady(test::kGlicFreContentsElementId))),
              WaitForState(kGlicFreShowingDialogState, true),
              StopObservingState(kGlicFreShowingDialogState));

    AddDescriptionPrefix(steps, "WaitForAndInstrumentGlicFre");
    return steps;
  }

  auto ForceInvalidateAccount() {
    return Do([this]() { InvalidateAccount(GetFreController()->profile()); });
  }

  auto ForceReauthAccount() {
    return Do([this]() { ReauthAccount(GetFreController()->profile()); });
  }

  auto CheckFreDialogIsShowing(bool is_showing) {
    return CheckResult(
        [this]() { return GetFreController()->IsShowingDialog() == true; },
        is_showing, "CheckFreDialogIsShowing");
  }

  // Ensures a mock fre button is present and then clicks it. Works even if the
  // element is off-screen.
  auto ClickMockFreElement(
      const WebContentsInteractionTestUtil::DeepQuery& where,
      const bool click_closes_window = false) {
    auto steps =
        Steps(WaitForElementVisible(test::kGlicFreContentsElementId, {"body"}),
              ExecuteJsAt(
                  test::kGlicFreContentsElementId, where, "(el)=>el.click()",
                  click_closes_window
                      ? InteractiveBrowserTestApi::ExecuteJsMode::kFireAndForget
                      : InteractiveBrowserTestApi::ExecuteJsMode::
                            kWaitForCompletion));

    AddDescriptionPrefix(steps, "ClickMockFreElement");
    return steps;
  }

  [[nodiscard]] StepBuilder HoverButton(ElementSpecifier button);
};

ui::test::InteractiveTestApi::StepBuilder GlicFreControllerUiTest::HoverButton(
    ElementSpecifier button) {
  // Using MouseMoveTo to simulate hover seems to be very unreliable on Mac and
  // flaky on other platforms. Just tell the button it's hovered.
  // See also crbug.com/358199067.
  return WithElement(button, [](ui::TrackedElement* el) {
    AsView<views::Button>(el)->SetState(views::Button::STATE_HOVERED);
  });
}

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<size_t>,
                                    kAcceptedSocketCount);

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest, PreconnectOnButtonHover) {
  EXPECT_TRUE(predictors::IsPreconnectAllowed(browser()->profile()));

  // The `server_running` handle is held until the end of the function, to keep
  // the server running but also it to gracefully shut down before test
  // teardown.
  net::test_server::ConnectionTracker connection_tracker(&fre_server());
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  RunTestSequence(
      EnsureGlicWindowState("window must be closed",
                            GlicWindowController::State::kClosed),
      WaitForShow(kGlicButtonElementId),
      PollState(kAcceptedSocketCount,
                [&]() { return connection_tracker.GetAcceptedSocketCount(); }),
      WaitForState(kAcceptedSocketCount, 0), HoverButton(kGlicButtonElementId),
      WaitForState(kAcceptedSocketCount, 1), PressButton(kGlicButtonElementId),
      Do([this]() {
        EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Shown"), 1);
        histogram_tester().ExpectUniqueSample(
            "Glic.FRE.InvocationSource",
            mojom::InvocationSource::kTopChromeButton, 1);
      }),
      WaitForAndInstrumentGlicFre(),
      InAnyContext(CheckElement(
          test::kGlicFreContentsElementId,
          [](ui::TrackedElement* el) {
            // Query parameters are added dynamically. Strip those here so that
            // we're only checking the rest (and most importantly, that it is
            // pointing at the server that received the preconnect).
            GURL url = AsInstrumentedWebContents(el)->web_contents()->GetURL();
            GURL::Replacements replacements;
            replacements.ClearQuery();
            replacements.ClearRef();
            return url.ReplaceComponents(replacements);
          },
          fre_url())),
      StopObservingState(kAcceptedSocketCount));

  EXPECT_EQ(connection_tracker.GetAcceptedSocketCount(), 1u);
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest, PressNoThanksButton) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  // Tests that pressing the "No Thanks" button in the FRE closes the FRE
  // dialog, and does not open the glic window.
  RunTestSequence(
      ObserveState(kFreWebUiState,
                   base::BindOnce(&GlicFreControllerUiTest::GetFreController,
                                  base::Unretained(this))),
      PressButton(kGlicButtonElementId), WaitForAndInstrumentGlicFre(),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
      ClickMockFreElement(kMockFreClientNoThanksButton, true),
      WaitForHide(GlicFreDialogView::kWebViewElementIdForTesting),
      CheckFreDialogIsShowing(false), CheckControllerHasWidget(false),
      Do([this]() {
        EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.NoThanks"), 1);
        EXPECT_EQ(
            user_action_tester().GetActionCount("Glic.Fre.ReadyPanelClosed"),
            1);
        histogram_tester().ExpectUniqueSample(
            "Glic.FreModalWebUiState.FinishState2",
            mojom::FreWebUiState::kReady, 1);
      }));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest, PressContinueButton) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  // Tests that pressing the "Continue" button in the FRE closes the FRE
  // dialog, and opens the glic window.
  RunTestSequence(
      ObserveState(kFreWebUiState,
                   base::BindOnce(&GlicFreControllerUiTest::GetFreController,
                                  base::Unretained(this))),
      PressButton(kGlicButtonElementId), WaitForAndInstrumentGlicFre(),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
      ClickMockFreElement(kMockFreClientContinueButton, true),
      WaitForHide(GlicFreDialogView::kWebViewElementIdForTesting),
      CheckFreDialogIsShowing(false), CheckControllerHasWidget(true),
      Do([this]() {
        EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.Accept"), 1);
        histogram_tester().ExpectUniqueSample(
            "Glic.FreModalWebUiState.FinishState2",
            mojom::FreWebUiState::kReady, 1);
      }));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest,
                       InvalidatedAccountSignInOnGlicFreOpenFlow) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  // Tests that, when FRE is required and the glic button is pressed while
  // signed out, the FRE dialog is shown after reauthorization is completed.
  RunTestSequence(
      ObserveState(kFreWebUiState,
                   base::BindOnce(&GlicFreControllerUiTest::GetFreController,
                                  base::Unretained(this))),
      ForceInvalidateAccount(), PressButton(kGlicButtonElementId),
      CheckFreDialogIsShowing(false), InstrumentTab(kFirstTab),
      WaitForWebContentsReady(kFirstTab),
      // Without a pause here, we will 'sign-in' before the callback is
      // registered to listen for it. This isn't a bug because it takes real
      // users finite time to actually sign-in.
      Wait(base::Milliseconds(500)), ForceReauthAccount(),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
      StopObservingState(kFreWebUiState));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest,
                       ShowsErrorPanelOnCookieSyncFailure) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  // Configure the injected TestCookieSynchronizer to fail for the FRE.
  glic_test_environment()
      .GetService(browser()->profile())
      ->SetResultForFutureCookieSyncInFre(false);

  RunTestSequence(
      ObserveState(kFreWebUiState,
                   base::BindOnce(&GlicFreControllerUiTest::GetFreController,
                                  base::Unretained(this))),
      PressButton(kGlicButtonElementId),
      WaitForShow(GlicFreDialogView::kWebViewElementIdForTesting),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kError), Do([this]() {
        histogram_tester().ExpectUniqueSample(
            "Glic.FreErrorStateReason",
            FreErrorStateReason::kErrorResyncingCookies, 1);
      }),
      InstrumentNonTabWebView(test::kGlicFreHostElementId,
                              GlicFreDialogView::kWebViewElementIdForTesting),
      InAnyContext(WaitForElementVisible(test::kGlicFreHostElementId,
                                         {"#errorPanel:not([hidden])"})));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest, ShowsErrorPanelOnInvalidAuth) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  RunTestSequence(
      ObserveState(kFreWebUiState,
                   base::BindOnce(&GlicFreControllerUiTest::GetFreController,
                                  base::Unretained(this))),
      PressButton(kGlicButtonElementId), ForceInvalidateAccount(),
      WaitForShow(GlicFreDialogView::kWebViewElementIdForTesting),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kError), Do([this]() {
        histogram_tester().ExpectUniqueSample(
            "Glic.FreErrorStateReason", FreErrorStateReason::kSignInRequired,
            1);
      }),
      InstrumentNonTabWebView(test::kGlicFreHostElementId,
                              GlicFreDialogView::kWebViewElementIdForTesting),
      InAnyContext(WaitForElementVisible(test::kGlicFreHostElementId,
                                         {"#errorPanel:not([hidden])"})));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest,
                       RecordTerminationStatusOnWebUICrash) {
  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();
  RunTestSequence(
      ObserveState(kFreWebUiState,
                   base::BindOnce(&GlicFreControllerUiTest::GetFreController,
                                  base::Unretained(this))),
      PressButton(kGlicButtonElementId), WaitForAndInstrumentGlicFre(),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
      // Crash the renderer process for the FRE WebUI.
      Do([this]() {
        content::WebContents* fre_web_contents =
            GetFreController()->GetWebContents();
        ASSERT_TRUE(fre_web_contents);
        content::RenderProcessHost* rph =
            fre_web_contents->GetPrimaryMainFrame()->GetProcess();
        ASSERT_TRUE(rph);
        rph->Shutdown(content::RESULT_CODE_KILLED);
      }),
      WaitForHide(GlicFreDialogView::kWebViewElementIdForTesting),
      InAnyContext(Do([this]() {
        histogram_tester().ExpectUniqueSample(
            "Glic.Fre.WebUITerminationStatus",
            base::TERMINATION_STATUS_PROCESS_WAS_KILLED, 1);
      })));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest,
                       RecordsWebUiAndWebContentLoadTimeHistograms) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  RunTestSequence(
      ObserveState(kFreWebUiState,
                   base::BindOnce(&GlicFreControllerUiTest::GetFreController,
                                  base::Unretained(this))),
      PressButton(kGlicButtonElementId), WaitForAndInstrumentGlicFre(),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
      InAnyContext(Do([&]() {
        histogram_tester().ExpectTotalCount("Glic.Fre.WidgetCreationTime", 1);
        histogram_tester().ExpectTotalCount("Glic.Fre.WebUiFrameworkLoadTime",
                                            1);
        histogram_tester().ExpectTotalCount("Glic.Fre.WebClientLoadTime", 1);
      })));
}

class GlicFreControllerUiHttpErrorTest : public GlicFreControllerUiTestBase {
 public:
  void SetUp() override {
    features_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kGlicWarming,
                               features::kGlicFreWarming});

    fre_server_.AddDefaultHandlers();
    // Register a handler that will return a 502 error.
    fre_server_.RegisterRequestHandler(base::BindRepeating(
        [](const GURL& url, const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == url.path()) {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_BAD_GATEWAY);
            return response;
          }
          return nullptr;
        },
        fre_url_));
    ASSERT_TRUE(fre_server_.InitializeAndListen());

    fre_url_ = fre_server_.GetURL("/glic/test_client/fre.html");

    GlicFreControllerUiTestBase::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiHttpErrorTest,
                       ShowsErrorPanelOnHttpError) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  RunTestSequence(
      ObserveState(
          kFreWebUiState,
          base::BindOnce(&GlicFreControllerUiHttpErrorTest::GetFreController,
                         base::Unretained(this))),
      PressButton(kGlicButtonElementId),
      WaitForShow(GlicFreDialogView::kWebViewElementIdForTesting),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kError), Do([this]() {
        EXPECT_EQ(
            user_action_tester().GetActionCount("Glic.Fre.WebviewLoadAborted"),
            1);
        histogram_tester().ExpectUniqueSample(
            "Glic.Fre.WebviewLoadAbortReason",
            10  // GlicFreWebviewLoadAbortReason::ERR_HTTP_RESPONSE_CODE_FAILURE
            ,
            1);
      }),
      InstrumentNonTabWebView(test::kGlicFreHostElementId,
                              GlicFreDialogView::kWebViewElementIdForTesting),
      InAnyContext(WaitForElementVisible(test::kGlicFreHostElementId,
                                         {"#errorPanel:not([hidden])"})));
}

class GlicFreControllerUiTimeoutTest : public GlicFreControllerUiTestBase {
 public:
  void SetUp() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlic,
         {{"glic-max-loading-time-ms", "0"},
          {"glic-min-loading-time-ms", "0"},
          {"glic-pre-loading-time-ms", "0"}}}};

    // TODO(b/399666689): Warming chrome://glic/ seems to allow that page to
    // interfere with chrome://glic-fre/'s <webview>, too, depending which loads
    // first. It's also unclear whether it ought to happen at all before FRE
    // completion. Disable that feature until that can be sorted out.
    features_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{features::kGlicWarming,
                               features::kGlicFreWarming});

    fre_server_.AddDefaultHandlers();
    fre_server_.ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));
    ASSERT_TRUE(fre_server_.InitializeAndListen());

    fre_url_ = fre_server_.GetURL("/glic/test_client/fre.html");

    GlicFreControllerUiTestBase::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTimeoutTest,
                       ShowsErrorPanelOnLoadingTimeout) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  RunTestSequence(
      ObserveState(
          kFreWebUiState,
          base::BindOnce(&GlicFreControllerUiTimeoutTest::GetFreController,
                         base::Unretained(this))),
      PressButton(kGlicButtonElementId),
      WaitForShow(GlicFreDialogView::kWebViewElementIdForTesting),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kError),
      InAnyContext(Do([this]() {
        EXPECT_EQ(
            user_action_tester().GetActionCount("Glic.Fre.WebviewLoadTimedOut"),
            1);
        histogram_tester().ExpectUniqueSample(
            "Glic.FreErrorStateReason",
            glic::FreErrorStateReason::kTimeoutExceeded, 1);
        histogram_tester().ExpectUniqueSample(
            "Glic.Fre.WebviewLoadAbortReason",
            9  // GlicFreWebviewLoadAbortReason::ERR_TIMED_OUT
            ,
            1);
      })),
      InstrumentNonTabWebView(test::kGlicFreHostElementId,
                              GlicFreDialogView::kWebViewElementIdForTesting),
      InAnyContext(WaitForElementVisible(test::kGlicFreHostElementId,
                                         {"#errorPanel:not([hidden])"})));
}

// TODO(crbug.com/427261741#comment11) Test is flaky on all platforms.
IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest, DISABLED_CloseWithEsc) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  RunTestSequence(
      ObserveState(kFreWebUiState,
                   base::BindOnce(&GlicFreControllerUiTest::GetFreController,
                                  base::Unretained(this))),
      PressButton(kGlicButtonElementId), WaitForAndInstrumentGlicFre(),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
      SendKeyPress(GlicFreDialogView::kWebViewElementIdForTesting,
                   ui::VKEY_ESCAPE, ui::EF_NONE),
      WaitForHide(GlicFreDialogView::kWebViewElementIdForTesting),
      CheckFreDialogIsShowing(false), CheckControllerHasWidget(false),
      InAnyContext(Do([&]() {
        EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.CloseWithEsc"),
                  1);
      })));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest, CloseByClosingHostTab) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  RunTestSequence(
      ObserveState(kFreWebUiState,
                   base::BindOnce(&GlicFreControllerUiTest::GetFreController,
                                  base::Unretained(this))),
      // Open a new tab before showing the FRE.
      Do([this]() {
        NavigateParams params(browser(), GURL("about:blank"),
                              ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
        params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
        Navigate(&params);
      }),
      PressButton(kGlicButtonElementId), WaitForAndInstrumentGlicFre(),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady), Do([this]() {
        // Assert that the FRE dialog is showing before closing the tab.
        EXPECT_TRUE(GetFreController()->IsShowingDialog());
        EXPECT_EQ(user_action_tester().GetActionCount(
                      "Glic.Fre.CloseByClosingHostTab"),
                  0);
      }),
      Do([&]() {
        // Close the second tab (the one with the FRE).
        TabStripModel* tab_strip_model = browser()->tab_strip_model();
        ASSERT_EQ(tab_strip_model->count(), 2);
        tab_strip_model->CloseWebContentsAt(1,
                                            TabCloseTypes::CLOSE_USER_GESTURE);
      }),
      WaitForHide(GlicFreDialogView::kWebViewElementIdForTesting),
      CheckFreDialogIsShowing(false), CheckControllerHasWidget(false),
      // Check the action count after the tab is closed.
      InAnyContext(Do([&]() {
        EXPECT_EQ(user_action_tester().GetActionCount(
                      "Glic.Fre.CloseByClosingHostTab"),
                  1);
      })));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest, CloseWithToggle) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  RunTestSequence(
      ObserveState(kFreWebUiState,
                   base::BindOnce(&GlicFreControllerUiTest::GetFreController,
                                  base::Unretained(this))),
      PressButton(kGlicButtonElementId), WaitForAndInstrumentGlicFre(),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
      PressButton(kGlicButtonElementId),
      WaitForHide(GlicFreDialogView::kWebViewElementIdForTesting),
      CheckFreDialogIsShowing(false), CheckControllerHasWidget(false),
      InAnyContext(Do([&]() {
        EXPECT_EQ(
            user_action_tester().GetActionCount("Glic.Fre.CloseWithToggle"), 1);
      })));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest, CloseWithXButton) {
  auto server_running = fre_server().StartAcceptingConnectionsAndReturnHandle();

  const InteractiveBrowserTestApi::DeepQuery kMockFreClientCloseButton = {
      "#close"};

  RunTestSequence(
      ObserveState(kFreWebUiState,
                   base::BindOnce(&GlicFreControllerUiTest::GetFreController,
                                  base::Unretained(this))),
      PressButton(kGlicButtonElementId), WaitForAndInstrumentGlicFre(),
      WaitForState(kFreWebUiState, mojom::FreWebUiState::kReady),
      ClickMockFreElement(kMockFreClientCloseButton, true),
      WaitForHide(GlicFreDialogView::kWebViewElementIdForTesting),
      CheckFreDialogIsShowing(false), CheckControllerHasWidget(false),
      InAnyContext(Do([&]() {
        EXPECT_EQ(user_action_tester().GetActionCount("Glic.Fre.CloseWithX"),
                  1);
        histogram_tester().ExpectUniqueSample(
            "Glic.FreModalWebUiState.FinishState2",
            mojom::FreWebUiState::kReady, 1);
      })));
}

}  // namespace
}  // namespace glic
