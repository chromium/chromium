// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_speech_recognition_impl.h"

#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
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
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/soda/soda_util.h"

namespace {
// Returns a boolean indicating whether the language is enabled.
bool IsLanguageInstallable(const std::string& language_code,
                           bool is_soda_binary_installed) {
  return base::Contains(
      speech::SodaInstaller::GetInstance()->GetLiveCaptionEnabledLanguages(),
      language_code);
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
  std::move(callback).Run(IsOnDeviceSpeechRecognitionAvailable(language));
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

  if (!CanInstallWithoutUserConsent(language_config.value().language_name)) {
    std::move(callback).Run(false);

    // TODO(crbug.com/40286514): Prompt the user for permission to download
    // language pack.
    return;
  }

  language_installation_callbacks_[language].push_back(std::move(callback));
  // `InstallSoda` will only install the SODA binary if it is not already
  // installed.
  speech::SodaInstaller::GetInstance()->InstallSoda(
      g_browser_process->local_state());

  // `InstallLanguage` will only install languages that are not already
  // installed.
  speech::SodaInstaller::GetInstance()->InstallLanguage(
      language_config.value().language_name, g_browser_process->local_state());
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
    : content::DocumentUserData<OnDeviceSpeechRecognitionImpl>(frame_host),
      pref_service_(Profile::FromBrowserContext(
                        frame_host->GetProcess()->GetBrowserContext())
                        ->GetPrefs()),
      language_prefs_(
          std::make_unique<language::LanguagePrefs>(pref_service_)) {
#if !BUILDFLAG(IS_ANDROID)
  speech::SodaInstaller* soda_installer = speech::SodaInstaller::GetInstance();
  if (soda_installer) {
    soda_installer->AddObserver(this);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool OnDeviceSpeechRecognitionImpl::CanInstallWithoutUserConsent(
    const std::string& language) {
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  if (connection_type != net::NetworkChangeNotifier::CONNECTION_ETHERNET &&
      connection_type != net::NetworkChangeNotifier::CONNECTION_WIFI) {
    return false;
  }

  std::vector<std::string> accept_languages;
  language_prefs_->GetAcceptLanguagesList(&accept_languages);
  for (auto accept_language : accept_languages) {
    if (l10n_util::GetLanguage(base::ToLowerASCII(accept_language)) ==
        l10n_util::GetLanguage(base::ToLowerASCII(language))) {
      return true;
    }
  }

  return false;
}

#if !BUILDFLAG(IS_ANDROID)
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
#endif  // !BUILDFLAG(IS_ANDROID)
DOCUMENT_USER_DATA_KEY_IMPL(OnDeviceSpeechRecognitionImpl);

}  // namespace speech
