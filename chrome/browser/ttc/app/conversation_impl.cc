// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/conversation_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chrome/browser/ttc/app/audio_controller.h"
#include "chrome/browser/ttc/core/session_controller.h"

namespace ttc {

ConversationImpl::ConversationImpl(
    std::unique_ptr<TtcBackend> backend,
    std::unique_ptr<AudioController> audio_controller,
    SessionController& session_controller)
    : backend_(std::move(backend)),
      audio_controller_(std::move(audio_controller)),
      session_controller_(session_controller) {
  CHECK(backend_);
  CHECK(audio_controller_);
}

ConversationImpl::~ConversationImpl() {
  Stop();
}

void ConversationImpl::AddObserver(Conversation::Observer* observer) {
  observers_.AddObserver(observer);
}

void ConversationImpl::RemoveObserver(Conversation::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ConversationImpl::Start() {
  audio_capture_subscription_ =
      audio_controller_->AddAudioCaptureListener(base::BindRepeating(
          &ConversationImpl::OnCapturedAudio, base::Unretained(this)));
  audio_energy_subscription_ =
      audio_controller_->AddAudioEnergyListener(base::BindRepeating(
          &ConversationImpl::OnAudioEnergy, base::Unretained(this)));
  playback_completion_subscription_ =
      audio_controller_->AddPlaybackCompletionListener(base::BindRepeating(
          &ConversationImpl::OnPlaybackCompleted, base::Unretained(this)));
  audio_controller_->StartCapture();

  backend_->Connect(this);

  // TODO(b/562979451): This needs to be called once we've received a reply.
  // Also audio playback/capture should be started only then too. Make this
  // change once backend is reliably hooked up.
  session_controller_->OnSessionInitialized();
}

void ConversationImpl::Stop() {
  audio_capture_subscription_ = {};
  audio_energy_subscription_ = {};
  playback_completion_subscription_ = {};

  audio_controller_->StopCapture();
  audio_controller_->StopPlayback();

  if (backend_->is_connected()) {
    backend_->Close();
  }
}

bool ConversationImpl::is_connected() const {
  return backend_->is_connected();
}

void ConversationImpl::SendTextInput(const std::string& text) {
  backend_->SendTextInput(text);
}

void ConversationImpl::SendContextUpdate(
    const GURL& url,
    const std::string& title,
    const optimization_guide::proto::AnnotatedPageContent& apc) {
  backend_->SendContextUpdate(url, title, apc);
}

void ConversationImpl::OnPageContextChanged() {
  NOTIMPLEMENTED();
}

void ConversationImpl::OnCapturedAudio(base::span<const int16_t> pcm_data,
                                       const media::AudioParameters& params) {
  backend_->SendAudioChunk(pcm_data);
}

void ConversationImpl::OnAudioEnergy(float energy) {
  session_controller_->UserAudioLevelUpdate(energy);
}

void ConversationImpl::OnPlaybackCompleted(int64_t sequence_number) {
  backend_->ReportPlaybackStatus(sequence_number);
}

void ConversationImpl::OnStreamingStateChanged(
    bool connected,
    const std::string& session_id,
    const std::string& error_message) {
  // TODO(b/561651267): Technically we only need to send this when a session is
  // established. This method can be called after an already created session
  // reconnects after a temporary disconnection. Differentiating this state
  // would require us to cache the session_id, and ideally TtcBackend::Observer
  // would just expose a separate notification for this.
  if (connected && !session_id.empty()) {
    // `backend_` is necessarily alive here since it invoked this
    // TtcBackend::Observer call.
    CHECK(backend_);
    backend_->SendToolSetUpdate(session_controller_->GetToolDefinitions());
  }

  for (auto& observer : observers_) {
    observer.OnConversationStateChanged(connected, session_id, error_message);
  }
}

void ConversationImpl::OnTranscriptions(
    const std::string& input_transcription,
    const std::string& output_transcription) {
  for (auto& observer : observers_) {
    observer.OnTranscriptions(input_transcription, output_transcription);
  }
}

void ConversationImpl::OnAudioOutput(base::span<const int16_t> audio_data,
                                     int64_t sequence_number) {
  audio_controller_->PlayAudio(audio_data, sequence_number);
}

void ConversationImpl::OnGenerationStateChanged(bool started,
                                                bool completed,
                                                bool interrupted) {
  if (interrupted) {
    audio_controller_->ClearPlaybackQueue();
  }
  for (auto& observer : observers_) {
    observer.OnGenerationStateChanged(started, completed, interrupted);
  }
}

void ConversationImpl::OnToolCall(const ToolRequest& tool_request,
                                  ToolResponseCallback response_callback) {
  session_controller_->ProcessToolCall(tool_request,
                                       std::move(response_callback));
}

}  // namespace ttc
