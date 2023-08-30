// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SPEECH_MONITOR_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SPEECH_MONITOR_H_

#include <chrono>
#include <map>

#include "base/containers/circular_deque.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/test/test_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// TODO(katie): This may need to move into Content as part of the TTS refactor.

namespace ash {
namespace test {

struct SpeechMonitorUtterance {
  SpeechMonitorUtterance(std::string text_, std::string lang_)
      : text(text_), lang(lang_) {}
  std::string text;
  std::string lang;
};

// For testing purpose installs itself as the platform speech synthesis engine,
// allowing it to intercept all speech calls. Provides an api to make
// asynchronous function calls and expectations about resulting speech.
class SpeechMonitor : public content::TtsPlatform {
 public:
  SpeechMonitor();

  SpeechMonitor(const SpeechMonitor&) = delete;
  SpeechMonitor& operator=(const SpeechMonitor&) = delete;

  virtual ~SpeechMonitor();

  // Holds an expectation for utterances.
  class Expectation {
   public:
    explicit Expectation(const std::string& text);
    ~Expectation();
    Expectation(const Expectation&);

    // Sets to perform regular expression matching.
    Expectation& AsPattern(bool enable = true) {
      as_pattern_ = true;
      return *this;
    }

    // Sets the expected locale for the given text.
    Expectation& WithLocale(const std::string& locale) {
      locale_ = locale;
      return *this;
    }

    // Sets the unexpected utterances until this consumes the expectation.
    Expectation& WithoutText(const std::vector<std::string>& text) {
      disallowed_text_ = text;
      return *this;
    }

    // Checks the given list of utterances matches this expectation.
    // Returns the iterator that points the matched item in the given list.
    // If not matched, returns the end() of the given list.
    base::circular_deque<SpeechMonitorUtterance>::const_iterator Matches(
        const base::circular_deque<SpeechMonitorUtterance>& queue) const;

    std::string ToString() const;

   private:
    std::string OptionsToString() const;

    std::string text_;
    bool as_pattern_ = false;
    absl::optional<std::string> locale_;
    std::vector<std::string> disallowed_text_;
  };

  // Use these apis if you want to write an async test e.g.
  // sm_.ExpectSpeech("foo");
  // sm_.Call([this]() { DoSomething(); })
  // sm_.Replay();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpredefined-identifier-outside-function"

  // Adds an expectation of spoken text.
  void ExpectSpeech(const Expectation& expectation,
                    const base::Location& location = FROM_HERE);
  void ExpectSpeech(const std::string& text,
                    const base::Location& location = FROM_HERE);
  void ExpectSpeechPattern(const std::string& pattern,
                           const base::Location& location = FROM_HERE);
  void ExpectNextSpeechIsNot(const std::string& text,
                             const base::Location& location = FROM_HERE);
  void ExpectNextSpeechIsNotPattern(const std::string& pattern,
                                    const base::Location& location = FROM_HERE);
  void ExpectHadNoRepeatedSpeech(const base::Location& location = FROM_HERE);

  // TTS parameters are harder to match against the entire spoken text, so the
  // expectations here work a bit more loosely:
  // * For matching text, use the methods above;
  // * use this to check if some TTS parameters were set when a specific piece
  // of text was being spoken.
  absl::optional<content::UtteranceContinuousParameters>
  GetParamsForPreviouslySpokenTextPattern(const std::string& pattern);

  // Adds a call to be included in replay.
  void Call(std::function<void()> func,
            const base::Location& location = FROM_HERE);

#pragma clang diagnostic pop

  // Replays all expectations.
  void Replay();

  // Delayed utterances.
  double GetDelayForLastUtteranceMS();

  int stop_count() { return stop_count_; }

 private:
  typedef std::pair<std::function<bool()>, std::string> ReplayArgs;

  // TtsPlatform implementation.
  bool PlatformImplSupported() override;
  bool PlatformImplInitialized() override;
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
  void LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override;
  std::string GetError() override;
  void ClearError() override;
  void SetError(const std::string& error) override;
  void Shutdown() override;
  void FinalizeVoiceOrdering(std::vector<content::VoiceData>& voices) override;
  void RefreshVoices() override;
  content::ExternalPlatformDelegate* GetExternalPlatformDelegate() override;

  void MaybeContinueReplay();
  void MaybePrintExpectations();

  // Our list of utterances and specified language.
  base::circular_deque<SpeechMonitorUtterance> utterance_queue_;

  std::string error_;

  // Calculates the milliseconds elapsed since the last call to Speak().
  double CalculateUtteranceDelayMS();

  // Stores the milliseconds elapsed since the last call to Speak().
  double delay_for_last_utterance_ms_;

  // Stores the last time Speak() was called.
  std::chrono::steady_clock::time_point time_of_last_utterance_;

  // Queue of expectations to be replayed.
  std::vector<ReplayArgs> replay_queue_;

  // Queue of expectations already satisfied.
  std::vector<std::string> replayed_queue_;

  // List of parameters for a given text.
  std::map<std::string, content::UtteranceContinuousParameters> text_params_;

  // Blocks this test when replaying expectations.
  scoped_refptr<content::MessageLoopRunner> replay_loop_runner_;

  // Used to track the size of |replay_queue_| for knowing when to print errors.
  size_t last_replay_queue_size_ = 0;

  // Whether |Replay| was called.
  bool replay_called_ = false;

  // The number of times StopSpeaking() has been called.
  int stop_count_ = 0;

  // Indicates if there were two consecutive utterances that match (i.e.
  // repeated speech).
  std::vector<std::string> repeated_speech_;

  base::WeakPtrFactory<SpeechMonitor> weak_factory_{this};
};

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SPEECH_MONITOR_H_
