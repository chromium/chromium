// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_SERVICE_H_
#define CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_SERVICE_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/speech/speech_recognition_service.h"
#include "chrome/browser/speech/speech_recognition_small_expert_model_installer.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/soda/soda_installer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace speech {

// Provides a mojo endpoint in the browser that allows the renderer process to
// launch and initialize the sandboxed speech recognition service
// process.
class ChromeSpeechRecognitionService
    : public SpeechRecognitionService,
      public media::mojom::SpeechRecognitionContext,
      public SodaInstaller::Observer,
      public SpeechRecognitionSmallExpertModelInstaller::Observer {
 public:
  explicit ChromeSpeechRecognitionService(content::BrowserContext* context);
  ChromeSpeechRecognitionService(const ChromeSpeechRecognitionService&) =
      delete;
  ChromeSpeechRecognitionService& operator=(
      const ChromeSpeechRecognitionService&) = delete;
  ~ChromeSpeechRecognitionService() override;

  // SpeechRecognitionService:
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver)
      override;
  void BindAudioSourceSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
          receiver) override;

  // media::mojom::SpeechRecognitionContext:
  void BindRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback) override;
  void BindWebSpeechRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
          session_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
          session_client,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
          audio_forwarder,
      int channel_count,
      int sample_rate,
      media::mojom::SpeechRecognitionOptionsPtr options,
      bool continuous) override;

  // Closes active recognition sessions and OnDeviceModelService handles.
  void CloseOnDeviceModelServiceHandles();

  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override;

  // SpeechRecognitionSmallExpertModelInstaller::Observer:
  void OnSpeechRecognitionSmallExpertModelInstalled() override;
  void OnSpeechRecognitionSmallExpertModelStateChanged(
      speech::SpeechRecognitionSmallExpertModelInstaller::
          SpeechRecognitionSmallExpertModelState state) override;

 protected:
  content::BrowserContext* context() { return context_; }

  // Receivers for SpeechRecognitionContext.
  mojo::ReceiverSet<media::mojom::SpeechRecognitionContext>
      speech_recognition_contexts_;

 private:
  friend class ChromeSpeechRecognitionServiceTest;

  void OnModelClientAvailable(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback,
      base::WeakPtr<optimization_guide::ModelClient> model_client);

  void BindSodaRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback);

  // Launches the speech recognition service in a sandboxed utility process.
  void LaunchIfNotRunning();

  // Gets the path of the SODA configuration file for the selected language.
  base::flat_map<std::string, base::FilePath> GetSodaConfigPaths();

  // The browser context associated with the keyed service.
  raw_ptr<content::BrowserContext> context_;

  // The remote to the speech recognition service. The browser will not launch a
  // new speech recognition service process if this remote is already bound.
  mojo::Remote<media::mojom::SpeechRecognitionService>
      speech_recognition_service_;

  // The remote to the speech recognition context in the service process.
  mojo::Remote<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_remote_;

  // ModelBrokerClient and recognizers for on-device model.
  std::unique_ptr<optimization_guide::ModelBrokerClient> model_broker_client_;
  mojo::UniqueReceiverSet<media::mojom::SpeechRecognitionRecognizer>
      on_device_recognizers_;

  base::ScopedObservation<SodaInstaller, SodaInstaller::Observer>
      soda_installer_observer_{this};

  base::ScopedObservation<SpeechRecognitionSmallExpertModelInstaller,
                          SpeechRecognitionSmallExpertModelInstaller::Observer>
      speech_recognition_small_expert_model_installer_observer_{this};

  base::WeakPtrFactory<ChromeSpeechRecognitionService> weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_CHROME_SPEECH_RECOGNITION_SERVICE_H_
