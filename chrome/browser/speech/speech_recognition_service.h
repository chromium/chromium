// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace speech {

// Provides a mojo endpoint in the browser that allows the renderer process to
// launch and initialize the sandboxed speech recognition service
// process.
class SpeechRecognitionService
    : public KeyedService,
      public media::mojom::SpeechRecognitionServiceClient {
 public:
  explicit SpeechRecognitionService(content::BrowserContext* context);
  SpeechRecognitionService(const SpeechRecognitionService&) = delete;
  SpeechRecognitionService& operator=(const SpeechRecognitionService&) = delete;
  ~SpeechRecognitionService() override;

  void Create(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver);

  // media::mojom::SpeechRecognitionServiceClient
  void OnNetworkServiceDisconnect() override;

 private:
  // Launches the speech recognition service in a sandboxed utility process.
  void LaunchIfNotRunning();

  // Gets the path of the SODA configuration file for the selected language.
  base::FilePath GetSodaConfigPath(PrefService* prefs);

  // The browser context associated with the keyed service.
  content::BrowserContext* const context_;

  // A flag indicating whether to use the Speech On-Device API (SODA) for speech
  // recognition.
  bool enable_soda_ = false;

  // The remote to the speech recognition service. The browser will not launch a
  // new speech recognition service process if this remote is already bound.
  mojo::Remote<media::mojom::SpeechRecognitionService>
      speech_recognition_service_;

  mojo::Receiver<media::mojom::SpeechRecognitionServiceClient>
      speech_recognition_service_client_{this};
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_H_
