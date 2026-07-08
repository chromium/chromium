// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_interactive_browser_test_base.h"

#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/session_controller.h"
#include "chrome/browser/dictation/session_ui_impl.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"

namespace dictation {

DictationInteractiveBrowserTestBase::DictationInteractiveBrowserTestBase() =
    default;

DictationInteractiveBrowserTestBase::~DictationInteractiveBrowserTestBase() =
    default;

void DictationInteractiveBrowserTestBase::TearDownOnMainThread() {
  dictation_service().EndSession();
  InteractiveBrowserTestMixin<DictationBrowserTestBase>::TearDownOnMainThread();
}

SessionUiImpl* DictationInteractiveBrowserTestBase::session_ui() {
  if (!dictation_service().session_controller()) {
    return nullptr;
  }
  return static_cast<SessionUiImpl*>(
      dictation_service().session_controller()->ui_for_testing());
}

content::WebContents* DictationInteractiveBrowserTestBase::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

DictationInteractiveBrowserTestBase::StepBuilder
DictationInteractiveBrowserTestBase::CheckHasSession(
    bool expected_has_session) {
  return CheckResult(
      [this]() { return dictation_service().session_controller() != nullptr; },
      expected_has_session);
}

DictationInteractiveBrowserTestBase::MultiStep
DictationInteractiveBrowserTestBase::StartSession() {
  return Steps(Do([this] {
    dictation_service().StartSession(*browser(),
                                     DefaultInPageTargetId(web_contents()));
    if (dictation_service().session_controller()) {
      last_started_provider_ =
          static_cast<ListenerStreamProvider*>(dictation_service()
                                                   .session_controller()
                                                   ->attached_stream_provider())
              ->GetWeakPtr();

      // Register callback to update last_started_provider_ on subsequent
      // starts.
      session_state_subscription_ =
          dictation_service()
              .session_controller()
              ->AddSessionStateChangedCallback(base::BindRepeating(
                  &DictationInteractiveBrowserTestBase::OnSessionStateChanged,
                  base::Unretained(this)));

      // A stream may not always be created (e.g. onboarding needs to be shown).
      if (last_started_provider_) {
        ExtensionWaitForStreamStart(
            profile(), last_started_provider_->stream_id_for_testing());
      }
    }
  }));
}

DictationInteractiveBrowserTestBase::MultiStep
DictationInteractiveBrowserTestBase::ExtensionAPISetStreamState(
    ExtensionStreamState state) {
  return Steps(Do([this, state] {
    ASSERT_NE(last_started_provider_, nullptr);
    ExtensionSendStreamStateUpdate(
        profile(), last_started_provider_->stream_id_for_testing(), state);
  }));
}

DictationInteractiveBrowserTestBase::MultiStep
DictationInteractiveBrowserTestBase::ExtensionAPIUpdateTranscription(
    ExtensionTranscriptionType type,
    std::string_view text) {
  return Steps(Do([this, type, text_str = std::string(text)] {
    ASSERT_NE(last_started_provider_, nullptr);
    ExtensionSendTranscriptUpdate(
        profile(), last_started_provider_->stream_id_for_testing(), type,
        text_str);
  }));
}

base::RepeatingCallback<SessionState()>
DictationInteractiveBrowserTestBase::GetSessionState() {
  return base::BindRepeating(
      [](DictationInteractiveBrowserTestBase* test) {
        return test->dictation_service().session_controller()->GetState();
      },
      base::Unretained(this));
}

base::RepeatingCallback<bool()>
DictationInteractiveBrowserTestBase::HasAttachedStreamProvider() {
  return base::BindRepeating(
      [](DictationInteractiveBrowserTestBase* test) {
        return test->dictation_service()
                   .session_controller()
                   ->attached_stream_provider() != nullptr;
      },
      base::Unretained(this));
}

void DictationInteractiveBrowserTestBase::OnSessionStateChanged(
    SessionState state) {
  if (state == SessionState::kStreamInitializing) {
    auto* controller = dictation_service().session_controller();
    if (controller && controller->attached_stream_provider()) {
      last_started_provider_ = static_cast<ListenerStreamProvider*>(
                                   controller->attached_stream_provider())
                                   ->GetWeakPtr();
    }
  }
}

}  // namespace dictation
