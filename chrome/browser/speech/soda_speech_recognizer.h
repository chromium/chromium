// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SODA_SPEECH_RECOGNIZER_H_
#define CHROME_BROWSER_SPEECH_SODA_SPEECH_RECOGNIZER_H_

#include "chrome/browser/speech/speech_recognizer.h"

class SpeechRecognizerDelegate;

// SodaSpeechRecognizer is a wrapper around the on-device speech recognition
// engine that simplifies its use from the browser process.
class SodaSpeechRecognizer : public SpeechRecognizer {
 public:
  // Returns true if SODA is available and installed on-device.
  // TODO(katie): Might need to take a language/locale parameter once we know
  // how to check for per-language SODA downloads.
  static bool IsSodaSpeechRecognizerAvailable();

  explicit SodaSpeechRecognizer(
      const base::WeakPtr<SpeechRecognizerDelegate>& delegate);
  ~SodaSpeechRecognizer() override;
  SodaSpeechRecognizer(const SodaSpeechRecognizer&) = delete;
  SodaSpeechRecognizer& operator=(const SodaSpeechRecognizer&) = delete;

  // SpeechRecognizer:
  // Start and Stop must be called on the UI thread.
  void Start() override;
  void Stop() override;
};

#endif  // CHROME_BROWSER_SPEECH_SODA_SPEECH_RECOGNIZER_H_
