// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/soda_speech_recognizer.h"

#include "chrome/browser/speech/speech_recognizer_delegate.h"

bool SodaSpeechRecognizer::IsSodaSpeechRecognizerAvailable() {
  return false;
}

SodaSpeechRecognizer::SodaSpeechRecognizer(
    const base::WeakPtr<SpeechRecognizerDelegate>& delegate)
    : SpeechRecognizer(delegate) {}

SodaSpeechRecognizer::~SodaSpeechRecognizer() {
  Stop();
}

void SodaSpeechRecognizer::Start() {}

void SodaSpeechRecognizer::Stop() {}
