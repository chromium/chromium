// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_SPEECH_RECOGNITION_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_SPEECH_RECOGNITION_ASH_H_

#include "chromeos/crosapi/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

class SpeechRecognitionAsh : public mojom::SpeechRecognition {
 public:
  SpeechRecognitionAsh();
  SpeechRecognitionAsh(const SpeechRecognitionAsh&) = delete;
  SpeechRecognitionAsh& operator=(const SpeechRecognitionAsh&) = delete;
  ~SpeechRecognitionAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::SpeechRecognition> receiver);

  // mojom::SpeechRecognition:
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver)
      override;
  void BindSpeechRecognitionClientBrowserInterface(
      mojo::PendingReceiver<
          media::mojom::SpeechRecognitionClientBrowserInterface> receiver)
      override;

 private:
  mojo::ReceiverSet<mojom::SpeechRecognition> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_SPEECH_RECOGNITION_ASH_H_
