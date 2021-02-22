// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_DELEGATE_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_DELEGATE_H_

#include <stdint.h>
#include <string>

#include "base/strings/string16.h"
#include "base/time/time.h"

// Requires cleanup. See crbug.com/800374.
enum SpeechRecognizerStatus {
  SPEECH_RECOGNIZER_OFF = 0,
  SPEECH_RECOGNIZER_READY,
  SPEECH_RECOGNIZER_RECOGNIZING,
  SPEECH_RECOGNIZER_IN_SPEECH,
  SPEECH_RECOGNIZER_STOPPING,
  SPEECH_RECOGNIZER_NETWORK_ERROR,
};

// Delegate for speech recognizer. All methods are called from the thread on
// which this delegate was constructed.
class SpeechRecognizerDelegate {
 public:
  // Receive a speech recognition result. |is_final| indicated whether the
  // result is an intermediate or final result. If |is_final| is true, then the
  // recognizer stops and no more results will be returned.
  // May include word timing information in |word_offsets|.
  virtual void OnSpeechResult(
      const base::string16& text,
      bool is_final,
      base::Optional<std::vector<base::TimeDelta>> word_offsets) = 0;

  // Invoked regularly to indicate the average sound volume.
  virtual void OnSpeechSoundLevelChanged(int16_t level) = 0;

  // Invoked when the state of speech recognition is changed.
  virtual void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) = 0;

 protected:
  virtual ~SpeechRecognizerDelegate() {}
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_DELEGATE_H_
