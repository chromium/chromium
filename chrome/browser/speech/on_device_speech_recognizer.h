// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNIZER_H_
#define CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNIZER_H_

#include "chrome/browser/speech/speech_recognizer.h"

class SpeechRecognizerDelegate;

// OnDeviceSpeechRecognizer is a wrapper around the on-device speech recognition
// engine that simplifies its use from the browser process.
class OnDeviceSpeechRecognizer : public SpeechRecognizer {
 public:
  // Returns true if on-device speech recognition is available and installed
  // on-device.
  // TODO(katie): Might need to take a language/locale parameter once we know
  // how to check for per-language downloads.
  static bool IsOnDeviceSpeechRecognizerAvailable();

  explicit OnDeviceSpeechRecognizer(
      const base::WeakPtr<SpeechRecognizerDelegate>& delegate);
  ~OnDeviceSpeechRecognizer() override;
  OnDeviceSpeechRecognizer(const OnDeviceSpeechRecognizer&) = delete;
  OnDeviceSpeechRecognizer& operator=(const OnDeviceSpeechRecognizer&) = delete;

  // SpeechRecognizer:
  // Start and Stop must be called on the UI thread.
  void Start() override;
  void Stop() override;
};

#endif  // CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNIZER_H_
