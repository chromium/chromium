// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/fre/glic_fre_dialog_view.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/connection_tracker.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/views/controls/button/button.h"

namespace glic {
namespace {

class GlicFreControllerUiTest : public test::InteractiveGlicTest {
 protected:
  void SetUp() override {
    // TODO(b/399666689): Warming chrome://glic/ seems to allow that page to
    // interfere with chrome://glic-fre/'s <webview>, too, depending which loads
    // first. It's also unclear whether it ought to happen at all before FRE
    // completion. Disable that feature until that can be sorted out.
    features_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{features::kGlicWarming,
                               features::kGlicFreWarming});

    fre_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(fre_server_.InitializeAndListen());

    // Use a page with no favicon as the FRE, suppressing the favicon request
    // and making it easier to reason about whether the preconnect worked as
    // expected (even over HTTP 1.1, which couldn't share a connection between
    // those requests).
    fre_url_ = fre_server_.GetURL("/data_favicon.html");

    InteractiveGlicTestT::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kGlicFreURL, fre_url_.spec());
  }

  net::EmbeddedTestServer& fre_server() { return fre_server_; }
  const GURL& fre_url() { return fre_url_; }

  [[nodiscard]] StepBuilder HoverButton(ElementSpecifier button);

 private:
  base::test::ScopedFeatureList features_;
  net::EmbeddedTestServer fre_server_;
  GURL fre_url_;
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

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kGlicFreHostElementId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kGlicFreContentsElementId);

IN_PROC_BROWSER_TEST_F(GlicFreControllerUiTest, PreconnectOnButtonHover) {
  SetFRECompletion(browser()->profile(), prefs::FreStatus::kNotStarted);
  EXPECT_TRUE(window_controller().fre_controller()->ShouldShowFreDialog());
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
      InstrumentNonTabWebView(kGlicFreHostElementId,
                              GlicFreDialogView::kWebViewElementIdForTesting),
      InstrumentInnerWebContents(kGlicFreContentsElementId,
                                 kGlicFreHostElementId, 0,
                                 /*wait_for_ready=*/true),
      InAnyContext(CheckElement(
          kGlicFreContentsElementId,
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

}  // namespace
}  // namespace glic
