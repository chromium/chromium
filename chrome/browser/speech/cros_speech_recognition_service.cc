// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/cros_speech_recognition_service.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/notimplemented.h"
#include "base/types/optional_util.h"
#include "chrome/services/speech/audio_source_fetcher_impl.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "chrome/services/speech/cros_speech_recognition_recognizer_impl.h"
#include "components/live_caption/pref_names.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(ENABLE_SERVER_BASED_RECOGNITION_RECOGNIZER)
#include "chrome/services/speech/internal/server_based_recognition_recognizer.h"
#endif  // BUILDFLAG(ENABLE_SERVER_BASED_RECOGNITION_RECOGNIZER)

namespace speech {

namespace {

constexpr char kInvalidSpeechRecogntionOptions[] =
    "Invalid SpeechRecognitionOptions provided";

void PopulateFilePaths(
    base::FilePath& binary_path,
    base::flat_map<std::string, base::FilePath>& config_paths) {
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();

  binary_path = soda_installer->GetSodaBinaryPath();
  for (const auto& language_code : soda_installer->InstalledLanguages()) {
    config_paths[GetLanguageName(language_code)] =
        soda_installer->GetLanguagePath(GetLanguageName(language_code));
  }
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
  // This binding is used by LiveCaption and it can't be server based
  // recognition.
  if (options->is_server_based ||
      options->recognizer_client_type !=
          media::mojom::RecognizerClientType::kLiveCaption) {
    mojo::ReportBadMessage(kInvalidSpeechRecogntionOptions);
    return;
  }

  base::FilePath binary_path;
  base::flat_map<std::string, base::FilePath> config_paths;
  // The options should have locale set, but if they don't, pull from the prefs.
  std::string language_name;
  if (options->language) {
    language_name = options->language.value();
  } else {
    PrefService* profile_prefs = user_prefs::UserPrefs::Get(context());
    language_name = prefs::GetLiveCaptionLanguageCode(profile_prefs);
  }

  PopulateFilePaths(binary_path, config_paths);

  // TODO(crbug.com/40924425): Implement offensive word mask on ChromeOS so that
  // mask_offensive_words is not hard-coded.
  CrosSpeechRecognitionRecognizerImpl::Create(
      std::move(receiver), std::move(client), std::move(options), binary_path,
      config_paths, language_name, /* mask_offensive_words= */ false);
  std::move(callback).Run(
      CrosSpeechRecognitionRecognizerImpl::IsMultichannelSupported());
}

void CrosSpeechRecognitionService::BindWebSpeechRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
        session_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
        session_client,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
        audio_forwarder,
    int channel_count,
    int sample_rate,
    media::mojom::SpeechRecognitionOptionsPtr options,
    bool continuous) {
  NOTIMPLEMENTED();
}

void CrosSpeechRecognitionService::BindAudioSourceFetcher(
    mojo::PendingReceiver<media::mojom::AudioSourceFetcher> fetcher_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    BindRecognizerCallback callback) {
  if (!options->is_server_based) {
    base::FilePath binary_path;
    base::flat_map<std::string, base::FilePath> config_paths;
    PopulateFilePaths(binary_path, config_paths);

    // The options should have locale set, but if they don't, pull from the
    // prefs.
    std::string language_name;
    if (options->language) {
      language_name = options->language.value();
    } else {
      PrefService* profile_prefs = user_prefs::UserPrefs::Get(context());
      language_name = prefs::GetLiveCaptionLanguageCode(profile_prefs);
    }
    // CrosSpeechRecognitionService runs on browser UI thread.
    // Create AudioSourceFetcher on browser IO thread to avoid UI jank.
    // Note that its CrosSpeechRecognitionRecognizer must also run
    // on the IO thread. If CrosSpeechRecognitionService is moved away from
    // browser UI thread, we can call AudioSourceFetcherImpl::Create directly.
    // TODO: Implement offensive word mask on ChromeOS so that
    // mask_offensive_words is not hard-coded.
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CrosSpeechRecognitionService::
                CreateAudioSourceFetcherForOnDeviceRecognitionOnIOThread,
            weak_factory_.GetWeakPtr(), std::move(fetcher_receiver),
            std::move(client), std::move(options), binary_path, config_paths,
            language_name, /* mask_offensive_words= */ false));
    std::move(callback).Run(
        CrosSpeechRecognitionRecognizerImpl::IsMultichannelSupported());
    return;
  }
#if BUILDFLAG(ENABLE_SERVER_BASED_RECOGNITION_RECOGNIZER)
  if (!ash::features::IsInternalServerSideSpeechRecognitionEnabled()) {
    // A request is made for a service that has not been enabled.
    mojo::ReportBadMessage(kInvalidSpeechRecogntionOptions);
    return;
  }
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CrosSpeechRecognitionService::
              CreateAudioSourceFetcherForServerBasedRecognitionOnIOThread,
          weak_factory_.GetWeakPtr(), std::move(fetcher_receiver),
          std::move(client), std::move(options),
          context()
              ->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcessIOThread()));
  std::move(callback).Run(/*is_multichannel_supported=*/false);
  return;
#else
  mojo::ReportBadMessage(kInvalidSpeechRecogntionOptions);
#endif  // BUILDFLAG(ENABLE_SERVER_BASED_RECOGNITION_RECOGNIZER)
}

void CrosSpeechRecognitionService::
    CreateAudioSourceFetcherForOnDeviceRecognitionOnIOThread(
        mojo::PendingReceiver<media::mojom::AudioSourceFetcher>
            fetcher_receiver,
        mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
            client,
        media::mojom::SpeechRecognitionOptionsPtr options,
        const base::FilePath& binary_path,
        const base::flat_map<std::string, base::FilePath>& config_paths,
        const std::string& primary_language_name,
        const bool mask_offensive_words) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!options->is_server_based);
  AudioSourceFetcherImpl::Create(
      std::move(fetcher_receiver),
      std::make_unique<CrosSpeechRecognitionRecognizerImpl>(
          std::move(client), std::move(options), binary_path, config_paths,
          primary_language_name, mask_offensive_words),
      CrosSpeechRecognitionRecognizerImpl::IsMultichannelSupported(),
      /*is_server_based=*/false);
}

void CrosSpeechRecognitionService::
    CreateAudioSourceFetcherForServerBasedRecognitionOnIOThread(
        mojo::PendingReceiver<media::mojom::AudioSourceFetcher>
            fetcher_receiver,
        mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
            client,
        media::mojom::SpeechRecognitionOptionsPtr options,
        std::unique_ptr<network::PendingSharedURLLoaderFactory>
            pending_loader_factory) {
#if BUILDFLAG(ENABLE_SERVER_BASED_RECOGNITION_RECOGNIZER)
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(options->is_server_based);
  AudioSourceFetcherImpl::Create(
      std::move(fetcher_receiver),
      std::make_unique<ServerBasedRecognitionRecognizer>(
          std::move(client), std::move(options),
          network::SharedURLLoaderFactory::Create(
              std::move(pending_loader_factory))),
      /*is_multichannel_supported=*/false, /*is_server_based=*/true);
#endif  // BUILDFLAG(ENABLE_SERVER_BASED_RECOGNITION_RECOGNIZER)
}

}  // namespace speech
