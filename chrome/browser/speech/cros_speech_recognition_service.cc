// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/cros_speech_recognition_service.h"

#include "chrome/browser/accessibility/soda_installer.h"
#include "chrome/services/speech/audio_source_fetcher_impl.h"
#include "chrome/services/speech/cros_speech_recognition_recognizer_impl.h"
#include "components/soda/constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/media_switches.h"

namespace speech {

CrosSpeechRecognitionService::CrosSpeechRecognitionService(
    content::BrowserContext* context)
    : ChromeSpeechRecognitionService(context),
      enable_soda_(
          base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)) {}

CrosSpeechRecognitionService::~CrosSpeechRecognitionService() {}

void CrosSpeechRecognitionService::Create(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  if (enable_soda_) {
    speech_recognition_contexts_.Add(this, std::move(receiver));
  } else {
    // If soda is not enabled, do the same thing as chrome.
    ChromeSpeechRecognitionService::Create(std::move(receiver));
  }
}

void CrosSpeechRecognitionService::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    BindRecognizerCallback callback) {
  base::FilePath binary_path, languagepack_path;
  PopulateFilePaths(binary_path, languagepack_path);

  CrosSpeechRecognitionRecognizerImpl::Create(
      std::move(receiver), std::move(client),
      nullptr /* =SpeechRecognitionService WeakPtr*/, binary_path,
      languagepack_path);
  std::move(callback).Run(
      CrosSpeechRecognitionRecognizerImpl::IsMultichannelSupported());
}

void CrosSpeechRecognitionService::BindAudioSourceFetcher(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    BindRecognizerCallback callback) {
  base::FilePath binary_path, languagepack_path;
  PopulateFilePaths(binary_path, languagepack_path);

  // CrosSpeechRecognitionService runs on browser UI thread.
  // Create AudioSourceFetcher on browser IO thread to avoid UI jank.
  // Note that SpeechRecognitionRecognizer (used for network speech) also runs
  // on the IO thread. If CrosSpeechRecognitionService is moved away from
  // browser UI thread, we can call AudioSourceFetcherImpl::Create directly.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AudioSourceFetcherImpl::Create, std::move(fetcher_receiver),
          std::make_unique<CrosSpeechRecognitionRecognizerImpl>(
              std::move(client), nullptr /* =SpeechRecognitionService WeakPtr*/,
              binary_path, languagepack_path)));
  std::move(callback).Run(
      CrosSpeechRecognitionRecognizerImpl::IsMultichannelSupported());
}

void CrosSpeechRecognitionService::PopulateFilePaths(
    base::FilePath& binary_path,
    base::FilePath& languagepack_path) {
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (soda_installer->IsSodaInstalled()) {
    binary_path = soda_installer->GetSodaBinaryPath();
    languagepack_path = soda_installer->GetLanguagePath();
  } else {
    LOG(DFATAL)
        << "Instantiation of SODA requested without SODA being installed.";
  }
}

}  // namespace speech
