// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_CONVERSATION_IMPL_H_
#define CHROME_BROWSER_TTC_APP_CONVERSATION_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/observer_list.h"
#include "chrome/browser/ttc/app/audio_controller.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/app/ttc_backend.h"
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
class ConversationImpl : public Conversation, public TtcBackend::Observer {
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
  void AddObserver(Conversation::Observer* observer) override;
  void RemoveObserver(Conversation::Observer* observer) override;
  void Start() override;
  void Stop() override;
  bool is_connected() const override;
  void SendTextInput(const std::string& text) override;
  void SendContextUpdate(
      const GURL& url,
      const std::string& title,
      const optimization_guide::proto::AnnotatedPageContent& apc) override;
  void OnPageContextChanged() override;

  // TtcBackend::Observer implementation:
  void OnStreamingStateChanged(bool connected,
                               const std::string& session_id,
                               const std::string& error_message) override;
  void OnTranscriptions(const std::string& input_transcription,
                        const std::string& output_transcription) override;
  void OnAudioOutput(base::span<const int16_t> audio_data,
                     int64_t sequence_number) override;
  void OnGenerationStateChanged(bool started,
                                bool completed,
                                bool interrupted) override;
  void OnToolCall(const ToolRequest& tool_request,
                  ToolResponseCallback response_callback) override;

  AudioController* audio_controller() { return audio_controller_.get(); }
  TtcBackend* backend() { return backend_.get(); }

 private:
  void OnCapturedAudio(base::span<const int16_t> pcm_data,
                       const media::AudioParameters& params);
  void OnAudioEnergy(float energy);
  void OnPlaybackCompleted(int64_t sequence_number);

  std::unique_ptr<TtcBackend> backend_;
  std::unique_ptr<AudioController> audio_controller_;

  // Safe because the SessionController owns this object.
  const raw_ref<SessionController> session_controller_;

  base::CallbackListSubscription audio_capture_subscription_;
  base::CallbackListSubscription audio_energy_subscription_;
  base::CallbackListSubscription playback_completion_subscription_;
  base::ObserverList<Conversation::Observer> observers_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_CONVERSATION_IMPL_H_
