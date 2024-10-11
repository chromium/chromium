// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/live_caption_ui_remote_driver.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "media/base/media_switches.h"

class PrefChangeRegistrar;

namespace speech {

SpeechRecognitionClientBrowserInterface::
    SpeechRecognitionClientBrowserInterface(content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  profile_prefs_ = profile->GetPrefs();
  controller_ = captions::LiveCaptionControllerFactory::GetForProfile(profile);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_prefs_);

  // Unretained is safe because |this| owns the pref_change_registrar_.
  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(&SpeechRecognitionClientBrowserInterface::
                              OnLiveCaptionAvailabilityChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLiveCaptionLanguageCode,
      base::BindRepeating(&SpeechRecognitionClientBrowserInterface::
                              OnLiveCaptionLanguageChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLiveCaptionMaskOffensiveWords,
      base::BindRepeating(&SpeechRecognitionClientBrowserInterface::
                              OnSpeechRecognitionMaskOffensiveWordsChanged,
                          base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  pref_change_registrar_->Add(
      prefs::kUserMicrophoneCaptionLanguageCode,
      base::BindRepeating(
          &SpeechRecognitionClientBrowserInterface::OnBabelOrcaLanguageChanged,
          base::Unretained(this)));
#endif
  speech::SodaInstaller::GetInstance()->AddObserver(this);
}

SpeechRecognitionClientBrowserInterface::
    ~SpeechRecognitionClientBrowserInterface() {
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
}

void SpeechRecognitionClientBrowserInterface::BindReceiver(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionClientBrowserInterface>
        receiver) {
  speech_recognition_client_browser_interface_.Add(this, std::move(receiver));
}

void SpeechRecognitionClientBrowserInterface::
    BindSpeechRecognitionBrowserObserver(
        mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
            pending_remote) {
  live_caption_availibility_observers_.Add(std::move(pending_remote));
  OnLiveCaptionAvailabilityChanged();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void SpeechRecognitionClientBrowserInterface::
    BindBabelOrcaSpeechRecognitionBrowserObserver(
        mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
            pending_remote) {
  babel_orca_availability_observers_.Add(std::move(pending_remote));
  OnBabelOrcaAvailabilityChanged(babel_orca_enabled_);
}

void SpeechRecognitionClientBrowserInterface::
    ChangeBabelOrcaSpeechRecognitionAvailability(bool enabled) {
  babel_orca_enabled_ = true;
  OnBabelOrcaAvailabilityChanged(enabled);
}
#endif

void SpeechRecognitionClientBrowserInterface::BindRecognizerToRemoteClient(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        client_receiver,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionSurfaceClient>
        surface_client_receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionSurface> surface_remote,
    media::mojom::SpeechRecognitionSurfaceMetadataPtr metadata) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ui_drivers_.Add(
      std::make_unique<captions::LiveCaptionUiRemoteDriver>(
          controller_, std::move(surface_client_receiver),
          std::move(surface_remote), metadata->session_id.ToString()),
      std::move(client_receiver));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void SpeechRecognitionClientBrowserInterface::OnSodaInstalled(
    speech::LanguageCode language_code) {
  if (waiting_on_live_caption_ &&
      prefs::IsLanguageCodeForLiveCaption(language_code, profile_prefs_)) {
    waiting_on_live_caption_ = false;
    NotifyLiveCaptionObservers(
        profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (waiting_on_babel_orca_ && prefs::IsLanguageCodeForMicrophoneCaption(
                                    language_code, profile_prefs_)) {
    waiting_on_babel_orca_ = false;
    NotifyBabelOrcaCaptionObservers(babel_orca_enabled_);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void SpeechRecognitionClientBrowserInterface::
    OnLiveCaptionAvailabilityChanged() {
  if (live_caption_availibility_observers_.empty()) {
    return;
  }

  bool enabled = profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled);
  bool is_language_installed =
      speech::SodaInstaller::GetInstance()->IsSodaInstalled(
          speech::GetLanguageCode(
              prefs::GetLiveCaptionLanguageCode(profile_prefs_)));

  if (enabled && is_language_installed) {
    NotifyLiveCaptionObservers(enabled);
  } else if (enabled && !is_language_installed) {
    waiting_on_live_caption_ = true;
  } else {
    NotifyLiveCaptionObservers(enabled);
  }
}

void SpeechRecognitionClientBrowserInterface::OnLiveCaptionLanguageChanged() {
  const std::string language =
      prefs::GetLiveCaptionLanguageCode(profile_prefs_);
  if (!SodaInstaller::GetInstance()->IsSodaInstalled(
          speech::GetLanguageCode(language))) {
    waiting_on_live_caption_ = true;
  }
  for (auto& observer : live_caption_availibility_observers_) {
    observer->SpeechRecognitionLanguageChanged(language);
  }
}

void SpeechRecognitionClientBrowserInterface::
    OnSpeechRecognitionMaskOffensiveWordsChanged() {
  bool mask_offensive_words =
      profile_prefs_->GetBoolean(prefs::kLiveCaptionMaskOffensiveWords);
  for (auto& observer : live_caption_availibility_observers_) {
    observer->SpeechRecognitionMaskOffensiveWordsChanged(mask_offensive_words);
  }
}

void SpeechRecognitionClientBrowserInterface::NotifyLiveCaptionObservers(
    bool enabled) {
  for (auto& observer : live_caption_availibility_observers_) {
    observer->SpeechRecognitionAvailabilityChanged(enabled);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void SpeechRecognitionClientBrowserInterface::OnBabelOrcaAvailabilityChanged(
    bool enabled) {
  if (babel_orca_availability_observers_.empty()) {
    return;
  }

  bool is_language_installed =
      speech::SodaInstaller::GetInstance()->IsSodaInstalled(
          speech::GetLanguageCode(
              prefs::GetUserMicrophoneCaptionLanguage(profile_prefs_)));

  if (enabled && is_language_installed) {
    NotifyBabelOrcaCaptionObservers(enabled);
  } else if (enabled && !is_language_installed) {
    waiting_on_babel_orca_ = true;
  } else {
    NotifyBabelOrcaCaptionObservers(enabled);
  }
}

void SpeechRecognitionClientBrowserInterface::OnBabelOrcaLanguageChanged() {
  const std::string language =
      prefs::GetUserMicrophoneCaptionLanguage(profile_prefs_);
  if (!SodaInstaller::GetInstance()->IsSodaInstalled(
          speech::GetLanguageCode(language))) {
    waiting_on_babel_orca_ = true;
  }
  for (auto& observer : babel_orca_availability_observers_) {
    observer->SpeechRecognitionLanguageChanged(language);
  }
}

void SpeechRecognitionClientBrowserInterface::NotifyBabelOrcaCaptionObservers(
    bool enabled) {
  for (auto& observer : babel_orca_availability_observers_) {
    observer->SpeechRecognitionAvailabilityChanged(enabled);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace speech
