// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/session_ui_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_interactive_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/browser/ui/views/dictation/dictation_overlay_view.h"
#include "chrome/browser/ui/views/dictation/ui_state.h"
#include "chrome/browser/ui/views/dictation/waveform_view.h"
#include "chrome/browser/ui/views/dictation/waveform_view_button.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "extensions/common/switches.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/button/label_button.h"
#include "url/gurl.h"

namespace dictation {

class SessionStateObserver : public ui::test::StateObserver<SessionState> {
 public:
  explicit SessionStateObserver(SessionController* controller) {
    subscription_ = controller->AddSessionStateChangedCallback(
        base::BindRepeating(&SessionStateObserver::OnStateObserverStateChanged,
                            base::Unretained(this)));
  }

  SessionState GetStateObserverInitialState() const override {
    return SessionState::kInactive;
  }

 private:
  base::CallbackListSubscription subscription_;
};

DECLARE_STATE_IDENTIFIER_VALUE(SessionStateObserver, kSessionStateIdentifier);
DEFINE_STATE_IDENTIFIER_VALUE(SessionStateObserver, kSessionStateIdentifier);

class DictationSessionUiImplBrowserTest
    : public DictationInteractiveBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  DictationSessionUiImplBrowserTest()
      : DictationInteractiveBrowserTestBase(GetParam()) {}
  ~DictationSessionUiImplBrowserTest() override = default;

 protected:
  auto CloseTab(int index) {
    return Do([this, index]() {
      browser()->GetTabStripModel()->CloseWebContentsAt(
          index, TabCloseTypes::CLOSE_USER_GESTURE);
    });
  }

  auto MoveTabToWindow(BrowserWindowInterface* source,
                       BrowserWindowInterface* target,
                       int index) {
    return Do([source, target, index]() {
      chrome::MoveTabsToExistingWindow(source, target, {index});
    });
  }

  auto GetSessionState() {
    return [this]() {
      return dictation_service().session_controller()->GetState();
    };
  }

  auto ObserveSessionStateChanges() {
    return ObserveState(kSessionStateIdentifier, [this]() {
      return dictation_service().session_controller();
    });
  }

  auto WaitForSessionState(SessionState state) {
    return WaitForState(kSessionStateIdentifier, state);
  }

  auto HasAttachedStreamProvider() {
    return [this]() {
      return dictation_service()
                 .session_controller()
                 ->attached_stream_provider() != nullptr;
    };
  }

  auto CheckShowingToast(ToastId toast_id, bool showing) {
    return Check([this, toast_id, showing]() {
      ToastController* const toast_controller =
          ToastController::From(browser());
      CHECK(toast_controller);
      const bool is_showing_toast =
          toast_controller->IsShowingToast() &&
          toast_controller->GetCurrentToastId() == toast_id;
      return is_showing_toast == showing;
    });
  }

  auto CheckShowingDictationErrorToast(bool showing) {
    return CheckShowingToast(ToastId::kDictationError, showing);
  }

  auto CheckShowingDictationNoMicrophoneErrorToast(bool showing) {
    return CheckShowingToast(ToastId::kDictationNoMicrophoneError, showing);
  }

  auto CheckShowingDictationStoppedToast(bool showing) {
    return CheckShowingToast(ToastId::kDictationStopped, showing);
  }

  auto StartDictationStream(DictationStreamStartTrigger trigger) {
    return Do([this, trigger]() {
      dictation_service().session_controller()->StartDictationStream(
          DefaultInPageTarget(web_contents()), trigger);
    });
  }

  auto LookupTargetElementBounds(ui::ElementIdentifier web_contents_id,
                                 std::string_view selector,
                                 gfx::Rect& target_bounds) {
    return WithElement(
        web_contents_id, [&target_bounds, selector = std::string(selector)](
                             ui::TrackedElement* el) {
          target_bounds =
              AsInstrumentedWebContents(el)->GetElementBoundsInScreen(selector);
        });
  }

  auto CheckElementWithinBounds(ui::ElementIdentifier element_id,
                                const gfx::Rect& target_bounds) {
    return InAnyContext(
        CheckElement(element_id, [&target_bounds](ui::TrackedElement* el) {
          const views::View* const view = AsView(el);
          const gfx::Rect view_bounds = view->GetBoundsInScreen();
          return target_bounds.Contains(view_bounds.origin());
        }));
  }

  auto ToggleFullscreen() {
    return Do(
        [this]() { ui_test_utils::ToggleFullscreenModeAndWait(browser()); });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       SessionStateUpdatesToggleButton) {
  if (GetParam()) {
    GTEST_SKIP() << "UI state behaviour differs in this config.";
  }

  // clang-format off
  RunTestSequence(
    StartSession(),
    ObserveSessionStateChanges(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // kStreamInitializing.
    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true),

    // kTranscribing.
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    WaitForShow(DictationBubbleUi::kWaveformElementIdForTesting),
    CheckResult(GetSessionState(), SessionState::kTranscribing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true),

    // kFinalizing.
    Do([this] {
      dictation_service().session_controller()->EndDictationStream(
          DictationStreamEndTrigger::kTest);
    }),
    CheckResult(GetSessionState(), SessionState::kFinalizing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, false),

    // kInactive
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete),
    WaitForSessionState(SessionState::kInactive),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Start"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest, UpdateAudioLevel) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),
    Do([this]{
      SessionUi* ui = session_ui();
      ASSERT_TRUE(ui);
      ui->UpdateAudioLevel(0.5f);
    })
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       ToastIsActivatableAfterCreation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),
    // Ensure that showing the bubble did not steal focus from the page.
    CheckView(DictationBubbleUi::kViewElementIdForTesting,
              [](views::View* view) {
                views::Widget* widget = view->GetWidget();
                return widget && !widget->IsActive();
              }),
    // Ensure that the bubble can be activated. This is needed on Windows,
    // otherwise we'd discard mouse activation messages from the OS and the
    // buttons wouldn't be clickable. See https://crbug.com/542199776
    CheckView(DictationBubbleUi::kViewElementIdForTesting,
              [](views::View* view) {
                views::Widget* widget = view->GetWidget();
                return widget && widget->widget_delegate() &&
                       widget->widget_delegate()->CanActivate();
              })
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       EndSessionTearsDownUI) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    PressButton(DictationBubbleUi::kCloseButtonElementIdForTesting),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    Check([this]{ return session_ui() == nullptr; })
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       DoneButtonEndsActiveStream) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    CheckResult(GetSessionState(), SessionState::kTranscribing),

    PressButton(DictationBubbleUi::kToggleButtonElementIdForTesting),

    // TODO(b/525943882): Currently the controller immediately disposes of the
    // stream but it should really be going into a finalization state. Update
    // once this is fixed.
    CheckResult(GetSessionState(), testing::Ne(SessionState::kTranscribing)),
    CheckResult(HasAttachedStreamProvider(), false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       ToggleStartStopFromUi) {
  if (GetParam()) {
    GTEST_SKIP()
        << "Multiple streams per session are not possible in this config.";
  }

  // clang-format off
  RunTestSequence(
    // Open the session ui.
    StartSession(),
    ObserveSessionStateChanges(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Move to transcribing state
    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    CheckResult(GetSessionState(), SessionState::kTranscribing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),

    // Click "Done".
    PressButton(DictationBubbleUi::kToggleButtonElementIdForTesting),
    CheckResult(GetSessionState(), SessionState::kFinalizing),
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete),
    WaitForSessionState(SessionState::kInactive),

    // The button should become "Start"; click it.
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Start"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true),
    PressButton(DictationBubbleUi::kToggleButtonElementIdForTesting),

    // Ensure the button becomes "Done" again and a new stream was started.
    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true),
    CheckResult(HasAttachedStreamProvider(), true),

    // Click "Done" to end the second stream.
    PressButton(DictationBubbleUi::kToggleButtonElementIdForTesting),
    CheckResult(GetSessionState(), SessionState::kFinalizing),
    CheckResult(HasAttachedStreamProvider(), false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest, TabSwitchHidesUI) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->GetTabStripModel()->ActivateTabAt(0);

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Switch to the second tab and ensure the UI hides.
    SelectTab(kTabStripElementId, 1),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest, CloseTabEndsSession) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->GetTabStripModel()->ActivateTabAt(0);

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Close the active tab (tab 0).
    CloseTab(0),

    // The UI should hide and the session should be ended.
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       UiFollowsDetachedTab) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->GetTabStripModel()->ActivateTabAt(0);

  // Create a second browser window.
  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Move the dictating tab to the second window.
    MoveTabToWindow(browser(), second_browser, 0),

    // Ensure the UI follows the tab to the new window.
    InContext(BrowserElements::From(second_browser)->GetContext(),
              WaitForShow(DictationBubbleUi::kViewElementIdForTesting)),
    InContext(BrowserElements::From(browser())->GetContext(),
              EnsureNotPresent(DictationBubbleUi::kViewElementIdForTesting)),
    CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       ReparentTabBetweenWindowsDoesNotCrash) {
  // Add a second tab with the first tab in the foreground so the initial
  // browser window does not close when its active tab is detached.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->GetTabStripModel()->ActivateTabAt(0);

  // Create a second browser window.
  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Move the dictating tab to the second window.
    MoveTabToWindow(browser(), second_browser, 0),

    // Move the tab back to the original window.
    MoveTabToWindow(second_browser, browser(), 1),

    // Verify the session remains active and UI is present without crashing.
    InContext(BrowserElements::From(browser())->GetContext(),
              WaitForShow(DictationBubbleUi::kViewElementIdForTesting)),
    CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       BackgroundTabActivationEndsSession) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->GetTabStripModel()->ActivateTabAt(0);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  // clang-format off
  RunTestSequence(
    InstrumentTab(kFirstWebContentsElementId),
    NavigateWebContents(kFirstWebContentsElementId, url),
    StartSessionWithTarget(kFirstWebContentsElementId, "#text_id"),
    ObserveSessionStateChanges(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),
    InAnyContext(WaitForShow(DictationOverlayView::kViewElementIdForTesting)),

    // Switch to the second tab. The session should be ended but only after
    // finalization.
    SelectTab(kTabStripElementId, 1),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    InAnyContext(
        EnsureNotPresent(DictationOverlayView::kViewElementIdForTesting)),
    CheckHasSession(true),
    CheckResult(GetSessionState(), SessionState::kFinalizing),

    // Once the finalization completes the sesison should be ended.
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete),
    WaitForSessionState(SessionState::kInactive),
    CheckHasSession(false),

    // Switch back to the first tab and ensure the UI does not reappear.
    SelectTab(kTabStripElementId, 0),
    EnsureNotPresent(DictationBubbleUi::kViewElementIdForTesting),
    InAnyContext(
        EnsureNotPresent(DictationOverlayView::kViewElementIdForTesting))
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       TabSwitchShowsDictationStoppedToast) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->GetTabStripModel()->ActivateTabAt(0);

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Switch to the second tab and verify that the Dictation stopped toast
    // is shown.
    SelectTab(kTabStripElementId, 1),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    CheckShowingDictationStoppedToast(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       SwitchBackToDictatingTabDuringFinalization) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->GetTabStripModel()->ActivateTabAt(0);

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Switch to the second tab. The session should be finalizing.
    SelectTab(kTabStripElementId, 1),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(true),
    CheckResult(GetSessionState(), SessionState::kFinalizing),

    // Switch back to the first tab (dictating tab) while still finalizing.
    SelectTab(kTabStripElementId, 0),
    EnsureNotPresent(DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(true),
    CheckResult(GetSessionState(), SessionState::kFinalizing)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest, ShowsToastOnError) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Ensure no toast is showing initially.
    CheckShowingDictationErrorToast(false),

    // Inject failure state via the extension API!
    ExtensionAPISetStreamState(ExtensionStreamState::kFailed),

    // The active stream failure should end the session, hide the bubble, and
    // show the error toast.
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    CheckShowingDictationErrorToast(true),
    Check([this]{ return session_ui() == nullptr; })
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       FailedStreamInitAllowsOngoingFinalizing) {
  base::WeakPtr<ListenerStreamProvider> finalizing_stream;
  StreamId finalizing_stream_id;

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    CheckResult(GetSessionState(), SessionState::kTranscribing),

    // End active stream to transition the first stream to finalization.
    Do([&finalizing_stream, &finalizing_stream_id, this] {
      finalizing_stream = last_started_provider_;
      ASSERT_NE(finalizing_stream, nullptr);
      finalizing_stream_id = finalizing_stream->stream_id_for_testing();
      dictation_service().session_controller()->EndDictationStream(
          DictationStreamEndTrigger::kTest);
    }),
    CheckResult(GetSessionState(), SessionState::kFinalizing),
    CheckResult(HasAttachedStreamProvider(), false),

    // Start a second stream while the first stream is finalizing.
    StartDictationStream(DictationStreamStartTrigger::kFocusChange),
    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    CheckResult(HasAttachedStreamProvider(), true),
    ExtensionAPIWaitForStreamStart(),

    CheckShowingDictationErrorToast(false),

    // Simulate the second stream failing to initialize.
    ExtensionAPISetStreamState(ExtensionStreamState::kFailed),

    // The second stream failure should show the error toast. However, because
    // the first stream is still finalizing, the session should remain active in
    // the finalizing state rather than immediately ending.
    CheckShowingDictationErrorToast(true),
    CheckHasSession(true),
    CheckResult(GetSessionState(), SessionState::kFinalizing),
    CheckResult(HasAttachedStreamProvider(), false),
    EnsurePresent(DictationBubbleUi::kViewElementIdForTesting),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, false),

    // The finalizing stream should still be able to accept final text.
    ExtensionAPIUpdateTranscription(finalizing_stream_id,
                                    ExtensionTranscriptionType::kFinal,
                                    "Final text"),
    Check([&finalizing_stream] {
      return finalizing_stream &&
             finalizing_stream->GetLatestTranscriptionForTesting() ==
                 "Final text" &&
             finalizing_stream->IsTranscriptionFinalForTesting();
    }),

    // Once the finalizing stream completes, the session should end.
    ExtensionAPISetStreamState(finalizing_stream_id,
                               ExtensionStreamState::kComplete),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(false)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       NavigationEndsSession) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Simulate navigation.
    Do([this]{
      ASSERT_TRUE(chrome_test_utils::NavigateToURL(web_contents(),
                                                   GURL("about:blank")));
    }),

    // The UI should hide and the session should be ended immediately.
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(false),
    CheckShowingDictationStoppedToast(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest, TabCrashEndsSession) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    Do([this]{
      web_contents()
          ->GetPrimaryMainFrame()
          ->GetProcess()
          ->Shutdown(content::RESULT_CODE_KILLED);
    }),

    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(false),
    CheckShowingDictationStoppedToast(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       SecondWindowInvokesDictationMovesUI) {
  // Create a second browser window.
  BrowserWindowInterface* second_browser =
      CreateBrowser(browser()->GetProfile());
  content::WebContents* window2_contents =
      second_browser->GetTabStripModel()->GetActiveWebContents();
  ASSERT_NE(window2_contents, nullptr);

  // clang-format off
  RunTestSequence(
    // Start dictation session in Window 1.
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Invoke dictation from the context menu in Window 2.
    Do([this, window2_contents] {
      dictation_service().ContextMenuHandler(
          DefaultInPageTarget(window2_contents));
    }),

    // Verify UI is showing in Window 2 and not showing in Window 1.
    InContext(BrowserElements::From(second_browser)->GetContext(),
              WaitForShow(DictationBubbleUi::kViewElementIdForTesting)),
    InContext(BrowserElements::From(browser())->GetContext(),
              EnsureNotPresent(DictationBubbleUi::kViewElementIdForTesting)),
    CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       OverlayButtonAppearsOnSessionStart) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  gfx::Rect target_bounds;

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    InAnyContext(WaitForShow(DictationOverlayView::kViewElementIdForTesting)),
    LookupTargetElementBounds(kWebContentsElementId, "#text_id", target_bounds),
    CheckElementWithinBounds(DictationOverlayView::kViewElementIdForTesting,
                             target_bounds)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       OverlayPositionUpdatedOnFullscreen) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  gfx::Rect target_bounds;

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    InAnyContext(WaitForShow(DictationOverlayView::kViewElementIdForTesting)),
    LookupTargetElementBounds(kWebContentsElementId, "#text_id", target_bounds),
    CheckElementWithinBounds(DictationOverlayView::kViewElementIdForTesting,
                             target_bounds),
    ToggleFullscreen(),
    LookupTargetElementBounds(kWebContentsElementId, "#text_id", target_bounds),
    CheckElementWithinBounds(DictationOverlayView::kViewElementIdForTesting,
                             target_bounds),
    ToggleFullscreen(),
    LookupTargetElementBounds(kWebContentsElementId, "#text_id", target_bounds),
    CheckElementWithinBounds(DictationOverlayView::kViewElementIdForTesting,
                             target_bounds)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       OverlayLeftInLastPositionWhenTargetLosesFocus) {
  if (!GetParam()) {
    // At least until crbug.com/552154453 is addressed for the multiple stream
    // mode, this test does not apply, as a new stream should move the overlay.
    GTEST_SKIP() << "Does not apply if focus changes start streams.";
  }

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  gfx::Rect target_bounds;

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    InAnyContext(WaitForShow(DictationOverlayView::kViewElementIdForTesting)),
    LookupTargetElementBounds(kWebContentsElementId, "#text_id", target_bounds),
    // Focus a second textarea, causing the first textarea to lose focus.
    ExecuteJs(kWebContentsElementId,
              "() => {"
              "  const textarea2 = document.createElement('textarea');"
              "  textarea2.id = 'text_id_2';"
              "  document.body.appendChild(textarea2);"
              "  textarea2.focus();"
              "}"),
    // The overlay should remain in its last position.
    CheckElementWithinBounds(DictationOverlayView::kViewElementIdForTesting,
                             target_bounds)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       OverlayButtonUpdatesOnStreamStateChange) {
  if (GetParam()) {
    GTEST_SKIP() << "UI state behaviour differs in this config.";
  }

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    ObserveSessionStateChanges(),
    InAnyContext(WaitForShow(DictationOverlayView::kViewElementIdForTesting)),

    // Initial state (kStreamInitializing): WaveformView shown, others absent.
    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    InAnyContext(WaitForShow(
        DictationOverlayView::kWaveformElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kMicButtonElementIdForTesting)),

    // Transition to kTranscribing: WaveformView shown, others absent.
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    CheckResult(GetSessionState(), SessionState::kTranscribing),
    InAnyContext(WaitForShow(
        DictationOverlayView::kWaveformElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kMicButtonElementIdForTesting)),

    // Transition to kFinalizing: WaveformView shown, others absent.
    Do([this] {
      dictation_service().session_controller()->EndDictationStream(
          DictationStreamEndTrigger::kTest);
    }),
    CheckResult(GetSessionState(), SessionState::kFinalizing),
    InAnyContext(WaitForShow(
        DictationOverlayView::kWaveformElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kMicButtonElementIdForTesting)),

    // Transition to kInactive: Mic icon button shown again, others absent.
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete),
    WaitForSessionState(SessionState::kInactive),
    InAnyContext(WaitForShow(
        DictationOverlayView::kMicButtonElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kWaveformElementIdForTesting))
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       OverlayWaveformReceivesAudioLevelUpdates) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    InAnyContext(WaitForShow(DictationOverlayView::kViewElementIdForTesting)),
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    InAnyContext(WaitForShow(
        DictationOverlayView::kWaveformElementIdForTesting)),
    Do([this] {
      static_cast<SessionUi*>(session_ui())->UpdateAudioLevel(0.05f);
    }),
    InAnyContext(CheckViewProperty(
        DictationOverlayView::kWaveformElementIdForTesting,
        &WaveformViewButton::audio_level_for_testing, 0.05f))
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       OverlayButtonsToggleStreamState) {
  if (GetParam()) {
    GTEST_SKIP()
        << "Multiple streams per session are not possible in this config.";
  }

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    ObserveSessionStateChanges(),
    InAnyContext(WaitForShow(DictationOverlayView::kViewElementIdForTesting)),

    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    InAnyContext(WaitForShow(
        DictationOverlayView::kWaveformElementIdForTesting)),

    // Pressing the waveform button while initializing ends the stream.
    InAnyContext(PressButton(
        DictationOverlayView::kWaveformElementIdForTesting)),
    CheckResult(GetSessionState(), SessionState::kFinalizing),

    ExtensionAPISetStreamState(ExtensionStreamState::kComplete),
    WaitForSessionState(SessionState::kInactive),

    // Pressing the mic button while inactive starts a stream.
    InAnyContext(WaitForShow(
        DictationOverlayView::kMicButtonElementIdForTesting)),
    InAnyContext(PressButton(
        DictationOverlayView::kMicButtonElementIdForTesting)),
    CheckResult(GetSessionState(), SessionState::kStreamInitializing),

    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    CheckResult(GetSessionState(), SessionState::kTranscribing),
    InAnyContext(WaitForShow(
        DictationOverlayView::kWaveformElementIdForTesting)),

    // Pressing the waveform button ends the stream.
    InAnyContext(PressButton(
        DictationOverlayView::kWaveformElementIdForTesting)),
    CheckResult(GetSessionState(), SessionState::kFinalizing)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_P(DictationSessionUiImplBrowserTest,
                       NoMicrophoneErrorShowsDedicatedToast) {
  constexpr int kNoMicrophoneErrorCode =
      static_cast<int>(StreamErrorReason::kNoMicrophone);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    InAnyContext(WaitForShow(DictationBubbleUi::kViewElementIdForTesting)),
    CheckShowingDictationNoMicrophoneErrorToast(false),

    // Extension reports failure with numeric error code for NO_MICROPHONE.
    ExtensionAPISetStreamState(
        ExtensionStreamState::kFailed, kNoMicrophoneErrorCode),

    InAnyContext(WaitForHide(DictationBubbleUi::kViewElementIdForTesting)),
    CheckShowingDictationNoMicrophoneErrorToast(true),
    Check([this] { return session_ui() == nullptr; })
  );
  // clang-format on
}

INSTANTIATE_TEST_SUITE_P(All,
                         DictationSessionUiImplBrowserTest,
                         testing::Bool());

}  // namespace dictation
