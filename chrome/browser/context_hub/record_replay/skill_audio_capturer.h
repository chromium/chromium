// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_AUDIO_CAPTURER_H_
#define CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_AUDIO_CAPTURER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/common/audio_data_s16_converter.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace capture_mode {
class AudioCapturer;
}  // namespace capture_mode

namespace media {
class AudioBus;
class AudioSystem;
struct SpeechRecognitionResult;
}  // namespace media

namespace context_hub {

// Coordinates audio capture and speech-to-text transcription during a skill
// recording session, forwarding timestamped transcript updates via
// `OnTranscriptUpdateCallback`.
class SkillAudioCapturer
    : public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  using OnTranscriptUpdateCallback = base::RepeatingCallback<
      void(const std::string& text, base::TimeDelta timestamp, bool is_final)>;

  // `profile` and `on_transcript_update_callback` must be non-null. `profile`
  // must outlive this `SkillAudioCapturer`.
  SkillAudioCapturer(Profile* profile,
                     OnTranscriptUpdateCallback on_transcript_update_callback);
  SkillAudioCapturer(const SkillAudioCapturer&) = delete;
  SkillAudioCapturer& operator=(const SkillAudioCapturer&) = delete;
  ~SkillAudioCapturer() override;

  // Starts microphone capture and binds the speech recognizer if not already
  // recording.
  void Start();

  // Stops microphone capture and resets the speech recognition bindings if
  // currently recording.
  void Stop();

  bool is_recording() const { return is_recording_; }

  // Overrides the `media::AudioSystem` used to query input device parameters in
  // tests.
  void SetAudioSystemForTesting(
      std::unique_ptr<media::AudioSystem> audio_system);

  const media::AudioParameters& audio_params_for_testing() const {
    return audio_params_;
  }

  // Forwards `audio_bus` to `OnAudioCaptured()` and flushes the speech
  // recognizer pipe for testing.
  void OnAudioCapturedForTesting(std::unique_ptr<media::AudioBus> audio_bus,
                                 base::TimeTicks audio_capture_time);

  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override;
  void OnSpeechRecognitionStopped() override;
  void OnSpeechRecognitionError() override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;

 private:
  void InitializeSpeechRecognition();
  void OnRecognizerBound(bool is_multichannel_supported);
  void OnInputStreamParametersReceived(
      const std::optional<media::AudioParameters>& device_params);
  void OnAudioCaptured(std::unique_ptr<media::AudioBus> audio_bus,
                       base::TimeTicks audio_capture_time);

  const raw_ref<Profile> profile_;
  const OnTranscriptUpdateCallback on_transcript_update_callback_;
  bool is_recording_ = false;
  bool is_multichannel_supported_ = false;
  base::TimeTicks session_start_time_;
  media::AudioParameters audio_params_;
  media::AudioDataS16Converter audio_converter_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  std::unique_ptr<capture_mode::AudioCapturer> audio_capturer_;

  mojo::Remote<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_;
  mojo::Remote<media::mojom::SpeechRecognitionRecognizer> speech_recognizer_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognizer_client_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SkillAudioCapturer> weak_ptr_factory_{this};
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_AUDIO_CAPTURER_H_
