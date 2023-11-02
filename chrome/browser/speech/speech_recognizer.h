// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_

#include "base/memory/weak_ptr.h"

class SpeechRecognizerDelegate;

// SpeechRecognizer is an interface for microphone-based speech recognition from
// the browser process.
class SpeechRecognizer {
 public:
  explicit SpeechRecognizer(
      const base::WeakPtr<SpeechRecognizerDelegate>& delegate);
  virtual ~SpeechRecognizer();

  // Starts speech recognition using the microphone. Results (or errors) will be
  // returned via the SpeechRecognizerDelegate.
  virtual void Start() = 0;

  // Stops speech recognition.
  virtual void Stop() = 0;

 protected:
  base::WeakPtr<SpeechRecognizerDelegate> delegate() { return delegate_; }

 private:
  base::WeakPtr<SpeechRecognizerDelegate> delegate_;
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNIZER_H_
