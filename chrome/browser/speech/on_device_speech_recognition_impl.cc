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
#include "components/soda/soda_util.h"

namespace {
const char kOnDeviceLanguagesDownloadedKey[] = "ondevice-languages-downloaded";

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

void OnDeviceSpeechRecognitionImpl::OnDeviceWebSpeechAvailable(
    const std::string& language,
    OnDeviceSpeechRecognitionImpl::OnDeviceWebSpeechAvailableCallback
        callback) {
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
#else
  if (!CanRenderFrameHostUseOnDeviceSpeechRecognition()) {
    std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
    return;
  }

  std::optional<speech::SodaLanguagePackComponentConfig> language_config =
      speech::GetLanguageComponentConfigMatchingLanguageSubtag(language);
  if (!language_config.has_value()) {
    std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
    return;
  }

  std::move(callback).Run(
      GetMaskedAvailabilityStatus(language_config.value().language_name));
#endif  // BUILDFLAG(IS_ANDROID)
}

void OnDeviceSpeechRecognitionImpl::InstallOnDeviceSpeechRecognition(
    const std::string& language,
    OnDeviceSpeechRecognitionImpl::InstallOnDeviceSpeechRecognitionCallback
        callback) {
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(false);
}
#else
  std::optional<speech::SodaLanguagePackComponentConfig> language_config =
      speech::GetLanguageComponentConfigMatchingLanguageSubtag(language);

  if (!language_config.has_value() ||
      !IsLanguageInstallable(
          language_config.value().language_name,
          speech::SodaInstaller::GetInstance()->IsSodaBinaryInstalled())) {
    std::move(callback).Run(false);
    return;
  }

  if (!CanRenderFrameHostUseOnDeviceSpeechRecognition()) {
    std::move(callback).Run(false);
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&OnDeviceSpeechRecognitionImpl::InstallLanguageInternal,
                     weak_ptr_factory_.GetWeakPtr(),
                     language_config.value().language_name,
                     std::move(callback)),
      GetDownloadDelay(language_config.value().language_name));
}

void OnDeviceSpeechRecognitionImpl::OnSodaInstalled(
    speech::LanguageCode language_code) {
  RunAndRemoveInstallationCallbacks(GetLanguageName(language_code),
                                    /*installation_success=*/true);
}

void OnDeviceSpeechRecognitionImpl::OnSodaInstallError(
    speech::LanguageCode language_code,
    speech::SodaInstaller::ErrorCode error_code) {
  RunAndRemoveInstallationCallbacks(GetLanguageName(language_code),
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
    const std::string& language,
    OnDeviceSpeechRecognitionImpl::InstallOnDeviceSpeechRecognitionCallback
        callback) {
  language_installation_callbacks_[language].push_back(std::move(callback));

  // `InstallSoda` will only install the SODA binary if it is not already
  // installed.
  speech::SodaInstaller::GetInstance()->InstallSoda(
      g_browser_process->local_state());

  // `InstallLanguage` will only install languages that are not already
  // installed.
  speech::SodaInstaller::GetInstance()->InstallLanguage(
      language, g_browser_process->local_state());

  SetOnDeviceLanguageDownloaded(language);
}

void OnDeviceSpeechRecognitionImpl::RunAndRemoveInstallationCallbacks(
    const std::string& language,
    bool installation_success) {
  auto it = language_installation_callbacks_.find(language);
  if (it != language_installation_callbacks_.end()) {
    std::list<InstallOnDeviceSpeechRecognitionCallback>& callbacks = it->second;
    for (auto callback_iterator = callbacks.begin();
         callback_iterator != callbacks.end();) {
      std::move(*callback_iterator).Run(installation_success);
      callback_iterator = callbacks.erase(callback_iterator);
    }
    language_installation_callbacks_.erase(it);
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
      IsOnDeviceSpeechRecognitionAvailable(language);
  if (availability_status == media::mojom::AvailabilityStatus::kAvailable &&
      !HasOnDeviceLanguageDownloaded(language)) {
    return media::mojom::AvailabilityStatus::kDownloadable;
  }

  return availability_status;
}

bool OnDeviceSpeechRecognitionImpl::HasOnDeviceLanguageDownloaded(
    const std::string& language) {
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
    const std::string& language) {
  // Check if SODA is already installed for the given language. If it is and the
  // origin isn't supposed to know that, then add a delay to simulate a real
  // download before proceeding.
  if (GetMaskedAvailabilityStatus(language) ==
          media::mojom::AvailabilityStatus::kDownloadable &&
      IsLanguageInstalled(language)) {
    return base::RandTimeDelta(base::Seconds(2), base::Seconds(3));
  }

  return base::TimeDelta();
}
#endif  // !BUILDFLAG(IS_ANDROID)

DOCUMENT_USER_DATA_KEY_IMPL(OnDeviceSpeechRecognitionImpl);

}  // namespace speech
