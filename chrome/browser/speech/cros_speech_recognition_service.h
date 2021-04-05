// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CROS_SPEECH_RECOGNITION_SERVICE_H_
#define CHROME_BROWSER_SPEECH_CROS_SPEECH_RECOGNITION_SERVICE_H_

#include "chrome/browser/speech/chrome_speech_recognition_service.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace speech {

// Provides a Mojo endpoint in the browser for the CROS system. This uses ML
// Service, so is actually executing a little more in the
// browser then regular chrome.
class CrosSpeechRecognitionService
    : public ChromeSpeechRecognitionService,
      public media::mojom::SpeechRecognitionContext {
 public:
  explicit CrosSpeechRecognitionService(content::BrowserContext* context);
  CrosSpeechRecognitionService(const CrosSpeechRecognitionService&) = delete;
  CrosSpeechRecognitionService& operator=(const SpeechRecognitionService&) =
      delete;
  ~CrosSpeechRecognitionService() override;
  void Create(mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
                  receiver) override;

  // media::mojom::SpeechRecognitionContext
  void BindRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      BindRecognizerCallback callback) override;
  void BindAudioSourceFetcher(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      BindRecognizerCallback callback) override;

 private:
  void PopulateFilePaths(base::FilePath& binary_path,
                         base::FilePath& languagepack_path);
  mojo::ReceiverSet<media::mojom::SpeechRecognitionContext>
      speech_recognition_contexts_;
  const bool enable_soda_;
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_CROS_SPEECH_RECOGNITION_SERVICE_H_
