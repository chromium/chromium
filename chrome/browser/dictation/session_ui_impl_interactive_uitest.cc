// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/dictation/dictation_interactive_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/dictation/dictation_bubble_ui.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

namespace dictation {

class DictationSessionUiImplInteractiveUiTest
    : public DictationInteractiveBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplInteractiveUiTest,
                       SecondWindowInvokesDictationHotkeyMovesUI) {
  // Create a second browser window.
  Browser* second_browser = CreateBrowser(browser()->GetProfile());
  content::WebContents* window2_contents =
      second_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(window2_contents, nullptr);

  // Navigate Window 2 to test page and wait for paint.
  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(content::NavigateToURL(window2_contents, url));
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(window2_contents);
  content::MainThreadFrameObserver frame_observer(
      window2_contents->GetPrimaryMainFrame()->GetRenderWidgetHost());
  frame_observer.Wait();

  // clang-format off
  RunTestSequence(
    // Start dictation session in Window 1.
    // Explicitly use Window 1 context for starting session and waiting for UI.
    InContext(BrowserElements::From(browser())->GetContext(),
              StartSession()),
    InContext(BrowserElements::From(browser())->GetContext(),
              WaitForShow(DictationBubbleUi::kViewElementIdForTesting)),

    // Activate Window 2.
    InContext(BrowserElements::From(second_browser)->GetContext(),
              ActivateSurface(kBrowserViewElementId)),

    // Invoke dictation from the hotkey in Window 2.
    // Explicitly use Window 2 context.
    InContext(BrowserElements::From(second_browser)->GetContext(),
              Do([this, window2_contents] {
                ASSERT_TRUE(content::ExecJs(
                    window2_contents,
                    "document.getElementById('text_id').focus();"));
                // Wait for focus to propagate to the editable element.
                EXPECT_TRUE(base::test::RunUntil([&]() {
                  auto* frame = window2_contents->GetFocusedFrame();
                  return frame && !frame->GetFocusedDOMNodeId().is_null();
                }));

                dictation_service().ToggleHotkeyHandler();
              })),

    // Verify UI is showing in Window 2 and not showing in Window 1.
    InContext(BrowserElements::From(second_browser)->GetContext(),
              WaitForShow(DictationBubbleUi::kViewElementIdForTesting)),
    InContext(BrowserElements::From(browser())->GetContext(),
              EnsureNotPresent(DictationBubbleUi::kViewElementIdForTesting)),
    CheckHasSession(true)
  );
  // clang-format on
}

IN_PROC_BROWSER_TEST_F(DictationSessionUiImplInteractiveUiTest,
                       HotkeyStartEndSession) {
  // Ensure onboarding is completed and hotkey is configured.
  profile()->GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                    true);
  profile()->GetPrefs()->SetString(prefs::kVoiceTypingHotkey, "Alt+D");
  DictationKeyedService::Get(profile())->DidInstallConnector();
  ui::Accelerator hotkey(ui::VKEY_D, ui::EF_ALT_DOWN);

  const GURL url =
      embedded_test_server()->GetURL("/textinput/simple_textarea.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents());
  content::MainThreadFrameObserver frame_observer(
      web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
  frame_observer.Wait();

  // clang-format off
  RunTestSequence(
    ActivateSurface(kBrowserViewElementId),
    Do([this] {
      ASSERT_TRUE(content::ExecJs(
          web_contents(),
          "document.getElementById('text_id').focus();"));
      EXPECT_TRUE(base::test::RunUntil([&]() {
        auto* frame = web_contents()->GetFocusedFrame();
        return frame && !frame->GetFocusedDOMNodeId().is_null();
      }));

      auto* focused_frame = web_contents()->GetFocusedFrame();
      ASSERT_NE(focused_frame, nullptr);
      EXPECT_EQ(focused_frame->GetFocusedEditableLevel(),
                content::EditableLevel::kPlaintextEditable);

      auto* service = DictationKeyedService::Get(profile());
      ASSERT_NE(service, nullptr);
      ASSERT_TRUE(service->IsEnabledAndReady());
      ASSERT_EQ(service->session_controller(), nullptr);

      // Verify the browser under test. GetLastActiveBrowser is preferred over
      // GetActiveBrowser here to avoid test flakiness due to async focus
      // propagation.
      auto* active_browser =
          GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
      ASSERT_NE(active_browser, nullptr);
      EXPECT_EQ(active_browser, browser());
    }),

    // Send actual hotkey press to start session
    SendAccelerator(kBrowserViewElementId, hotkey),

    Do([this] {
      auto* service = DictationKeyedService::Get(profile());
      ASSERT_NE(service->session_controller(), nullptr);
      auto* stream_provider =
          service->session_controller()->attached_stream_provider();
      ASSERT_NE(stream_provider, nullptr);
      auto* target = stream_provider->GetTarget();
      ASSERT_NE(target, nullptr);

      std::optional<int> expected_node_id = content::GetDOMNodeId(
          *web_contents()->GetPrimaryMainFrame(), "#text_id");
      ASSERT_TRUE(expected_node_id.has_value());
      EXPECT_EQ(target->global_dom_node_id().target_element_dom_id,
                blink::DOMNodeIdType(*expected_node_id));
    }),

    // Send actual hotkey press to end session
    SendAccelerator(kBrowserViewElementId, hotkey),

    Do([this] {
      auto* service = DictationKeyedService::Get(profile());
      EXPECT_NE(service->session_controller(), nullptr);
      EXPECT_EQ(service->session_controller()->attached_stream_provider(),
                nullptr);
    })
  );
  // clang-format on
}

}  // namespace dictation
