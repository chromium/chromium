// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_CROS_SPEECH_RECOGNITION_SERVICE_H_
#define CHROME_BROWSER_SPEECH_CROS_SPEECH_RECOGNITION_SERVICE_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/speech/chrome_speech_recognition_service.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace network {
class PendingSharedURLLoaderFactory;
}  // namespace network

namespace speech {

// Provides a Mojo endpoint in the browser for the CROS system. This uses ML
// Service, so is actually executing a little more in the
// browser then regular chrome.
class CrosSpeechRecognitionService
    : public ChromeSpeechRecognitionService,
      public media::mojom::AudioSourceSpeechRecognitionContext,
      public media::mojom::SpeechRecognitionContext {
 public:
  explicit CrosSpeechRecognitionService(content::BrowserContext* context);
  CrosSpeechRecognitionService(const CrosSpeechRecognitionService&) = delete;
  CrosSpeechRecognitionService& operator=(const SpeechRecognitionService&) =
      delete;
  ~CrosSpeechRecognitionService() override;

  // SpeechRecognitionService:
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver)
      override;
  void BindAudioSourceSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
          receiver) override;

  // media::mojom::SpeechRecognitionContext
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

  // media::mojom::AudioSourceSpeechRecognitionContext:
  void BindAudioSourceFetcher(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback) override;

 private:
  void CreateAudioSourceFetcherForOnDeviceRecognitionOnIOThread(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      const base::FilePath& binary_path,
      const base::flat_map<std::string, base::FilePath>& config_paths,
      const std::string& primary_language_name,
      const bool mask_offensive_words);

  void CreateAudioSourceFetcherForServerBasedRecognitionOnIOThread(
      mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_loader_factory);

  mojo::ReceiverSet<media::mojom::AudioSourceSpeechRecognitionContext>
      audio_source_speech_recognition_contexts_;
  mojo::ReceiverSet<media::mojom::SpeechRecognitionContext>
      speech_recognition_contexts_;
  base::WeakPtrFactory<CrosSpeechRecognitionService> weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_CROS_SPEECH_RECOGNITION_SERVICE_H_
