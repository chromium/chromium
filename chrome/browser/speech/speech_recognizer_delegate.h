// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_DELEGATE_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_DELEGATE_H_

#include <stdint.h>

#include <optional>
#include <string>

#include "base/time/time.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

// Requires cleanup. See crbug.com/800374.
enum SpeechRecognizerStatus {
  SPEECH_RECOGNIZER_OFF = 0,
  // Ready for SpeechRecognizer::Start() to be called.
  SPEECH_RECOGNIZER_READY,
  // Beginning to listen for speech, but have not received any yet.
  SPEECH_RECOGNIZER_RECOGNIZING,
  // Sounds are being recognized.
  SPEECH_RECOGNIZER_IN_SPEECH,
  // There was an error.
  SPEECH_RECOGNIZER_ERROR,
  // Stopping speech recognition.
  SPEECH_RECOGNITION_STOPPING
};

// Delegate for speech recognizer. All methods are called from the thread on
// which this delegate was constructed.
class SpeechRecognizerDelegate {
 public:
  // Receive a speech recognition result. |is_final| indicated whether the
  // result is an intermediate or final result. If |is_final| is true, then the
  // recognizer stops and no more results will be returned.
  // May include word timing information in |full_result| if the speech
  // recognizer is an on device speech recognizer.
  virtual void OnSpeechResult(
      const std::u16string& text,
      bool is_final,
      const std::optional<media::SpeechRecognitionResult>& full_result) = 0;

  // Invoked regularly to indicate the average sound volume.
  virtual void OnSpeechSoundLevelChanged(int16_t level) = 0;

  // Invoked when the state of speech recognition is changed.
  virtual void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) = 0;

  // Invoked when the speech recognition has stopped.
  virtual void OnSpeechRecognitionStopped() = 0;

  // Invoked when a language identification event
  // during speech recognition has been received.
  virtual void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) = 0;

 protected:
  virtual ~SpeechRecognizerDelegate() {}
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_DELEGATE_H_
