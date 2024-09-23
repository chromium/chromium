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
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/soda/soda_util.h"

namespace {
// Returns a boolean indicating whether the language is both enabled and not
// already installed.
bool IsLanguageInstallable(const std::string& language_code,
                           bool is_soda_binary_installed) {
  if (is_soda_binary_installed) {
    // Check if the language pack is already installed if the SODA binary was
    // already installed. If the SODA binary has not been installed,
    // `prefs::kSodaRegisteredLanguagePacks` will contain the default language
    // pack which may not exist on the device.
    for (const auto& language : g_browser_process->local_state()->GetList(
             prefs::kSodaRegisteredLanguagePacks)) {
      if (language.GetString() == language_code) {
        return false;
      }
    }
  }

  return base::Contains(
      speech::SodaInstaller::GetInstance()->GetLiveCaptionEnabledLanguages(),
      language_code);
}

}  // namespace
#endif  // !BUILDFLAG(IS_ANDROID)

namespace speech {

OnDeviceSpeechRecognitionImpl::~OnDeviceSpeechRecognitionImpl() = default;

void OnDeviceSpeechRecognitionImpl::Bind(
    mojo::PendingReceiver<media::mojom::OnDeviceSpeechRecognition> receiver) {
  receiver_.Bind(std::move(receiver));
}

void OnDeviceSpeechRecognitionImpl::OnDeviceWebSpeechAvailable(
    const std::string& language,
    OnDeviceSpeechRecognitionImpl::OnDeviceWebSpeechAvailableCallback
        callback) {
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(false);
#else
  std::move(callback).Run(
      speech::IsOnDeviceSpeechRecognitionAvailable(language));
#endif  // BUILDFLAG(IS_ANDROID)
}

void OnDeviceSpeechRecognitionImpl::InstallOnDeviceSpeechRecognition(
    const std::string& language,
    OnDeviceSpeechRecognitionImpl::InstallOnDeviceSpeechRecognitionCallback
        callback) {
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(false);
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

  // `InstallSoda` will only install the SODA binary if it is not already
  // installed.
  speech::SodaInstaller::GetInstance()->InstallSoda(
      g_browser_process->local_state());

  // `InstallLanguage` will only install languages that are not already
  // installed.
  speech::SodaInstaller::GetInstance()->InstallLanguage(
      language_config.value().language_name, g_browser_process->local_state());
  std::move(callback).Run(true);
#endif  // BUILDFLAG(IS_ANDROID)
}

OnDeviceSpeechRecognitionImpl::OnDeviceSpeechRecognitionImpl(
    content::RenderFrameHost* frame_host)
    : content::DocumentUserData<OnDeviceSpeechRecognitionImpl>(frame_host),
      pref_service_(Profile::FromBrowserContext(
                        frame_host->GetProcess()->GetBrowserContext())
                        ->GetPrefs()),
      language_prefs_(
          std::make_unique<language::LanguagePrefs>(pref_service_)) {}

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

DOCUMENT_USER_DATA_KEY_IMPL(OnDeviceSpeechRecognitionImpl);

}  // namespace speech
