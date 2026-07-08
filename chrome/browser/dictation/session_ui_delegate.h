// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_UI_DELEGATE_H_
#define CHROME_BROWSER_DICTATION_SESSION_UI_DELEGATE_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/dictation/session_state.h"

namespace dictation {

// Interface for the UI to communicate back to the session controller.
class SessionUiDelegate {
 public:
  virtual ~SessionUiDelegate() = default;

  // Requests that the session be ended. The UI may be deleted as part of this
  // call but this will happen asynchronously.
  virtual void UiRequestEndSession() = 0;

  // Requests that the active stream be stopped. Must only be called if there
  // is an attached initializing or transcribing.
  virtual void UiRequestEndActiveStream() = 0;

  // Called to start a new stream from the UI. Must only be called if the
  // session is inactive. Starts a new stream on the last used Target.
  virtual void UiRequestStartStream() = 0;

  // Returns the current state of the dictation session.
  virtual SessionState GetState() const = 0;

  using SessionStateChangedCallback =
      base::RepeatingCallback<void(SessionState)>;
  // Registers a callback to be notified of session state changes.
  virtual base::CallbackListSubscription AddSessionStateChangedCallback(
      SessionStateChangedCallback callback) = 0;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_UI_DELEGATE_H_
