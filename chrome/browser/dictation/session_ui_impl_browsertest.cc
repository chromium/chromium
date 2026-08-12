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
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
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
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/button/label_button.h"
#include "url/gurl.h"

namespace dictation {

class DictationSessionUiImplBrowserTest
    : public DictationInteractiveBrowserTestBase {
 public:
  DictationSessionUiImplBrowserTest() = default;
  ~DictationSessionUiImplBrowserTest() override = default;

 protected:
  auto CloseTab(int index) {
    return Do([this, index]() {
      browser()->tab_strip_model()->CloseWebContentsAt(
          index, TabCloseTypes::CLOSE_USER_GESTURE);
    });
  }

  auto MoveTabToWindow(Browser* source, Browser* target, int index) {
    return Do([source, target, index]() {
      chrome::MoveTabsToExistingWindow(source, target, {index});
    });
  }

  auto GetSessionState() {
    return [this]() {
      return dictation_service().session_controller()->GetState();
    };
  }

  auto HasAttachedStreamProvider() {
    return [this]() {
      return dictation_service()
                 .session_controller()
                 ->attached_stream_provider() != nullptr;
    };
  }

  auto CheckShowingDictationErrorToast(bool showing) {
    return Check([this, showing]() {
      ToastController* const toast_controller =
          browser()->GetFeatures().toast_controller();
      CHECK(toast_controller);
      const bool is_showing_dictation_error_toast =
          toast_controller->IsShowingToast() &&
          toast_controller->GetCurrentToastId() == ToastId::kDictationError;
      return is_showing_dictation_error_toast == showing;
    });
  }

  auto CheckShowingDictationStoppedToast(bool showing) {
    return Check([this, showing]() {
      ToastController* const toast_controller =
          browser()->GetFeatures().toast_controller();
      CHECK(toast_controller);
      const bool is_showing_dictation_stopped_toast =
          toast_controller->IsShowingToast() &&
          toast_controller->GetCurrentToastId() == ToastId::kDictationStopped;
      return is_showing_dictation_stopped_toast == showing;
    });
  }

  auto StartDictationStream(DictationStreamStartTrigger trigger) {
    return Do([this, trigger]() {
      dictation_service().session_controller()->StartDictationStream(
          DefaultInPageTarget(web_contents()), trigger);
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       SessionStateUpdatesToggleButton) {
  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),
    WaitForShow(DictationBubbleUi::kWaveformElementIdForTesting),

    // kStreamInitializing.
    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true),

    // kTranscribing.
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    CheckResult(GetSessionState(), SessionState::kTranscribing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true),

    // kFinalizing.
    Do([this]{
      dictation_service().session_controller()->EndDictationStream();
    }),
    CheckResult(GetSessionState(), SessionState::kFinalizing),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Done"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, false),

    // kInactive
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete),
    CheckResult(GetSessionState(), SessionState::kInactive),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::LabelButton::GetText, u"Start"),
    CheckViewProperty(DictationBubbleUi::kToggleButtonElementIdForTesting,
                      &views::View::GetEnabled, true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest, UpdateAudioLevel) {
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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       ToggleStartStopFromUi) {
  // clang-format off
  RunTestSequence(
    // Open the session ui.
    StartSession(),
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
    CheckResult(GetSessionState(), SessionState::kInactive),

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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest, TabSwitchHidesUI) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);

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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest, CloseTabEndsSession) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);

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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       UiFollowsDetachedTab) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Create a second browser window.
  Browser* second_browser = CreateBrowser(browser()->GetProfile());

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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       BackgroundTabActivationEndsSession) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);

  // clang-format off
  RunTestSequence(
    StartSession(),
    WaitForShow(DictationBubbleUi::kViewElementIdForTesting),

    // Switch to the second tab. The session should be ended but only after
    // finalization.
    SelectTab(kTabStripElementId, 1),
    WaitForHide(DictationBubbleUi::kViewElementIdForTesting),
    CheckHasSession(true),
    CheckResult(GetSessionState(), SessionState::kFinalizing),

    // Once the finalization completes the sesison should be ended.
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete),
    CheckHasSession(false),

    // Switch back to the first tab and ensure the UI does not reappear.
    SelectTab(kTabStripElementId, 0),
    EnsureNotPresent(DictationBubbleUi::kViewElementIdForTesting)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       TabSwitchShowsDictationStoppedToast) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);

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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       SwitchBackToDictatingTabDuringFinalization) {
  // Add a second tab with the first tab in the foreground.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  browser()->tab_strip_model()->ActivateTabAt(0);

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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest, ShowsToastOnError) {
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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
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
      dictation_service().session_controller()->EndDictationStream();
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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       SecondWindowInvokesDictationMovesUI) {
  // Create a second browser window.
  Browser* second_browser = CreateBrowser(browser()->GetProfile());
  content::WebContents* window2_contents =
      second_browser->tab_strip_model()->GetActiveWebContents();
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

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
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
    WithElement(
        kWebContentsElementId,
        [&target_bounds](ui::TrackedElement* el) {
          target_bounds = AsInstrumentedWebContents(el)
                              ->GetElementBoundsInScreen("#text_id");
        }),
    InAnyContext(CheckElement(
        DictationOverlayView::kViewElementIdForTesting,
        [&target_bounds](ui::TrackedElement* el) {
          const views::View* const overlay_view = AsView(el);
          const gfx::Rect overlay_bounds = overlay_view->GetBoundsInScreen();
          return target_bounds.Contains(overlay_bounds.origin());
        }))
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       OverlayButtonUpdatesOnStreamStateChange) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    InAnyContext(WaitForShow(DictationOverlayView::kViewElementIdForTesting)),

    // Initial state (kStreamInitializing): Mic icon button present, others absent.
    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    InAnyContext(EnsurePresent(
        DictationOverlayView::kMicButtonElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kWaveformElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kFinalizingImageElementIdForTesting)),

    // Transition to kTranscribing: WaveformView shown, others absent.
    ExtensionAPISetStreamState(ExtensionStreamState::kTranscribing),
    CheckResult(GetSessionState(), SessionState::kTranscribing),
    InAnyContext(WaitForShow(
        DictationOverlayView::kWaveformElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kMicButtonElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kFinalizingImageElementIdForTesting)),

    // Transition to kFinalizing: 3-dot finalizing image shown, others absent.
    Do([this] {
      dictation_service().session_controller()->EndDictationStream();
    }),
    CheckResult(GetSessionState(), SessionState::kFinalizing),
    InAnyContext(WaitForShow(
        DictationOverlayView::kFinalizingImageElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kMicButtonElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kWaveformElementIdForTesting)),

    // Transition to kInactive: Mic icon button shown again, others absent.
    ExtensionAPISetStreamState(ExtensionStreamState::kComplete),
    CheckResult(GetSessionState(), SessionState::kInactive),
    InAnyContext(WaitForShow(
        DictationOverlayView::kMicButtonElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kWaveformElementIdForTesting)),
    InAnyContext(EnsureNotPresent(
        DictationOverlayView::kFinalizingImageElementIdForTesting))
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
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
        &WaveformViewButton::audio_level_for_testing, 0.5f))
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplBrowserTest,
                       OverlayButtonsToggleStreamState) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContentsElementId);
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");

  // clang-format off
  RunTestSequence(
    InstrumentTab(kWebContentsElementId),
    NavigateWebContents(kWebContentsElementId, url),
    StartSessionWithTarget(kWebContentsElementId, "#text_id"),
    InAnyContext(WaitForShow(DictationOverlayView::kViewElementIdForTesting)),

    CheckResult(GetSessionState(), SessionState::kStreamInitializing),
    InAnyContext(WaitForShow(
        DictationOverlayView::kMicButtonElementIdForTesting)),

    // Pressing the mic button while initializing ends the stream.
    InAnyContext(PressButton(
        DictationOverlayView::kMicButtonElementIdForTesting)),
    CheckResult(GetSessionState(), SessionState::kFinalizing),

    ExtensionAPISetStreamState(ExtensionStreamState::kComplete),
    CheckResult(GetSessionState(), SessionState::kInactive),

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

}  // namespace dictation
