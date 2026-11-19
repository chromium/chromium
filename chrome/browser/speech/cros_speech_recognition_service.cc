// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/cros_speech_recognition_service.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/optional_util.h"
#include "chrome/services/speech/audio_source_fetcher_impl.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "chrome/services/speech/cros_speech_recognition_recognizer_impl.h"
#include "components/live_caption/pref_names.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "components/soda/soda_util.h"
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
#include "chrome/services/speech/internal/server_based_recognition_recognizer.h"  // nogncheck crbug.com/40147906
#endif  // BUILDFLAG(ENABLE_SERVER_BASED_RECOGNITION_RECOGNIZER)

namespace speech {

namespace {

constexpr char kInvalidSpeechRecogntionOptions[] =
    "Invalid SpeechRecognitionOptions provided";

std::string_view PrimaryLanguageSubtag(std::string_view locale) {
  if (const auto split = base::SplitStringOnce(locale, '-')) {
    return split->first;
  }
  return locale;
}

void PopulateFilePaths(
    base::FilePath& binary_path,
    base::flat_map<std::string, base::FilePath>& config_paths) {
  if (!speech::IsOnDeviceSpeechRecognitionSupported()) {
    return;
  }
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (!soda_installer) {
    return;
  }

  binary_path = soda_installer->GetSodaBinaryPath();
  bool has_unmapped_installed_language = false;
  for (const auto& language_code : soda_installer->InstalledLanguages()) {
    std::string language_name = GetLanguageName(language_code);
    if (language_name.empty()) {
      has_unmapped_installed_language = true;
      continue;
    }
    base::FilePath language_path =
        soda_installer->GetLanguagePath(language_name);
    if (!language_path.empty()) {
      config_paths[language_name] = std::move(language_path);
    }
  }
  if (has_unmapped_installed_language) {
    // Regional ChromeOS DLCs (such as `en-GB` or `es-US`) are absent from
    // desktop `kLanguageComponentConfigs`, so `GetLanguageName()` returns an
    // empty string for their `LanguageCode`. Query `GetAvailableLanguages()`
    // and record any regional pack with a non-empty installed path.
    for (const std::string& language_name :
         soda_installer->GetAvailableLanguages()) {
      if (config_paths.contains(language_name)) {
        continue;
      }
      base::FilePath language_path =
          soda_installer->GetLanguagePath(language_name);
      if (!language_path.empty()) {
        config_paths[language_name] = std::move(language_path);
      }
    }
  }
}

std::unique_ptr<CrosSpeechRecognitionRecognizerImpl>
CreateCrosSpeechRecognitionRecognizer(
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    media::mojom::SpeechRecognitionOptionsPtr options,
    const base::FilePath& binary_path,
    const base::flat_map<std::string, base::FilePath>& config_paths,
    const std::string& primary_language_name,
    const bool mask_offensive_words) {
  return std::make_unique<CrosSpeechRecognitionRecognizerImpl>(
      std::move(client), std::move(options), binary_path, config_paths,
      primary_language_name, mask_offensive_words);
}

}  // namespace

CrosSpeechRecognitionService::CrosSpeechRecognitionService(
    content::BrowserContext* context)
    : ChromeSpeechRecognitionService(context) {
  cros_speech_recognition_recognizer_cb_ =
      base::BindRepeating(CreateCrosSpeechRecognitionRecognizer);
}

CrosSpeechRecognitionService::~CrosSpeechRecognitionService() = default;

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
  // This binding is used by LiveCaption and TTC, and it can't be server based
  // recognition.
  if (options->is_server_based ||
      (options->recognizer_client_type !=
           media::mojom::RecognizerClientType::kLiveCaption &&
       options->recognizer_client_type !=
           media::mojom::RecognizerClientType::kTtc)) {
    // Report through the receiver set so it closes the offending receiver
    // before `callback` is dropped. A dropped reply callback on an open pipe
    // triggers a DCHECK.
    speech_recognition_contexts_.ReportBadMessage(
        kInvalidSpeechRecogntionOptions);
    return;
  }

  base::FilePath binary_path;
  base::flat_map<std::string, base::FilePath> config_paths;
  PopulateFilePaths(binary_path, config_paths);
  if (binary_path.empty() || config_paths.empty()) {
    std::move(callback).Run(/*is_multichannel_supported=*/false);
    return;
  }

  // The options should have locale set, but if they don't, pull from the prefs.
  std::string language_name;
  if (options->language) {
    language_name = options->language.value();
  } else {
    PrefService* profile_prefs = user_prefs::UserPrefs::Get(context());
    language_name = prefs::GetLiveCaptionLanguageCode(profile_prefs);
  }

  language_name = speech::MaybeMapToChineseLocale(language_name);

  if (options->recognizer_client_type ==
          media::mojom::RecognizerClientType::kTtc &&
      !config_paths.contains(language_name)) {
    if (std::optional<speech::SodaLanguagePackComponentConfig> config =
            speech::GetLanguageComponentConfigMatchingLanguageSubtag(
                language_name);
        config && config_paths.contains(config->language_name)) {
      language_name = std::string(config->language_name);
    } else {
      const std::string_view requested_subtag =
          PrimaryLanguageSubtag(language_name);
      if (!requested_subtag.empty() &&
          !base::EqualsCaseInsensitiveASCII(requested_subtag, "zh") &&
          !base::EqualsCaseInsensitiveASCII(requested_subtag, "cmn")) {
        for (const auto& [installed_locale, path] : config_paths) {
          if (!path.empty() &&
              base::EqualsCaseInsensitiveASCII(
                  PrimaryLanguageSubtag(installed_locale), requested_subtag)) {
            language_name = installed_locale;
            break;
          }
        }
      }
    }
  }
  auto config_it = config_paths.find(language_name);
  if (config_it == config_paths.end() || config_it->second.empty()) {
    std::move(callback).Run(/*is_multichannel_supported=*/false);
    return;
  }

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

    language_name = speech::MaybeMapToChineseLocale(language_name);

    // `mask_offensive_words` is always true for
    // `RecognizerClientType::kSchoolTools`.
    // TODO(crbug.com/40924425): Implement offensive word mask on ChromeOS for
    // live caption as well so that mask_offensive_words is set according to the
    //  settings for `RecognizerClientType::kLiveCaption`.
    bool mask_offensive_words =
        options->recognizer_client_type ==
        media::mojom::RecognizerClientType::kSchoolTools;
    // CrosSpeechRecognitionService runs on browser UI thread.
    // Create AudioSourceFetcher on browser IO thread to avoid UI jank.
    // Note that its CrosSpeechRecognitionRecognizer must also run
    // on the IO thread. If CrosSpeechRecognitionService is moved away from
    // browser UI thread, we can call AudioSourceFetcherImpl::Create directly.
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &CrosSpeechRecognitionService::
                CreateAudioSourceFetcherForOnDeviceRecognitionOnIOThread,
            weak_factory_.GetWeakPtr(), std::move(fetcher_receiver),
            std::move(client), std::move(options), binary_path, config_paths,
            language_name, mask_offensive_words));
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
    SetCreateCrosSpeechRecognitionRecognizerCbForTesting(
        CreateCrosSpeechRecognitionRecognizerCb callback) {
  cros_speech_recognition_recognizer_cb_ = std::move(callback);
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
  CHECK_CURRENTLY_ON(content::BrowserThread::IO, base::NotFatalUntil::M162);
  CHECK(!options->is_server_based, base::NotFatalUntil::M162);
  AudioSourceFetcherImpl::Create(
      std::move(fetcher_receiver),
      cros_speech_recognition_recognizer_cb_.Run(
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
  CHECK_CURRENTLY_ON(content::BrowserThread::IO, base::NotFatalUntil::M162);
  CHECK(options->is_server_based, base::NotFatalUntil::M162);
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
