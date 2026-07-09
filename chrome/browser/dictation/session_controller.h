// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_H_

#include <iosfwd>
#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/session_ui_delegate.h"
#include "chrome/browser/dictation/stream_provider_delegate.h"
#include "chrome/browser/dictation/target.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace dictation {

class SessionControllerDelegate;
class SessionUi;
class StreamProvider;

// The session_controller is a coordinating class between the StreamProvider and
// the UI. It manages Profile-level state and transitions and synchronizes the
// dictation system.
class SessionController : public SessionUiDelegate,
                          public StreamProviderDelegate {
 public:
  explicit SessionController(SessionControllerDelegate& delegate);
  ~SessionController() override;
  SessionController(const SessionController&) = delete;
  SessionController& operator=(const SessionController&) = delete;

  // Called by the service when it's ready for the session to start.
  void Initialize();

  // SessionUiDelegate:
  void UiRequestEndSession() override;
  void UiRequestEndActiveStream() override;
  void FinalizeAndShutdown() override;
  void UiRequestStartStream() override;
  SessionState GetState() const override;
  base::CallbackListSubscription AddSessionStateChangedCallback(
      SessionStateChangedCallback callback) override;
  void HostTabDidClose() override;

  // StreamProviderDelegate:
  void DidUpdateStreamProviderState(
      StreamProvider& stream_provider,
      StreamProvider::StreamState old_state) override;

  // Starts a new dictation stream by creating and attaching a new stream
  // provider. An existing stream must have been detached before calling this
  // method.
  void StartDictationStream(const TargetId& target_id);

  // Ends the current dictation stream and detaches the stream provider.
  void EndDictationStream();

  StreamProvider* attached_stream_provider() const {
    return attached_stream_provider_.get();
  }

  SessionUi* ui_for_testing() { return ui_.get(); }

 private:
  void MoveToState(SessionState new_state);
  void EndSessionAsynchronously();
  void PurgeToDeleteStreamProviders();

  const base::raw_ref<SessionControllerDelegate> delegate_;

  SessionState state_ = SessionState::kInactive;
  bool is_shutting_down_ = false;

  // The currently attached stream provider. The state of this provider is used
  // to drive the current state of dictation in the UI.
  std::unique_ptr<StreamProvider> attached_stream_provider_;

  // Stream providers that are finalizing.
  absl::flat_hash_set<std::unique_ptr<StreamProvider>>
      finalizing_stream_providers_;

  // Stream providers that are queued to be deleted asynchronously.
  absl::flat_hash_set<std::unique_ptr<StreamProvider>>
      to_delete_stream_providers_;

  std::unique_ptr<SessionUi> ui_;

  base::RepeatingCallbackList<void(SessionState)>
      session_state_changed_callback_list_;

  std::optional<TargetId> last_used_target_id_;

  base::WeakPtrFactory<SessionController> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_H_
