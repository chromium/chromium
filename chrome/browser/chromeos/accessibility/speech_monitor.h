// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SPEECH_MONITOR_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SPEECH_MONITOR_H_

#include <chrono>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/test/test_utils.h"

// TODO(katie): This may need to move into Content as part of the TTS refactor.

namespace chromeos {

struct SpeechMonitorUtterance {
  SpeechMonitorUtterance(std::string text_, std::string lang_)
      : text(text_), lang(lang_) {}
  std::string text;
  std::string lang;
};

// For testing purpose installs itself as the platform speech synthesis engine,
// allowing it to intercept all speech calls, and then provides a method to
// block until the next utterance is spoken.
class SpeechMonitor : public content::TtsPlatform {
 public:
  SpeechMonitor();
  virtual ~SpeechMonitor();

  // Blocks until the next utterance is spoken, and returns its text.
  std::string GetNextUtterance();
  // Blocks until the next utterance is spoken, and returns its text.
  SpeechMonitorUtterance GetNextUtteranceWithLanguage();

  // Wait for next utterance and return true if next utterance is ChromeVox
  // enabled message.
  bool SkipChromeVoxEnabledMessage();
  bool SkipChromeVoxMessage(const std::string& message);

  // Returns true if StopSpeaking() was called on TtsController.
  bool DidStop();

  // Blocks until StopSpeaking() is called on TtsController.
  void BlockUntilStop();

  // Delayed utterances.
  double GetDelayForLastUtteranceMS();

 private:
  // TtsPlatform implementation.
  bool PlatformImplAvailable() override;
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const content::VoiceData& voice,
             const content::UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override;
  bool StopSpeaking() override;
  bool IsSpeaking() override;
  void GetVoices(std::vector<content::VoiceData>* out_voices) override;
  void Pause() override {}
  void Resume() override {}
  void WillSpeakUtteranceWithVoice(
      content::TtsUtterance* utterance,
      const content::VoiceData& voice_data) override;
  bool LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override;
  std::string GetError() override;
  void ClearError() override;
  void SetError(const std::string& error) override;

  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  // Our list of utterances and specified language.
  base::circular_deque<SpeechMonitorUtterance> utterance_queue_;
  bool did_stop_ = false;
  std::string error_;

  // Delayed utterances.
  // Calculates the milliseconds elapsed since the last call to Speak().
  double CalculateUtteranceDelayMS();
  // Stores the milliseconds elapsed since the last call to Speak().
  double delay_for_last_utterance_MS_;
  // Stores the last time Speak() was called.
  std::chrono::steady_clock::time_point time_of_last_utterance_;

  DISALLOW_COPY_AND_ASSIGN(SpeechMonitor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SPEECH_MONITOR_H_
