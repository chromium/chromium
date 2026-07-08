// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_DICTATION_INTERACTIVE_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_DICTATION_DICTATION_INTERACTIVE_BROWSER_TEST_BASE_H_

#include <string_view>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/dictation/dictation_browser_test_base.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "chrome/test/interaction/interactive_browser_test.h"

namespace content {
class WebContents;
}

namespace dictation {

class ListenerStreamProvider;
class SessionUiImpl;

class DictationInteractiveBrowserTestBase
    : public InteractiveBrowserTestMixin<DictationBrowserTestBase> {
 public:
  using ExtensionStreamState = extensions::api::dictation_private::StreamState;
  using ExtensionTranscriptionType =
      extensions::api::dictation_private::TranscriptionType;

  DictationInteractiveBrowserTestBase();
  ~DictationInteractiveBrowserTestBase() override;

  // InteractiveBrowserTestMixin:
  void TearDownOnMainThread() override;

  SessionUiImpl* session_ui();

  content::WebContents* web_contents();

  StepBuilder CheckHasSession(bool expected_has_session);

  // Starts a dictation session. If a stream is created this will also block
  // until the StreamStart event has been received in the extension.
  MultiStep StartSession();

  MultiStep ExtensionAPISetStreamState(ExtensionStreamState state);
  MultiStep ExtensionAPIUpdateTranscription(ExtensionTranscriptionType type,
                                            std::string_view text);

  base::RepeatingCallback<SessionState()> GetSessionState();
  base::RepeatingCallback<bool()> HasAttachedStreamProvider();

 protected:
  base::WeakPtr<ListenerStreamProvider> last_started_provider_;

 private:
  void OnSessionStateChanged(SessionState state);

  base::CallbackListSubscription session_state_subscription_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_DICTATION_INTERACTIVE_BROWSER_TEST_BASE_H_
