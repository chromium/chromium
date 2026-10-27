// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_CONVERSATION_IMPL_H_
#define CHROME_BROWSER_TTC_APP_CONVERSATION_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "components/ttc/app/ttc_backend.h"
#include "url/gurl.h"

namespace media {
class AudioParameters;
}

namespace optimization_guide::proto {
class AnnotatedPageContent;
}  // namespace optimization_guide::proto

namespace ttc {

class SessionController;

// Manages a voice/multimodal conversation session with the TTC model.
// Wires AudioController to TtcBackend for bidirectional streaming, handling
// speech capture, audio playback, interruptions, transcripts, and tools.
class ConversationImpl : public Conversation,
                         public AudioController::Observer,
                         public TtcBackend::Observer {
 public:
  // `backend` and `audio_controller` are required and provided to enable
  // dependency injection in tests. Use MakeConversationImpl() to create a
  // conversation using the production versions of these.
  ConversationImpl(std::unique_ptr<TtcBackend> backend,
                   std::unique_ptr<AudioController> audio_controller,
                   SessionController& session_controller);
  ~ConversationImpl() override;

  ConversationImpl(const ConversationImpl&) = delete;
  ConversationImpl& operator=(const ConversationImpl&) = delete;

  // Conversation implementation:
  void Start() override;
  void Stop() override;
  void SendTextInput(const std::string& text) override;
  void SendContextUpdate(
      const GURL& url,
      const std::string& title,
      const optimization_guide::proto::AnnotatedPageContent& apc) override;
  void OnPageContextInvalidated() override;
  void SetMicMuted(bool muted) override;
  bool IsMicMuted() const override;

  // AudioController::Observer implementation:
  void OnCapturedAudio(base::span<const int16_t> pcm_data,
                       const media::AudioParameters& params,
                       float energy) override;
  void OnCaptureStopped() override;
  void OnModelAudioEnvelopeReady(const AudioEnvelope& envelope) override;
  void OnPlaybackCompleted(int64_t sequence_number) override;
  void OnAudioError(ErrorCode error) override;

  // TtcBackend::Observer implementation:
  void OnBackendInitialized() override;
  void OnBackendClosed() override;
  void OnBackendError(ErrorCode error) override;
  void OnTranscriptions(const std::string& input_transcription,
                        const std::string& output_transcription) override;
  void OnAudioOutput(base::span<const int16_t> audio_data,
                     int64_t sequence_number) override;
  void OnGenerationStateChanged(bool started,
                                bool completed,
                                bool interrupted) override;
  void OnToolCall(const ToolRequest& tool_request,
                  ToolResponseCallback response_callback) override;
  void OnJournalEvent(const ServerJournalEvent& journal_event) override;

  AudioController* audio_controller() { return audio_controller_.get(); }
  TtcBackend* backend() { return backend_.get(); }

 private:
  std::unique_ptr<TtcBackend> backend_;
  std::unique_ptr<AudioController> audio_controller_;

  // Safe because the SessionController owns this object.
  const raw_ref<SessionController> session_controller_;

  base::ScopedObservation<AudioController, AudioController::Observer>
      audio_controller_observation_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_CONVERSATION_IMPL_H_
