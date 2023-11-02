// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace speech {

class SpeechRecognitionService : public KeyedService {
 public:
  virtual void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
          receiver) = 0;
  virtual void BindAudioSourceSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
          receiver) = 0;
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_H_
