// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognition_impl.h"

#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/pref_names.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/speech/on_device_speech_recognition_util.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "components/soda/soda_util.h"

namespace {
const char kOnDeviceLanguagesDownloadedKey[] = "ondevice-languages-downloaded";
const char kEnglishLanguageCodeKey[] = "en-US";

// Returns a boolean indicating whether the language is enabled.
bool IsLanguageInstallable(const std::string& language_code,
                           bool is_soda_binary_installed) {
  return base::Contains(
      speech::SodaInstaller::GetInstance()->GetLiveCaptionEnabledLanguages(),
      language_code);
}

bool IsLanguageInstalled(const std::string& language_code) {
  for (const auto& language : g_browser_process->local_state()->GetList(
           prefs::kSodaRegisteredLanguagePacks)) {
    if (language.GetString() == language_code) {
      return true;
    }
  }

  return false;
}

}  // namespace
#endif  // !BUILDFLAG(IS_ANDROID)

namespace speech {

OnDeviceSpeechRecognitionImpl::~OnDeviceSpeechRecognitionImpl() {
#if !BUILDFLAG(IS_ANDROID)
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  // `soda_installer` is not guaranteed to be valid, since it's possible for
  // this class to out-live it. This means that this class cannot use
  // ScopedObservation and needs to manage removing the observer itself.
  if (soda_installer) {
    soda_installer->RemoveObserver(this);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void OnDeviceSpeechRecognitionImpl::Bind(
    mojo::PendingReceiver<media::mojom::OnDeviceSpeechRecognition> receiver) {
  receiver_.Bind(std::move(receiver));
}

void OnDeviceSpeechRecognitionImpl::Available(
    const std::vector<std::string>& languages,
    OnDeviceSpeechRecognitionImpl::AvailableCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
#else
  if (!CanRenderFrameHostUseOnDeviceSpeechRecognition()) {
    std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
    return;
  }

  if (languages.empty()) {
    std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
    return;
  }

  media::mojom::AvailabilityStatus overall_status =
      media::mojom::AvailabilityStatus::kAvailable;
  for (const std::string& language : languages) {
    std::optional<speech::SodaLanguagePackComponentConfig> language_config =
        speech::GetLanguageComponentConfigMatchingLanguageSubtag(language);
    if (!language_config.has_value()) {
      std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
      return;
    }

    // According to the spec, the status returned by this API should be the
    // minimum status. I.e., the API returns:
    //   'available' if all languages are available
    //   'downloading' if all languages are either downloading or available
    //   'downloadable' if all languages are either available, downloading, or
    //   downloadable 'unavailable' in if one or more language is unavailable
    media::mojom::AvailabilityStatus status =
        GetMaskedAvailabilityStatus(language_config.value().language_name);
    if (status < overall_status) {
      overall_status = status;
    }
  }

  std::move(callback).Run(overall_status);
#endif  // BUILDFLAG(IS_ANDROID)
}

void OnDeviceSpeechRecognitionImpl::Install(
    const std::vector<std::string>& languages,
    OnDeviceSpeechRecognitionImpl::InstallCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(false);
#else
  if (!CanRenderFrameHostUseOnDeviceSpeechRecognition()) {
    std::move(callback).Run(false);
    return;
  }

  for (const std::string& language : languages) {
    std::optional<speech::SodaLanguagePackComponentConfig> language_config =
        speech::GetLanguageComponentConfigMatchingLanguageSubtag(language);

    if (!language_config.has_value() ||
        !IsLanguageInstallable(
            language_config.value().language_name,
            speech::SodaInstaller::GetInstance()->IsSodaBinaryInstalled())) {
      std::move(callback).Run(false);
      return;
    }
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceSpeechRecognitionImpl::InstallLanguageInternal,
                     weak_ptr_factory_.GetWeakPtr(), languages,
                     std::move(callback)),
      GetDownloadDelay(languages));
#endif  // BUILDFLAG(IS_ANDROID)
}

#if !BUILDFLAG(IS_ANDROID)
void OnDeviceSpeechRecognitionImpl::OnSodaInstalled(
    speech::LanguageCode language_code) {
  ProcessLanguageInstallationUpdate(GetLanguageName(language_code),
                                    /*installation_success=*/true);
}

void OnDeviceSpeechRecognitionImpl::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  ProcessLanguageInstallationUpdate(GetLanguageName(language_code),
                                    /*installation_success=*/false);
}
#endif  // !BUILDFLAG(IS_ANDROID)

OnDeviceSpeechRecognitionImpl::OnDeviceSpeechRecognitionImpl(
    content::RenderFrameHost* frame_host)
    : content::DocumentUserData<OnDeviceSpeechRecognitionImpl>(frame_host) {
#if !BUILDFLAG(IS_ANDROID)
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (soda_installer) {
    soda_installer->AddObserver(this);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool OnDeviceSpeechRecognitionImpl::
    CanRenderFrameHostUseOnDeviceSpeechRecognition() {
  if (render_frame_host().GetStoragePartition() !=
      render_frame_host().GetBrowserContext()->GetDefaultStoragePartition()) {
    return !render_frame_host().GetLastCommittedURL().SchemeIsHTTPOrHTTPS();
  }

  return true;
}

#if !BUILDFLAG(IS_ANDROID)
void OnDeviceSpeechRecognitionImpl::InstallLanguageInternal(
    const std::vector<std::string>& languages,
    OnDeviceSpeechRecognitionImpl::InstallCallback callback) {
  std::set<std::string> language_names_key;
  for (const std::string& subtag : languages) {
    std::optional<speech::SodaLanguagePackComponentConfig> lang_config =
        speech::GetLanguageComponentConfigMatchingLanguageSubtag(subtag);
    if (lang_config.has_value()) {
      language_names_key.insert(lang_config.value().language_name);
    }
  }

  if (language_names_key.empty()) {
    std::move(callback).Run(false);
    return;
  }

  if (base::FeatureList::IsEnabled(media::kOnDeviceWebSpeechGeminiNano)) {
    OptimizationGuideKeyedService* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(
                render_frame_host().GetBrowserContext()));
    if (!optimization_guide_keyed_service) {
      std::move(callback).Run(false);
      return;
    }

    language_installation_callbacks_[language_names_key].push_back(
        std::move(callback));

    model_broker_client_ =
        optimization_guide_keyed_service->CreateModelBrokerClient();

    // Call `GetSubscriber()` to trigger the download and installation of
    // the model.
    // TODO(crbug.com/446260680): Use
    // OnDeviceFeature::kOnDeviceSpeechRecognition.
    model_broker_client_
        ->GetSubscriber(optimization_guide::mojom::OnDeviceFeature::kPromptApi)
        .WaitForClient(base::BindOnce(
            &OnDeviceSpeechRecognitionImpl::OnModelClientAvailable,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    language_installation_callbacks_[language_names_key].push_back(
        std::move(callback));

    // `InstallSoda` will only install the SODA binary if it is not already
    // installed.
    speech::SodaInstaller::GetInstance()->InstallSoda(
        g_browser_process->local_state());

    // `InstallLanguage` will only install languages that are not already
    // installed.
    for (const std::string& language : language_names_key) {
      speech::SodaInstaller::GetInstance()->InstallLanguage(
          language, g_browser_process->local_state());
    }
  }

  for (const std::string& language : language_names_key) {
    SetOnDeviceLanguageDownloaded(language);
  }
}

void OnDeviceSpeechRecognitionImpl::ProcessLanguageInstallationUpdate(
    const std::string& language,
    bool installation_success) {
  for (auto it = language_installation_callbacks_.begin();
       it != language_installation_callbacks_.end();) {
    std::set<std::string> pending_languages_key = it->first;

    if (pending_languages_key.count(language)) {
      // This callback group was waiting for the processed `language`.
      std::list<InstallCallback> moved_callbacks = std::move(it->second);
      it = language_installation_callbacks_.erase(it);

      if (!installation_success) {
        // Installation failed for this language; fail all callbacks in this
        // group.
        for (auto& callback : moved_callbacks) {
          std::move(callback).Run(false);
        }
      } else {
        // Installation succeeded for this language.
        // Remove it from the pending set for this group.
        std::set<std::string> updated_key = pending_languages_key;
        updated_key.erase(language);

        if (updated_key.empty()) {
          // All languages for this group are now installed.
          for (auto& callback : moved_callbacks) {
            std::move(callback).Run(true);
          }
        } else {
          // Still waiting for other languages in this group.
          // Re-insert with the updated key, merging if the key now matches an
          // existing one.
          auto [inserted_it, success] =
              language_installation_callbacks_.emplace(
                  std::move(updated_key), std::list<InstallCallback>());
          inserted_it->second.splice(inserted_it->second.end(),
                                     moved_callbacks);
        }
      }
    } else {
      // This group of callbacks was not waiting for the current `language`.
      ++it;
    }
  }
}

base::Value
OnDeviceSpeechRecognitionImpl::GetOnDeviceLanguagesDownloadedValue() {
  GURL url = render_frame_host().GetLastCommittedOrigin().GetURL();
  return HostContentSettingsMapFactory::GetForProfile(
             render_frame_host().GetBrowserContext())
      ->GetWebsiteSetting(url, url,
                          ContentSettingsType::
                              ON_DEVICE_SPEECH_RECOGNITION_LANGUAGES_DOWNLOADED,
                          /*info=*/nullptr);
}

void OnDeviceSpeechRecognitionImpl::
    SetOnDeviceLanguagesDownloadedContentSetting(
        base::Value on_device_languages_downloaded) {
  GURL url = render_frame_host().GetLastCommittedOrigin().GetURL();
  HostContentSettingsMapFactory::GetForProfile(
      render_frame_host().GetBrowserContext())
      ->SetWebsiteSettingDefaultScope(
          url, url,
          ContentSettingsType::
              ON_DEVICE_SPEECH_RECOGNITION_LANGUAGES_DOWNLOADED,
          std::move(on_device_languages_downloaded));
}

media::mojom::AvailabilityStatus
OnDeviceSpeechRecognitionImpl::GetMaskedAvailabilityStatus(
    const std::string& language) {
  media::mojom::AvailabilityStatus availability_status =
      GetOnDeviceSpeechRecognitionAvailabilityStatus(
          render_frame_host().GetBrowserContext(), language);
  if (availability_status == media::mojom::AvailabilityStatus::kAvailable &&
      !HasOnDeviceLanguageDownloaded(language)) {
    return media::mojom::AvailabilityStatus::kDownloadable;
  }

  return availability_status;
}

bool OnDeviceSpeechRecognitionImpl::HasOnDeviceLanguageDownloaded(
    const std::string& language) {
  const GURL url = render_frame_host().GetLastCommittedOrigin().GetURL();
  if (!url.is_valid() || url.SchemeIsFile()) {
    return transient_on_device_languages_downloaded_.contains(language);
  }

  base::Value on_device_languages_downloaded_value =
      GetOnDeviceLanguagesDownloadedValue();
  if (on_device_languages_downloaded_value.is_dict()) {
    return on_device_languages_downloaded_value.GetDict()
        .EnsureList(kOnDeviceLanguagesDownloadedKey)
        ->contains(language);
  }

  return false;
}

void OnDeviceSpeechRecognitionImpl::SetOnDeviceLanguageDownloaded(
    const std::string& language) {
  const GURL url = render_frame_host().GetLastCommittedOrigin().GetURL();
  if (!url.is_valid() || url.SchemeIsFile()) {
    transient_on_device_languages_downloaded_.insert(language);
    return;
  }

  base::Value on_device_languages_downloaded_value =
      GetOnDeviceLanguagesDownloadedValue();

  // Initialize a list to store data, if none exists.
  if (!on_device_languages_downloaded_value.is_dict()) {
    on_device_languages_downloaded_value = base::Value(base::Value::Dict());
  }

  // Update or initialize the list of targets for the source language.
  base::Value::List* on_device_languages_downloaded_list =
      on_device_languages_downloaded_value.GetDict().EnsureList(
          kOnDeviceLanguagesDownloadedKey);
  if (!on_device_languages_downloaded_list->contains(language)) {
    on_device_languages_downloaded_list->Append(language);
  }

  SetOnDeviceLanguagesDownloadedContentSetting(
      std::move(on_device_languages_downloaded_value));
}

base::TimeDelta OnDeviceSpeechRecognitionImpl::GetDownloadDelay(
    const std::vector<std::string>& languages) {
  for (const std::string& language_subtag : languages) {
    std::optional<speech::SodaLanguagePackComponentConfig> lang_config =
        speech::GetLanguageComponentConfigMatchingLanguageSubtag(
            language_subtag);
    if (!lang_config.has_value()) {
      // If the subtag is invalid or doesn't map to a SODA language,
      // skip it for delay calculation.
      continue;
    }
    const std::string& soda_language_name = lang_config.value().language_name;

    // Check if SODA is already installed for the given language. If it is and
    // the origin isn't supposed to know that, then add a delay to simulate a
    // real download before proceeding.
    if (GetMaskedAvailabilityStatus(soda_language_name) ==
            media::mojom::AvailabilityStatus::kDownloadable &&
        IsLanguageInstalled(soda_language_name)) {
      return base::RandTimeDelta(base::Seconds(2), base::Seconds(3));
    }
  }

  return base::TimeDelta();
}

void OnDeviceSpeechRecognitionImpl::OnModelClientAvailable(
    base::WeakPtr<optimization_guide::ModelClient> client) {
  ProcessLanguageInstallationUpdate(kEnglishLanguageCodeKey, bool(client));
}
#endif  // !BUILDFLAG(IS_ANDROID)

DOCUMENT_USER_DATA_KEY_IMPL(OnDeviceSpeechRecognitionImpl);

}  // namespace speech
