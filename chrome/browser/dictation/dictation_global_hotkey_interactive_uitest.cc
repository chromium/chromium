// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the OS-wide "stop dictation" hotkey. These live in
// interactive_ui_tests because the handler's behavior depends on which browser
// window is active, which requires real window activation. Registration
// lifecycle is covered by unit tests.

#include "base/check.h"
#include "chrome/browser/dictation/dictation_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/hotkey_manager.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"

namespace dictation {

namespace {

using ExtensionStreamState = extensions::api::dictation_private::StreamState;
using StreamId = DictationMultiplexer::StreamId;

class DictationGlobalHotkeyInteractiveTest : public DictationBrowserTestBase {
 public:
  void SetUpOnMainThread() override {
    DictationBrowserTestBase::SetUpOnMainThread();
    // The global hotkey is owned by the hotkey manager.
    ASSERT_NE(dictation_service().hotkey_manager_for_testing(), nullptr);
  }

  ui::Accelerator hotkey() { return GetDictationHotkeyFromPrefs(profile()); }

  // Starts a session in the active tab of `browser()` and waits until the
  // stream is transcribing. Returns the stream ID.
  StreamId StartTranscribing() {
    StartSession();
    ListenerStreamProvider* provider = attached_stream();
    CHECK(provider);
    const StreamId stream_id = provider->stream_id_for_testing();
    ExtensionWaitForStreamStart(profile(), stream_id);
    ExtensionSendStreamStateUpdate(profile(), stream_id,
                                   ExtensionStreamState::kTranscribing);
    WaitForSessionState(SessionState::kTranscribing);
    return stream_id;
  }

  // Presses the global hotkey and waits for `stream_id` to end.
  void PressGlobalHotkeyAndWaitForStreamEnd(StreamId stream_id) {
    CHECK(fake_global_listener().IsListeningFor(hotkey()));
    fake_global_listener().Press(hotkey());
    ExtensionWaitForStreamEnd(profile(), stream_id);
  }
};

// When the window hosting dictation is focused, the OS consumes the keystroke
// so the local accelerator never fires. The global handler must preserve the
// in-Chrome toggle behavior, i.e. stop the stream.
IN_PROC_BROWSER_TEST_F(DictationGlobalHotkeyInteractiveTest,
                       PressWhileDictationWindowActiveStopsStream) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  const StreamId stream_id = StartTranscribing();

  PressGlobalHotkeyAndWaitForStreamEnd(stream_id);

  EXPECT_FALSE(session_controller() &&
               session_controller()->attached_stream_provider());
  EXPECT_FALSE(fake_global_listener().IsListeningFor(hotkey()));
}

// A press while another window of the same profile is focused, with nothing
// editable focused there, stops dictation in the original window.
IN_PROC_BROWSER_TEST_F(DictationGlobalHotkeyInteractiveTest,
                       PressWhileOtherWindowActiveStopsStream) {
  const StreamId stream_id = StartTranscribing();

  BrowserWindowInterface* other_browser = CreateBrowser(profile());
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(other_browser));

  PressGlobalHotkeyAndWaitForStreamEnd(stream_id);

  EXPECT_FALSE(session_controller() &&
               session_controller()->attached_stream_provider());
  EXPECT_FALSE(fake_global_listener().IsListeningFor(hotkey()));
}

// A press while a window of a different profile is focused stops dictation in
// the original profile. Uses an incognito window, since creating additional
// regular profiles isn't supported on all platforms (e.g. ChromeOS).
IN_PROC_BROWSER_TEST_F(DictationGlobalHotkeyInteractiveTest,
                       PressWhileOtherProfileWindowActiveStopsStream) {
  const StreamId stream_id = StartTranscribing();

  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(incognito_browser));

  PressGlobalHotkeyAndWaitForStreamEnd(stream_id);

  EXPECT_FALSE(session_controller() &&
               session_controller()->attached_stream_provider());
  EXPECT_FALSE(fake_global_listener().IsListeningFor(hotkey()));
}

}  // namespace

}  // namespace dictation
