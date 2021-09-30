// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_RECOGNIZER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_RECOGNIZER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "components/soda/constants.h"

class SpeechRecognizer;

namespace extensions {

using OnResultCallback =
    base::RepeatingCallback<void(const std::u16string& transcript,
                                 bool isFinal)>;

using OnErrorCallback =
    base::RepeatingCallback<void(const std::string& message)>;

// This class is a wrapper around SpeechRecognizer and can be used to start and
// stop speech recognition. It routes speech recognition events to the API
// manager using callbacks. It is also responsible for deciding whether to
// use the on-device or network speech recognition.
class SpeechRecognitionPrivateRecognizer : public SpeechRecognizerDelegate {
 public:
  SpeechRecognitionPrivateRecognizer(base::RepeatingClosure on_stop_calback,
                                     OnResultCallback on_result_callback,
                                     OnErrorCallback on_error_callback);
  ~SpeechRecognitionPrivateRecognizer() override;

  // SpeechRecognizerDelegate:
  void OnSpeechResult(const std::u16string& text,
                      bool is_final,
                      const absl::optional<media::SpeechRecognitionResult>&
                          full_result) override;
  void OnSpeechSoundLevelChanged(int16_t level) override {}
  void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) override;

  // Handles a call to start speech recognition.
  void HandleStart(absl::optional<std::string> locale,
                   absl::optional<bool> interim_results,
                   base::OnceClosure callback);
  // Handles a call to stop speech recognition. The callback accepts an
  // optional string specifying an error message, if any.
  void HandleStop(
      base::OnceCallback<void(absl::optional<std::string>)> callback);

  std::string locale() { return locale_; }
  bool interim_results() { return interim_results_; }
  SpeechRecognizerStatus current_state() { return current_state_; }

 private:
  friend class SpeechRecognitionPrivateRecognizerTest;

  // Turns the speech recognizer off.
  void RecognizerOff();

  // Updates properties used for speech recognition.
  void MaybeUpdateProperties(absl::optional<std::string> locale,
                             absl::optional<bool> interim_results,
                             base::OnceClosure callback);

  base::WeakPtr<SpeechRecognitionPrivateRecognizer> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  SpeechRecognizerStatus current_state_ = SPEECH_RECOGNIZER_OFF;
  std::string locale_ = speech::kUsEnglishLocale;
  bool interim_results_ = false;
  // A callback that is run when speech recognition starts. Note, this is a
  // OnceClosure and is updated whenever HandleStart() is called.
  base::OnceClosure on_start_callback_;
  // A callback that is run whenever speech recognition stops.
  base::RepeatingClosure on_stop_callback_;
  // A callback that is run whenever speech recognition results are received
  // from the speech recognition service.
  OnResultCallback on_result_callback_;
  // A callback that is run whenever a speech recognition error occurs.
  OnErrorCallback on_error_callback_;
  std::unique_ptr<SpeechRecognizer> speech_recognizer_;

  base::WeakPtrFactory<SpeechRecognitionPrivateRecognizer> weak_ptr_factory_{
      this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_RECOGNIZER_H_
