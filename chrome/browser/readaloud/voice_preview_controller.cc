// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/voice_preview_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/unguessable_token.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/readaloud/audio_generation/speech_synthesis_broker.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "content/public/browser/service_process_host.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace readaloud {

VoicePreviewController::VoicePreviewController(
    Profile* profile,
    SpeechSynthesisBroker* speech_synthesis_broker,
    StateChangedCallback state_changed_callback,
    PlaybackControllerBinder controller_binder,
    ReadAloudAudioBroker::AudioStreamFactoryBinder factory_binder)
    : profile_(profile),
      speech_synthesis_broker_(speech_synthesis_broker),
      state_changed_callback_(std::move(state_changed_callback)),
      controller_binder_(std::move(controller_binder)),
      audio_broker_(
          std::make_unique<ReadAloudAudioBroker>(std::move(factory_binder))) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

VoicePreviewController::~VoicePreviewController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VoicePreviewController::PlayPreview(std::string_view voice_id,
                                         std::u16string_view preview_text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!voice_id.empty() && voice_id == voice_id_ &&
      utility_player_.is_bound()) {
    utility_player_->Play();
    return;
  }

  ResetControllerAndAudio();
  if (voice_id.empty() || preview_text.empty()) {
    if (state_changed_callback_) {
      state_changed_callback_.Run(voice_id,
                                  read_aloud::mojom::PlaybackState::kError);
    }
    return;
  }

  voice_id_ = std::string(voice_id);
  base::WeakPtr<VoicePreviewController> weak_this =
      preview_weak_factory_.GetWeakPtr();

  if (state_changed_callback_) {
    const std::string current_voice_id = voice_id_;
    state_changed_callback_.Run(current_voice_id,
                                read_aloud::mojom::PlaybackState::kBuffering);
    if (!weak_this) {
      return;
    }
  }

  BindNewPlaybackController();
  InitializeAudioStream();

  if (utility_player_.is_bound()) {
    std::vector<read_aloud::mojom::TextSegmentPtr> segments;
    segments.push_back(read_aloud::mojom::TextSegment::New(
        /*segment_index=*/0, std::u16string(preview_text),
        read_aloud::mojom::Speaker::kSpeaker1));
    utility_player_->Play();
    utility_player_->SetTextContent(std::move(segments));
  }
}

void VoicePreviewController::PausePreview() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsPreviewActive() || !utility_player_.is_bound()) {
    return;
  }
  utility_player_->Pause();
}

void VoicePreviewController::StopPreview() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsPreviewActive()) {
    return;
  }
  ResetControllerAndAudio();

  // Notify the UI/client delegate that voice preview playback has stopped.
  // Passing an empty string indicates that any active voice preview is stopped.
  if (state_changed_callback_) {
    state_changed_callback_.Run(/*voice_id=*/"",
                                read_aloud::mojom::PlaybackState::kStopped);
  }
}

void VoicePreviewController::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetControllerAndAudio();
  player_factory_.reset();
}

void VoicePreviewController::ResetActivePreviewState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  preview_weak_factory_.InvalidateWeakPtrs();
  voice_id_.clear();
}

void VoicePreviewController::ResetControllerAndAudio() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ResetActivePreviewState();
  weak_factory_.InvalidateWeakPtrs();
  utility_observer_receiver_.reset();
  utility_player_.reset();
  audio_broker_->Reset();
}

void VoicePreviewController::BindNewPlaybackController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  utility_observer_receiver_.reset();
  utility_player_.reset();
  mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
      controller_receiver = utility_player_.BindNewPipeAndPassReceiver();
  mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      client_remote = utility_observer_receiver_.BindNewPipeAndPassRemote();

  if (controller_binder_) {
    controller_binder_.Run(std::move(controller_receiver),
                           std::move(client_remote));
  } else {
    if (!player_factory_.is_bound()) {
      content::ServiceProcessHost::Launch<
          read_aloud::mojom::ReadAloudPlaybackControllerFactory>(
          player_factory_.BindNewPipeAndPassReceiver(),
          content::ServiceProcessHost::Options()
              // TODO(b/525116429): Use localized string resource.
              .WithDisplayName("ReadAloud Playback Service")
              .Pass());
      player_factory_.reset_on_disconnect();
    }
    player_factory_->CreateController(std::move(controller_receiver),
                                      std::move(client_remote));
  }

  utility_player_.set_disconnect_handler(
      base::BindOnce(&VoicePreviewController::OnUtilityDisconnect,
                     weak_factory_.GetWeakPtr()));
  utility_observer_receiver_.set_disconnect_handler(
      base::BindOnce(&VoicePreviewController::OnUtilityDisconnect,
                     weak_factory_.GetWeakPtr()));
}

void VoicePreviewController::InitializeAudioStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!utility_player_.is_bound()) {
    return;
  }

  // TODO(b/547989045): Query audio parameters from the OS rather than using
  // hardcoded constants.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(),
                                readaloud::kAudioSampleRate,
                                readaloud::kAudioFramesPerBuffer);

  // Voice previews play from Settings independent of any specific tab, so use
  // an isolated audio group ID rather than associating with a WebContents tab
  // muting/media-indicator group.
  audio_broker_->CreateOutputStream(
      base::UnguessableToken::Create(), params,
      base::BindOnce(&VoicePreviewController::OnAudioStreamCreated,
                     weak_factory_.GetWeakPtr(), params));
}

void VoicePreviewController::OnAudioStreamCreated(
    const media::AudioParameters& params,
    mojo::PendingRemote<media::mojom::AudioOutputStream> stream_remote,
    media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!utility_player_.is_bound()) {
    return;
  }
  if (!stream_remote.is_valid() || !data_pipe) {
    FinishWithError();
    return;
  }

  utility_player_->InitializeAudio(std::move(stream_remote),
                                   std::move(data_pipe), params);
}

void VoicePreviewController::OnUtilityDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FinishWithError();
}

void VoicePreviewController::FinishWithError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string finished_voice_id = std::move(voice_id_);
  Reset();
  if (state_changed_callback_ && !finished_voice_id.empty()) {
    state_changed_callback_.Run(finished_voice_id,
                                read_aloud::mojom::PlaybackState::kError);
  }
}

void VoicePreviewController::OnPlaybackStateChanged(
    read_aloud::mojom::PlaybackState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state == read_aloud::mojom::PlaybackState::kStopped ||
      state == read_aloud::mojom::PlaybackState::kPlaybackCreation) {
    utility_observer_receiver_.ReportBadMessage(
        "VoicePreviewController: browser-only PlaybackState received from "
        "utility");
    FinishWithError();
    return;
  }

  if (!IsPreviewActive()) {
    return;
  }

  switch (state) {
    case read_aloud::mojom::PlaybackState::kPlaying:
    case read_aloud::mojom::PlaybackState::kBuffering:
    case read_aloud::mojom::PlaybackState::kPaused:
      if (state_changed_callback_) {
        const std::string current_voice_id = voice_id_;
        state_changed_callback_.Run(current_voice_id, state);
      }
      return;
    case read_aloud::mojom::PlaybackState::kEndOfStream: {
      // TODO(b/565419447): Once kEndOfStream is driven by a real end-of-stream
      // signal after audio playout finishes, tear down the controller and audio
      // stream here via ResetControllerAndAudio(). Until then, keep them alive
      // so any remaining buffered audio can finish playing out.
      std::string finished_voice_id = std::move(voice_id_);
      ResetActivePreviewState();
      if (state_changed_callback_ && !finished_voice_id.empty()) {
        state_changed_callback_.Run(finished_voice_id,
                                    read_aloud::mojom::PlaybackState::kStopped);
      }
      return;
    }
    case read_aloud::mojom::PlaybackState::kError:
      FinishWithError();
      return;
    case read_aloud::mojom::PlaybackState::kStopped:
    case read_aloud::mojom::PlaybackState::kPlaybackCreation:
      NOTREACHED();
  }
}

void VoicePreviewController::RequestSpeechSynthesis(
    const std::u16string& text_chunk,
    read_aloud::mojom::Speaker /*speaker*/,
    uint64_t /*sequence_id*/,
    RequestSpeechSynthesisCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!speech_synthesis_broker_ || !IsPreviewActive() || text_chunk.empty() ||
      text_chunk.size() > readaloud::kMaxTextLengthPerSegment) {
    std::move(callback).Run(mojo_base::BigBuffer(), /*success=*/false);
    return;
  }

  OptimizationGuideKeyedService* opt_guide_service = nullptr;
  if (profile_) {
    opt_guide_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  }

  speech_synthesis_broker_->SynthesizeSpeech(
      opt_guide_service, text_chunk,
      /*voice_id_override=*/voice_id_,
      base::BindOnce(&VoicePreviewController::OnSpeechSynthesized,
                     preview_weak_factory_.GetWeakPtr(),
                     mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                         std::move(callback), mojo_base::BigBuffer(),
                         /*success=*/false)));
}

void VoicePreviewController::OnSpeechSynthesized(
    RequestSpeechSynthesisCallback callback,
    mojo_base::BigBuffer response_bytes,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(std::move(response_bytes), success);
}

}  // namespace readaloud
