// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognition_impl.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/pref_names.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/site_instance.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/barrier_callback.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/speech/on_device_speech_recognition_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"  // nogncheck crbug.com/40147906
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-shared.h"
#include "components/soda/soda_util.h"

namespace {
const char kOnDeviceLanguagesDownloadedKey[] = "ondevice-languages-downloaded";

int GetPriority(media::mojom::AvailabilityStatus status) {
  switch (status) {
    case media::mojom::AvailabilityStatus::kUnavailable:
      return 0;
    case media::mojom::AvailabilityStatus::kDownloadable:
      return 1;
    case media::mojom::AvailabilityStatus::kDownloadableWithoutUserActivation:
      return 2;
    case media::mojom::AvailabilityStatus::kDownloading:
      return 3;
    case media::mojom::AvailabilityStatus::kAvailable:
      return 4;
  }
  NOTREACHED();
}

// Returns a boolean indicating whether the language is enabled.
bool IsLanguageInstallable(std::string_view language_code,
                           bool is_soda_binary_installed) {
  return std::ranges::contains(
      speech::SodaInstaller::GetInstance()->GetLiveCaptionEnabledLanguages(),
      language_code);
}

std::vector<std::string_view> GetAcceptLanguagesList(
    PrefService* profile_prefs,
    std::string& accept_languages_out) {
  if (!profile_prefs) {
    return {};
  }
  accept_languages_out =
      profile_prefs->GetString(language::prefs::kAcceptLanguages);
  return base::SplitStringPiece(accept_languages_out, ",",
                                base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY);
}

bool HasMicPermission(content::RenderFrameHost& rfh) {
  return rfh.GetBrowserContext()
             ->GetPermissionController()
             ->GetPermissionStatusForCurrentDocument(
                 content::PermissionDescriptorUtil::
                     CreatePermissionDescriptorForPermissionType(
                         blink::PermissionType::AUDIO_CAPTURE),
                 &rfh) == blink::mojom::PermissionStatus::GRANTED;
}

bool HasMicAndAcceptLanguage(
    std::string_view language,
    const std::vector<std::string_view>& accept_languages_list,
    bool has_mic_permission) {
  if (!has_mic_permission) {
    return false;
  }
  return std::ranges::any_of(
      accept_languages_list, [&language](std::string_view accept_lang) {
        return base::EqualsCaseInsensitiveASCII(accept_lang, language);
      });
}

media::mojom::AvailabilityStatus ApplyOriginMasking(
    media::mojom::AvailabilityStatus availability_status,
    bool is_language_masked,
    bool has_mic_and_accept_lang) {
  if (availability_status == media::mojom::AvailabilityStatus::kAvailable &&
      is_language_masked) {
    return media::mojom::AvailabilityStatus::kDownloadable;
  }

  if (availability_status == media::mojom::AvailabilityStatus::kDownloadable &&
      has_mic_and_accept_lang) {
    return media::mojom::AvailabilityStatus::kDownloadableWithoutUserActivation;
  }

  return availability_status;
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
    media::mojom::SpeechRecognitionQuality quality,
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

  if (quality == media::mojom::SpeechRecognitionQuality::kConversation &&
      !base::FeatureList::IsEnabled(media::kOnDeviceWebSpeechGeminiNano)) {
    std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
    return;
  }

  if (quality == media::mojom::SpeechRecognitionQuality::kDictation &&
      !base::FeatureList::IsEnabled(
          media::kOnDeviceWebSpeechSmallExpertModel)) {
    std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  PrefService* profile_prefs = profile ? profile->GetPrefs() : nullptr;
  std::string accept_languages;
  std::vector<std::string_view> accept_languages_list =
      GetAcceptLanguagesList(profile_prefs, accept_languages);

  bool has_mic_permission = HasMicPermission(render_frame_host());

  std::vector<std::string> valid_language_names;
  for (std::string_view language : languages) {
    std::string_view target_language = language;

    if (quality != media::mojom::SpeechRecognitionQuality::kConversation &&
        quality != media::mojom::SpeechRecognitionQuality::kDictation) {
      std::optional<speech::SodaLanguagePackComponentConfig> language_config =
          speech::GetLanguageComponentConfigMatchingLanguageSubtag(language);
      if (!language_config.has_value()) {
        std::move(callback).Run(media::mojom::AvailabilityStatus::kUnavailable);
        return;
      }
      target_language = language_config.value().language_name;
    }
    valid_language_names.emplace_back(target_language);
  }

  // According to the spec, the status returned by this API should be the
  // minimum status. I.e., the API returns:
  //   'available' if all languages are available
  //   'downloading' if all languages are either downloading or available
  //   'downloadable' if all languages are either available, downloading, or
  //   downloadable 'unavailable' in if one or more language is unavailable
  auto barrier_callback =
      base::BarrierCallback<media::mojom::AvailabilityStatus>(
          valid_language_names.size(),
          base::BindOnce(
              [](AvailableCallback callback,
                 const std::vector<media::mojom::AvailabilityStatus>&
                     statuses) {
                media::mojom::AvailabilityStatus overall_status =
                    media::mojom::AvailabilityStatus::kAvailable;
                for (auto status : statuses) {
                  if (GetPriority(status) < GetPriority(overall_status)) {
                    overall_status = status;
                  }
                }
                std::move(callback).Run(overall_status);
              },
              std::move(callback)));

  for (const std::string& language_name : valid_language_names) {
    GetMaskedAvailabilityStatusAsync(language_name, quality,
                                     accept_languages_list, has_mic_permission,
                                     profile_prefs, barrier_callback);
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void OnDeviceSpeechRecognitionImpl::Install(
    const std::vector<std::string>& languages,
    media::mojom::SpeechRecognitionQuality quality,
    OnDeviceSpeechRecognitionImpl::InstallCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(false);
#else
  if (!CanRenderFrameHostUseOnDeviceSpeechRecognition()) {
    std::move(callback).Run(false);
    return;
  }

  if (languages.empty()) {
    std::move(callback).Run(false);
    return;
  }

  if (quality == media::mojom::SpeechRecognitionQuality::kConversation &&
      !base::FeatureList::IsEnabled(media::kOnDeviceWebSpeechGeminiNano)) {
    std::move(callback).Run(false);
    return;
  }

  if (quality == media::mojom::SpeechRecognitionQuality::kDictation &&
      !base::FeatureList::IsEnabled(
          media::kOnDeviceWebSpeechSmallExpertModel)) {
    std::move(callback).Run(false);
    return;
  }

  if (quality == media::mojom::SpeechRecognitionQuality::kConversation ||
      quality == media::mojom::SpeechRecognitionQuality::kDictation) {
    for (std::string_view language : languages) {
      if (GetOnDeviceSpeechRecognitionAvailabilityStatus(
              render_frame_host().GetBrowserContext(), language, quality) ==
          media::mojom::AvailabilityStatus::kUnavailable) {
        std::move(callback).Run(false);
        return;
      }
    }

    OptimizationGuideKeyedService* optimization_guide_keyed_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(
                render_frame_host().GetBrowserContext()));
    if (!optimization_guide_keyed_service) {
      std::move(callback).Run(false);
      return;
    }

    std::set<std::string> language_names_key;
    for (std::string_view language : languages) {
      language_names_key.insert(std::string(language));
    }

    language_installation_callbacks_[language_names_key].push_back(
        std::move(callback));

    model_broker_client_ =
        optimization_guide_keyed_service->CreateModelBrokerClient();

    optimization_guide::mojom::OnDeviceFeature feature =
        quality == media::mojom::SpeechRecognitionQuality::kDictation
            ? optimization_guide::mojom::OnDeviceFeature::
                  kSpeechRecognitionSmallExpertModel
            : optimization_guide::mojom::OnDeviceFeature::
                  kOnDeviceSpeechRecognition;

    // Call `RequestAssetsFor()` to trigger the download and installation of
    // the model.
    model_broker_client_->RequestAssetsFor(feature);

    model_broker_client_->GetSubscriber(feature).WaitForClient(
        base::BindOnce(&OnDeviceSpeechRecognitionImpl::OnModelClientAvailable,
                       weak_ptr_factory_.GetWeakPtr(), language_names_key));
    return;
  }

  for (std::string_view language : languages) {
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

  std::set<std::string> language_names_key;
  for (std::string_view subtag : languages) {
    std::optional<speech::SodaLanguagePackComponentConfig> lang_config =
        speech::GetLanguageComponentConfigMatchingLanguageSubtag(subtag);
    if (lang_config.has_value()) {
      language_names_key.insert(std::string(lang_config.value().language_name));
    }
  }

  if (language_names_key.empty()) {
    std::move(callback).Run(false);
    return;
  }

  std::set<std::string> pending_languages;

  const bool binary_installed =
      speech::SodaInstaller::GetInstance()->IsSodaBinaryInstalled();
  const std::set<speech::LanguageCode> installed_languages =
      speech::SodaInstaller::GetInstance()->InstalledLanguages();

  for (std::string_view language : language_names_key) {
    if (!binary_installed ||
        !installed_languages.contains(speech::GetLanguageCode(language))) {
      pending_languages.insert(std::string(language));
    }
  }

  if (pending_languages.empty()) {
    for (std::string_view language : language_names_key) {
      SetOnDeviceLanguageDownloaded(language);
    }
    std::move(callback).Run(true);
    return;
  }

  language_installation_callbacks_[pending_languages].push_back(
      std::move(callback));

  // `InstallSoda` will only install the SODA binary if it is not already
  // installed.
  speech::SodaInstaller::GetInstance()->InstallSoda(
      g_browser_process->local_state());

  // `InstallLanguage` will only install languages that are not already
  // installed.
  for (std::string_view language : language_names_key) {
    speech::SodaInstaller::GetInstance()->InstallLanguage(
        language, g_browser_process->local_state());
  }

  for (std::string_view language : language_names_key) {
    SetOnDeviceLanguageDownloaded(language);
  }
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
  if (!render_frame_host().IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::
              kOnDeviceSpeechRecognition)) {
    return false;
  }

  content::RenderFrameHost* main_frame = render_frame_host().GetMainFrame();
  if (main_frame->GetSiteInstance()->GetSecurityPrincipal().IsGuest()) {
    return false;
  }

  // Allow trusted/special app contexts (like Chrome Extensions and Isolated Web
  // Apps) that use non-HTTP/HTTPS schemes within custom StoragePartitions.
  if (main_frame->GetStoragePartition() !=
      main_frame->GetBrowserContext()->GetDefaultStoragePartition()) {
    return !main_frame->GetLastCommittedURL().SchemeIsHTTPOrHTTPS();
  }

  return true;
}

#if !BUILDFLAG(IS_ANDROID)

void OnDeviceSpeechRecognitionImpl::ProcessLanguageInstallationUpdate(
    std::string_view language,
    bool installation_success) {
  for (auto it = language_installation_callbacks_.begin();
       it != language_installation_callbacks_.end();) {
    std::set<std::string> pending_languages_key = it->first;

    // If the SODA binary (empty language string) failed, fail all pending
    // installations.
    if (language.empty() && !installation_success) {
      std::list<InstallCallback> moved_callbacks = std::move(it->second);
      it = language_installation_callbacks_.erase(it);
      for (auto& callback : moved_callbacks) {
        std::move(callback).Run(false);
      }
      continue;
    }

    if (pending_languages_key.count(std::string(language))) {
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
        updated_key.erase(std::string(language));

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

void OnDeviceSpeechRecognitionImpl::GetMaskedAvailabilityStatusAsync(
    std::string_view language,
    media::mojom::SpeechRecognitionQuality quality,
    const std::vector<std::string_view>& accept_languages_list,
    bool has_mic_permission,
    PrefService* profile_prefs,
    base::OnceCallback<void(media::mojom::AvailabilityStatus)> callback) {
  bool has_mic_and_accept_lang = HasMicAndAcceptLanguage(
      language, accept_languages_list, has_mic_permission);

  GetOnDeviceSpeechRecognitionAvailabilityStatusAsync(
      render_frame_host().GetBrowserContext(), language, quality,
      base::BindOnce(
          [](base::WeakPtr<OnDeviceSpeechRecognitionImpl> self,
             std::string language_str, bool has_mic_and_accept_lang,
             PrefService* profile_prefs,
             base::OnceCallback<void(media::mojom::AvailabilityStatus)>
                 callback,
             media::mojom::AvailabilityStatus availability_status) {
            if (!self) {
              std::move(callback).Run(
                  media::mojom::AvailabilityStatus::kUnavailable);
              return;
            }
            bool is_masked = self->IsLanguageAvailabilityMaskedForOrigin(
                language_str, has_mic_and_accept_lang, profile_prefs);
            std::move(callback).Run(ApplyOriginMasking(
                availability_status, is_masked, has_mic_and_accept_lang));
          },
          weak_ptr_factory_.GetWeakPtr(), std::string(language),
          has_mic_and_accept_lang, profile_prefs, std::move(callback)));
}

bool OnDeviceSpeechRecognitionImpl::IsLanguageAvailabilityMaskedForOrigin(
    std::string_view language,
    bool has_mic_and_accept_lang,
    PrefService* profile_prefs) {
  if (base::FeatureList::IsEnabled(media::kPreemptiveSodaDownload) &&
      profile_prefs) {
    if (language ==
        speech::GetDefaultLiveCaptionLanguage(
            g_browser_process->GetApplicationLocale(), profile_prefs)) {
      return false;
    }
  }

  if (has_mic_and_accept_lang) {
    return false;
  }

  const GURL url = render_frame_host().GetLastCommittedOrigin().GetURL();
  if (!url.is_valid() || url.SchemeIsFile()) {
    return !transient_on_device_languages_downloaded_.contains(
        std::string(language));
  }

  base::Value on_device_languages_downloaded_value =
      GetOnDeviceLanguagesDownloadedValue();
  if (on_device_languages_downloaded_value.is_dict()) {
    return !on_device_languages_downloaded_value.GetDict()
                .EnsureList(kOnDeviceLanguagesDownloadedKey)
                ->contains(language);
  }

  return true;
}

void OnDeviceSpeechRecognitionImpl::SetOnDeviceLanguageDownloaded(
    std::string_view language) {
  const GURL url = render_frame_host().GetLastCommittedOrigin().GetURL();
  if (!url.is_valid() || url.SchemeIsFile()) {
    transient_on_device_languages_downloaded_.insert(std::string(language));
    return;
  }

  base::Value on_device_languages_downloaded_value =
      GetOnDeviceLanguagesDownloadedValue();

  // Initialize a list to store data, if none exists.
  if (!on_device_languages_downloaded_value.is_dict()) {
    on_device_languages_downloaded_value = base::Value(base::DictValue());
  }

  // Update or initialize the list of targets for the source language.
  base::ListValue* on_device_languages_downloaded_list =
      on_device_languages_downloaded_value.GetDict().EnsureList(
          kOnDeviceLanguagesDownloadedKey);
  if (!on_device_languages_downloaded_list->contains(language)) {
    on_device_languages_downloaded_list->Append(language);
  }

  SetOnDeviceLanguagesDownloadedContentSetting(
      std::move(on_device_languages_downloaded_value));
}

void OnDeviceSpeechRecognitionImpl::OnModelClientAvailable(
    std::set<std::string> languages,
    base::WeakPtr<optimization_guide::ModelClient> client) {
  for (const std::string& language : languages) {
    if (client) {
      SetOnDeviceLanguageDownloaded(language);
    }
    ProcessLanguageInstallationUpdate(language, !!client);
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

DOCUMENT_USER_DATA_KEY_IMPL(OnDeviceSpeechRecognitionImpl);

}  // namespace speech
