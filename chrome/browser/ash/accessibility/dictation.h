// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "content/public/browser/speech_recognition_session_preamble.h"
#include "ui/base/ime/input_method_observer.h"

namespace ui {
struct CompositionText;
class TextInputClient;
}  // namespace ui

class Profile;
class SpeechRecognizer;

namespace ash {

// Provides global dictation (type what you speak) on Chrome OS.
class Dictation : public SpeechRecognizerDelegate,
                  public ui::InputMethodObserver {
 public:
  // Stores whether locales are supported by offline speech recognition and
  // if the corresponding language pack is installed.
  struct LocaleData {
    bool works_offline = false;
    bool installed = false;
  };

  // Gets the default locale given a user |profile|. If this is a |new_user|,
  // uses the application language. Otherwise uses previous method of
  // determining Dictation language with default IME language.
  // This is guaranteed to return a supported BCP-47 locale.
  static std::string DetermineDefaultSupportedLocale(Profile* profile,
                                                     bool new_user);

  // Gets all possible BCP-47 style locale codes supported by Dictation,
  // and whether they are available offline.
  static const base::flat_map<std::string, LocaleData> GetAllSupportedLocales();

  explicit Dictation(Profile* profile);

  Dictation(const Dictation&) = delete;
  Dictation& operator=(const Dictation&) = delete;

  ~Dictation() override;

  // User-initiated dictation.
  bool OnToggleDictation();

 private:
  friend class DictationTest;

  // SpeechRecognizerDelegate:
  void OnSpeechResult(const std::u16string& transcription,
                      bool is_final,
                      const absl::optional<media::SpeechRecognitionResult>&
                          full_result) override;
  void OnSpeechSoundLevelChanged(int16_t level) override;
  void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) override;
  void OnSpeechRecognitionStopped() override {}

  // ui::InputMethodObserver:
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}
  void OnFocus() override {}
  void OnBlur() override {}

  // Starts a timer for |timeout_duration|. When the timer expires, will stop
  // capturing audio and finalize any pending utterances.
  void StartSpeechTimeout(base::TimeDelta timeout_duration);
  void StopSpeechTimeout();
  void OnSpeechTimeout();

  // Saves current dictation result and stops listening.
  void DictationOff();

  // Commits the current composition text.
  void CommitCurrentText();

  // Whether Dictation is toggled on or off.
  bool is_started_ = false;
  SpeechRecognizerStatus current_state_;
  bool has_committed_text_ = false;

  std::unique_ptr<SpeechRecognizer> speech_recognizer_;
  std::unique_ptr<ui::CompositionText> composition_;

  Profile* profile_;

  base::OneShotTimer speech_timeout_;

  // Used for metrics.
  bool used_on_device_speech_ = false;
  base::ElapsedTimer listening_duration_timer_;

  base::WeakPtrFactory<Dictation> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_DICTATION_H_
