// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_H_
#define CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/dictation/metrics.h"
#include "chrome/browser/dictation/session_state.h"
#include "chrome/browser/dictation/session_ui_delegate.h"
#include "chrome/browser/dictation/stream_provider_delegate.h"
#include "chrome/browser/dictation/target.h"
#include "content/public/browser/global_dom_node_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace dictation {

enum class DictationStreamEndTrigger;
class SessionControllerDelegate;
class SessionUi;
class StreamProvider;

// The session_controller is a coordinating class between the StreamProvider,
// Target, and UI. It manages Profile-level state and transitions and
// synchronizes the dictation system.
class SessionController : public SessionUiDelegate,
                          public StreamProviderDelegate,
                          public Target::Delegate {
 public:
  explicit SessionController(SessionControllerDelegate& delegate);
  ~SessionController() override;
  SessionController(const SessionController&) = delete;
  SessionController& operator=(const SessionController&) = delete;

  // Called by the service to prime the session for a new UI.
  void ResetUi();

  // SessionUiDelegate:
  void UiRequestEndSession() override;
  void FinalizeAndShutdown(DictationStreamEndTrigger trigger) override;
  SessionState GetState() const override;
  base::CallbackListSubscription AddSessionStateChangedCallback(
      SessionStateChangedCallback callback) override;
  void HostTabDidClose() override;

  // StreamProviderDelegate:
  void DidUpdateStreamProviderState(
      StreamProvider& stream_provider,
      StreamProvider::StreamState old_state,
      StreamErrorReason reason = StreamErrorReason::kNone) override;

  // Target::Delegate:
  void OnTargetRequestEndStream(DictationStreamEndTrigger trigger) override;
  void OnTargetRequestEndSession(DictationStreamEndTrigger trigger) override;
  void OnTargetInvalidated() override;

  // Starts a new dictation stream by creating and attaching a new stream
  // provider bound to `target`. An existing stream must have been detached
  // before calling this method.
  void StartDictationStream(std::unique_ptr<Target> target,
                            DictationStreamStartTrigger trigger,
                            base::TimeTicks trigger_time);

  // Ends the current dictation stream and detaches the stream provider.
  void EndDictationStream(DictationStreamEndTrigger trigger);

  // Updates the audio level in the UI.
  void UpdateAudioLevel(float audio_level);

  // Stops any stream, moves to `SessionState::kEnded`, then asks the delegate
  // to destroy the session, and so `this`. Every session ends through here.
  void EndSessionSynchronously();

  StreamProvider* attached_stream_provider() const {
    return attached_stream_ ? attached_stream_->provider.get() : nullptr;
  }

  // The DOM node ID the latest stream was started on, or nullopt for non-DOM
  // targets or before any stream has started.
  const std::optional<content::GlobalDOMNodeId>& last_used_target_id() const {
    return last_used_target_id_;
  }

  SessionUi* ui_for_testing() { return ui_.get(); }
  Target* current_target_for_testing() { return current_target_.get(); }

  bool is_auto_session_end_timer_running_for_testing() const {
    return auto_session_end_timer_.IsRunning();
  }

  void FlushWakeLockForTesting();

 private:
  struct StreamEntry {
    StreamEntry(std::unique_ptr<Target> target,
                std::unique_ptr<StreamProvider> provider);
    ~StreamEntry();

    // Declared before `provider` so `provider` is destroyed first (releasing
    // any non-owning reference to `target` before `target` is destroyed).
    std::unique_ptr<Target> target;
    std::unique_ptr<StreamProvider> provider;
  };

  void ClearCurrentTarget();
  void MoveToState(SessionState new_state);
  void UpdateWakeLock();
  device::mojom::WakeLock* GetWakeLock();
  void EndSessionAsynchronously();
  void PurgeToDeleteStreamProviders();
  content::BrowserContext* GetBrowserContext() const;

  const base::raw_ref<SessionControllerDelegate> delegate_;

  SessionState state_ = SessionState::kInactive;
  bool is_shutting_down_ = false;

  // Target retained after its stream finishes so it can continue observing
  // target-surface lifecycle/input events (e.g. a second Escape press or
  // navigation) while the session is `kInactive`.
  std::unique_ptr<Target> idle_target_;

  // Stream entries that are finalizing.
  absl::flat_hash_set<std::unique_ptr<StreamEntry>> finalizing_streams_;

  // The currently attached stream entry. The state of its provider is used to
  // drive the current state of dictation in the UI.
  std::unique_ptr<StreamEntry> attached_stream_;

  // Non-owning pointer to the target currently using `this` as its
  // `Target::Delegate`. Owned by `attached_stream_`, `finalizing_streams_`, or
  // `idle_target_`. Declared after those owners so it is destroyed first, and
  // explicitly cleared in `~SessionController()`.
  raw_ptr<Target> current_target_ = nullptr;

  // Stream providers that are queued to be deleted asynchronously after
  // `DetachTarget()` has been called.
  absl::flat_hash_set<std::unique_ptr<StreamProvider>>
      to_delete_stream_providers_;

  std::unique_ptr<SessionUi> ui_;

  base::RepeatingCallbackList<void(SessionState)>
      session_state_changed_callback_list_;

  std::optional<content::GlobalDOMNodeId> last_used_target_id_;

  // When the user action that started the current stream happened. Used to
  // record VoiceTyping.StreamStartLatency and
  // VoiceTyping.StreamAbandonedBeforeListening.
  base::TimeTicks stream_trigger_time_;

  // Timer for delayed session shutdown after a stream ends.
  base::OneShotTimer auto_session_end_timer_;

  // Display wake lock held whenever `state_` is not `SessionState::kInactive`
  // so the screen does not go to sleep during long dictations.
  // TODO(https://crbug.com/581952682): Consider still allowing the screen to
  // dim during periods of silence on platforms where that kind of wake lock is
  // supported.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
  bool is_wake_lock_active_ = false;

  base::WeakPtrFactory<SessionController> weak_ptr_factory_{this};
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_SESSION_CONTROLLER_H_
