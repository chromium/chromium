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
#include "base/observer_list.h"
#include "chrome/browser/ttc/app/audio_controller.h"
#include "chrome/browser/ttc/app/ttc_backend.h"
#include "chrome/browser/ttc/conversation.h"
#include "url/gurl.h"

class Profile;

namespace media {
class AudioParameters;
}

namespace optimization_guide::proto {
class AnnotatedPageContent;
}  // namespace optimization_guide::proto

namespace ttc {

// Downsamples 48kHz 16-bit mono PCM to 16kHz using a 3:1 moving average filter.
std::vector<uint8_t> Downsample48kHzTo16kHz(base::span<const uint8_t> pcm_data);

// Manages a voice/multimodal conversation session with the TTC model.
// Wires AudioController to TtcBackend for bidirectional streaming, handling
// speech capture, audio playback, interruptions, transcripts, and tools.
class ConversationImpl : public Conversation, public TtcBackend::Observer {
 public:
  explicit ConversationImpl(Profile* profile);
  ConversationImpl(std::unique_ptr<TtcBackend> backend,
                   std::unique_ptr<AudioController> audio_controller);
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
  void SendToolSetUpdate(const std::vector<ToolDefinition>& tools) override;
  void OnPageContextChanged() override;

  // TtcBackend::Observer implementation:
  void OnStreamingStateChanged(bool connected,
                               const std::string& session_id,
                               const std::string& error_message) override;
  void OnTranscriptions(const std::string& input_transcription,
                        const std::string& output_transcription) override;
  void OnAudioOutput(const std::vector<uint8_t>& audio_data,
                     int64_t sequence_number) override;
  void OnGenerationStateChanged(bool started,
                                bool completed,
                                bool interrupted) override;
  void OnToolCall(const std::string& name,
                  base::DictValue arguments,
                  ToolResponseCallback response_callback) override;

  AudioController* audio_controller() { return audio_controller_.get(); }
  TtcBackend* backend() { return backend_.get(); }

 private:
  void OnCapturedAudio(const std::vector<uint8_t>& pcm_data,
                       const media::AudioParameters& params);
  void OnPlaybackCompleted(int64_t sequence_number);

  std::unique_ptr<TtcBackend> backend_;
  std::unique_ptr<AudioController> audio_controller_;

  base::CallbackListSubscription audio_capture_subscription_;
  base::CallbackListSubscription playback_completion_subscription_;
  base::ObserverList<Conversation::Observer> observers_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_CONVERSATION_IMPL_H_
