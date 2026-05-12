// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/dictation/session_ui_delegate.h"

namespace dictation {

class SessionControllerDelegate;
class SessionUi;
class StreamProvider;
class Target;

// The session_controller is a coordinating class between the StreamProvider and
// the UI. It manages Profile-level state and transitions and synchronizes the
// dictation system.
class SessionController : public SessionUiDelegate {
 public:
  enum class State {
    // Dictation is currently not active, there is no stream provider attached.
    kInactive,

    // A stream provider has just been attached but it is still starting up and
    // not yet active.
    kStreamInitializing,

    // A stream provider is attached and actively transcribing and sending
    // data.
    kTranscribing,

    // A stream provider is attached and has finished transcribing but is still
    // finalizing the transcription and more data may be provided.
    kFinalizing,
  };

  explicit SessionController(SessionControllerDelegate& delegate);
  ~SessionController() override;
  SessionController(const SessionController&) = delete;
  SessionController& operator=(const SessionController&) = delete;

  // Called by the service when it's ready for the session to start.
  void Initialize();

  // SessionUiDelegate
  void RequestEndSession() override;

  // Starts a new dictation stream by creating and attaching a new stream
  // provider. An existing stream must have been detached before calling this
  // method.
  void StartDictationStream(Target& target);

  // Ends the current dictation stream and detaches the stream provider.
  void EndDictationStream();

  State state() const { return state_; }
  StreamProvider* attached_stream_provider() const {
    return attached_stream_provider_.get();
  }

  SessionUi* ui_for_testing() { return ui_.get(); }

 private:
  void MoveToState(State new_state);

  const base::raw_ref<SessionControllerDelegate> delegate_;

  State state_ = State::kInactive;

  // The currently attached stream provider. The state of this provider is used
  // to drive the current state of dictation in the UI.
  std::unique_ptr<StreamProvider> attached_stream_provider_;

  std::unique_ptr<SessionUi> ui_;

  base::WeakPtrFactory<SessionController> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_H_
