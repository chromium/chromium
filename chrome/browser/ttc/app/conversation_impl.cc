// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/conversation_impl.h"

#include <utility>

#include "base/check.h"
#include "base/notimplemented.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/states.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/ttc/app/public/error_codes.h"
#include "components/ttc/app/public/session_journal.h"

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

void ConversationImpl::Start() {
  audio_controller_observation_.Observe(audio_controller_.get());
  backend_->Connect(this);
}

void ConversationImpl::Stop() {
  audio_controller_observation_.Reset();

  audio_controller_->StopCapture();
  audio_controller_->StopPlayback();

  if (backend_->is_transport_connected()) {
    backend_->Close();
  }
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

void ConversationImpl::OnPageContextInvalidated() {
  NOTIMPLEMENTED();
}

void ConversationImpl::SetMicMuted(bool muted) {
  if (muted) {
    audio_controller_->StopCapture();
  } else {
    audio_controller_->StartCapture(/*device_id=*/"");
  }
}

bool ConversationImpl::IsMicMuted() const {
  return !audio_controller_->is_capturing();
}

void ConversationImpl::OnCapturedAudio(base::span<const int16_t> pcm_data,
                                       const media::AudioParameters& params,
                                       float energy) {
  backend_->SendAudioChunk(pcm_data);
  session_controller_->UserAudioLevelUpdate(energy);
}

void ConversationImpl::OnCaptureStopped() {
  // No more audio levels arrive, so settle the UI at silence.
  session_controller_->UserAudioLevelUpdate(0.0f);
}

void ConversationImpl::OnModelAudioEnvelopeReady(
    const AudioEnvelope& envelope) {
  session_controller_->OnModelAudioEnvelopeReady(envelope);
}

void ConversationImpl::OnPlaybackCompleted(int64_t sequence_number) {
  backend_->ReportPlaybackStatus(sequence_number);
}

void ConversationImpl::OnAudioError(ErrorCode error) {
  session_controller_->OnError(error);
}

void ConversationImpl::OnBackendInitialized() {
  if (session_controller_->GetSessionLifecycle() ==
      SessionLifecycle::kFinished) {
    return;
  }
  backend_->SendToolSetUpdate(session_controller_->GetToolDefinitions());
  session_controller_->SetSessionLifecycle(SessionLifecycle::kLive);
  audio_controller_->StartCapture(/*device_id=*/"");
  session_controller_->OnSessionInitialized();
}

void ConversationImpl::OnBackendClosed() {
  session_controller_->GetJournal().Log("TtcBackendClosed", {});
  session_controller_->EndSessionAsync();
}

void ConversationImpl::OnBackendError(ErrorCode error) {
  session_controller_->OnError(error);
}

void ConversationImpl::OnTranscriptions(
    const std::string& input_transcription,
    const std::string& output_transcription) {
  if (!input_transcription.empty()) {
    session_controller_->OnInputTranscription(input_transcription);
  }
  if (!output_transcription.empty()) {
    session_controller_->OnOutputTranscription(output_transcription);
  }
}

void ConversationImpl::OnAudioOutput(base::span<const int16_t> audio_data,
                                     int64_t sequence_number) {
  audio_controller_->PlayAudio(audio_data, sequence_number);
}

void ConversationImpl::OnGenerationStateChanged(bool started,
                                                bool completed,
                                                bool interrupted) {
  session_controller_->GetJournal().Log("TtcGenerationStateChanged",
                                        actor::JournalDetailsBuilder()
                                            .Add("started", started)
                                            .Add("completed", completed)
                                            .Add("interrupted", interrupted)
                                            .Build());
  if (interrupted) {
    audio_controller_->ClearPlaybackQueue();
    session_controller_->OnModelInterrupted();
  }
  if (started) {
    session_controller_->OnModelTurnStarted();
  }
  if (completed) {
    session_controller_->OnModelTurnComplete();
  }
}

void ConversationImpl::OnToolCall(const ToolRequest& tool_request,
                                  ToolResponseCallback response_callback) {
  session_controller_->ProcessToolCall(tool_request,
                                       std::move(response_callback));
}

void ConversationImpl::OnJournalEvent(const ServerJournalEvent& journal_event) {
  session_controller_->GetJournal().HandleServerJournalEvent(journal_event);
}

}  // namespace ttc
