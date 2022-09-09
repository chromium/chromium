// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/cros_speech_recognition_service.h"

#include "base/files/file_path.h"
#include "base/types/optional_util.h"
#include "chrome/services/speech/audio_source_fetcher_impl.h"
#include "chrome/services/speech/cros_speech_recognition_recognizer_impl.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"

namespace speech {

namespace {

void PopulateFilePaths(const std::string* language,
                       base::FilePath& binary_path,
                       base::FilePath& languagepack_path) {
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  // TODO(crbug.com/1161569): Language should not be optional in
  // PopulateFilePaths, as it will be required once we support multiple
  // languages since the CrosSpeechRecognitionService supports several
  // features at once. For now only US English is available.
  LanguageCode language_code =
      language ? GetLanguageCode(*language) : LanguageCode::kEnUs;
  if (!soda_installer->IsSodaInstalled(language_code)) {
    LOG(DFATAL) << "Instantiation of SODA requested with language "
                << GetLanguageName(language_code)
                << ", but either SODA or the requested language was not "
                   "already installed";
    return;
  }
  binary_path = soda_installer->GetSodaBinaryPath();
  languagepack_path =
      soda_installer->GetLanguagePath(GetLanguageName(language_code));
}

}  // namespace

CrosSpeechRecognitionService::CrosSpeechRecognitionService(
    content::BrowserContext* context)
    : ChromeSpeechRecognitionService(context) {}

CrosSpeechRecognitionService::~CrosSpeechRecognitionService() {}

void CrosSpeechRecognitionService::BindSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  speech_recognition_contexts_.Add(this, std::move(receiver));
}

void CrosSpeechRecognitionService::BindAudioSourceSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::AudioSourceSpeechRecognitionContext>
        receiver) {
  audio_source_speech_recognition_contexts_.Add(this, std::move(receiver));
}

void CrosSpeechRecognitionService::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  base::FilePath binary_path, languagepack_path;
  PopulateFilePaths(base::OptionalToPtr(options->language), binary_path,
                    languagepack_path);

  CrosSpeechRecognitionRecognizerImpl::Create(
      std::move(receiver), std::move(client),
      nullptr /* =SpeechRecognitionService WeakPtr*/, std::move(options),
      binary_path, languagepack_path);
  std::move(callback).Run(
      CrosSpeechRecognitionRecognizerImpl::IsMultichannelSupported());
}

void CrosSpeechRecognitionService::BindAudioSourceFetcher(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  base::FilePath binary_path, languagepack_path;
  PopulateFilePaths(base::OptionalToPtr(options->language), binary_path,
                    languagepack_path);

  // CrosSpeechRecognitionService runs on browser UI thread.
  // Create AudioSourceFetcher on browser IO thread to avoid UI jank.
  // Note that its CrosSpeechRecognitionRecognizer must also run
  // on the IO thread. If CrosSpeechRecognitionService is moved away from
  // browser UI thread, we can call AudioSourceFetcherImpl::Create directly.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CrosSpeechRecognitionService::CreateAudioSourceFetcherOnIOThread,
          weak_factory_.GetWeakPtr(), std::move(fetcher_receiver),
          std::move(client), std::move(options), binary_path,
          languagepack_path));
  std::move(callback).Run(
      CrosSpeechRecognitionRecognizerImpl::IsMultichannelSupported());
}

void CrosSpeechRecognitionService::CreateAudioSourceFetcherOnIOThread(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    const base::FilePath& binary_path,
    const base::FilePath& languagepack_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  AudioSourceFetcherImpl::Create(
      std::move(fetcher_receiver),
      std::make_unique<CrosSpeechRecognitionRecognizerImpl>(
          std::move(client), nullptr /* =SpeechRecognitionService WeakPtr*/,
          std::move(options), binary_path, languagepack_path));
}

}  // namespace speech
