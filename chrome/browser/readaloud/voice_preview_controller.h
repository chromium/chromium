// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_VOICE_PREVIEW_CONTROLLER_H_
#define CHROME_BROWSER_READALOUD_VOICE_PREVIEW_CONTROLLER_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/readaloud/read_aloud_audio_broker.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace media {
class AudioParameters;
}  // namespace media

namespace readaloud {

class SpeechSynthesisBroker;

// Manages the lifecycle, utility process connection, audio output stream, and
// state transitions for Read Aloud voice preview playback in settings.
//
// Isolates voice preview state and IPC callbacks from `ReadAloudService` so
// previewing a voice does not interfere with active or paused article playback.
//
// Threading & Lifetime:
// Bound to the Browser UI thread; all public methods and Mojo client callbacks
// must be invoked on the creation sequence. `profile` and
// `speech_synthesis_broker` must outlive `VoicePreviewController`.
class VoicePreviewController
    : public read_aloud::mojom::ReadAloudPlaybackControllerClient {
 public:
  // Invoked when the voice preview playback state changes. `voice_id`
  // identifies the previewed voice, or is empty (`""`) when playback is
  // explicitly cancelled via `StopPreview()`.
  using StateChangedCallback =
      base::RepeatingCallback<void(std::string_view voice_id,
                                   read_aloud::mojom::PlaybackState state)>;

  using PlaybackControllerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>,
      mojo::PendingRemote<
          read_aloud::mojom::ReadAloudPlaybackControllerClient>)>;

  VoicePreviewController(
      Profile* profile,
      SpeechSynthesisBroker* speech_synthesis_broker,
      StateChangedCallback state_changed_callback,
      PlaybackControllerBinder controller_binder = {},
      ReadAloudAudioBroker::AudioStreamFactoryBinder factory_binder = {});

  VoicePreviewController(const VoicePreviewController&) = delete;
  VoicePreviewController& operator=(const VoicePreviewController&) = delete;

  ~VoicePreviewController() override;

  // Starts synthesizing and playing `preview_text` for `voice_id`, or resumes
  // playback if `voice_id` is already the active preview. Cancels any
  // previously active preview session for a different voice.
  void PlayPreview(std::string_view voice_id, std::u16string_view preview_text);

  // Pauses the active voice preview playback without tearing down the session.
  void PausePreview();

  // Stops any active voice preview playback and notifies the callback with
  // an empty voice_id and `PlaybackState::kStopped`.
  void StopPreview();

  // Tears down the utility connection and audio broker without emitting a
  // state change notification.
  void Reset();

  bool IsPreviewActive() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !voice_id_.empty();
  }

  // read_aloud::mojom::ReadAloudPlaybackControllerClient:
  void OnPlaybackStateChanged(read_aloud::mojom::PlaybackState state) override;
  void OnPlaybackProgressChanged(base::TimeDelta elapsed,
                                 base::TimeDelta duration) override {}
  void OnWordBoundaryReached(uint32_t start_character_offset,
                             uint32_t end_character_offset) override {}
  void OnTextChunked(const std::vector<std::u16string>& chunks) override {}
  void RequestSpeechSynthesis(const std::u16string& text_chunk,
                              read_aloud::mojom::Speaker speaker,
                              uint64_t sequence_id,
                              RequestSpeechSynthesisCallback callback) override;

 private:
  void ResetActivePreviewState();
  void ResetControllerAndAudio();
  void BindNewPlaybackController();
  void InitializeAudioStream();
  void OnAudioStreamCreated(
      const media::AudioParameters& params,
      mojo::PendingRemote<media::mojom::AudioOutputStream> stream_remote,
      media::mojom::ReadWriteAudioDataPipePtr data_pipe);
  void OnUtilityDisconnect();
  void FinishWithError();
  void OnSpeechSynthesized(RequestSpeechSynthesisCallback callback,
                           mojo_base::BigBuffer response_bytes,
                           bool success);

  const raw_ptr<Profile> profile_;
  const raw_ptr<SpeechSynthesisBroker> speech_synthesis_broker_;
  const StateChangedCallback state_changed_callback_;
  const PlaybackControllerBinder controller_binder_;
  const std::unique_ptr<ReadAloudAudioBroker> audio_broker_;

  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerFactory>
      player_factory_;
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackController> utility_player_;
  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      utility_observer_receiver_{this};

  // Non-empty while a voice preview is buffering or playing.
  std::string voice_id_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Invalidated whenever the active preview changes or stops.
  base::WeakPtrFactory<VoicePreviewController> preview_weak_factory_{this};
  // Invalidated whenever controller/audio resources (`utility_player_` /
  // `audio_broker_`) are torn down.
  base::WeakPtrFactory<VoicePreviewController> weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_VOICE_PREVIEW_CONTROLLER_H_
