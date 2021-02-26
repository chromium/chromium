// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognizer.h"

#include "chrome/browser/speech/speech_recognizer_delegate.h"

bool OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable() {
  return false;
}

OnDeviceSpeechRecognizer::OnDeviceSpeechRecognizer(
    const base::WeakPtr<SpeechRecognizerDelegate>& delegate)
    : SpeechRecognizer(delegate) {}

OnDeviceSpeechRecognizer::~OnDeviceSpeechRecognizer() {
  Stop();
}

void OnDeviceSpeechRecognizer::Start() {}

void OnDeviceSpeechRecognizer::Stop() {}
