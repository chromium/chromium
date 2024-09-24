// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"

#include <memory>

#include "base/feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/live_caption_ui_remote_driver.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
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
      base::BindRepeating(
          &SpeechRecognitionClientBrowserInterface::OnLiveCaptionPrefChange,
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
  speech::SodaInstaller::GetInstance()->AddObserver(this);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  pref_change_registrar_->Add(
      prefs::kUserMicrophoneCaptionLanguageCode,
      base::BindRepeating(
          &SpeechRecognitionClientBrowserInterface::OnBabelOrcaLanguageChanged,
          base::Unretained(this)));
#endif
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
  speech_recognition_availibility_observers_.Add(std::move(pending_remote));
  OnSpeechRecognitionAvailabilityChanged(availability_);
}

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
void SpeechRecognitionClientBrowserInterface::
    BindBabelOrcaSpeechRecognitionBrowserObserver(
        mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
            pending_remote) {
  babel_orca_speech_recognition_availability_observers_.Add(
      std::move(pending_remote));
  OnSpeechRecognitionAvailabilityChanged(availability_);
}
#endif

void SpeechRecognitionClientBrowserInterface::OnSodaInstalled(
    speech::LanguageCode language_code) {
  if (prefs::IsLanguageCodeForLiveCaption(language_code, profile_prefs_) &&
      availability_.IsWaitingOnLiveCaption()) {
    NotifyObservers(CopyAvailabilityAndFlipLiveCaption(
        profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled)));
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (prefs::IsLanguageCodeForMicrophoneCaption(language_code,
                                                profile_prefs_) &&
      availability_.IsWaitingOnBabelOrca()) {
    NotifyObservers(CopyAvailabilityAndFlipBabelOrca(/*enabled=*/true));
  }
#endif
}

SpeechRecognitionClientBrowserInterface::Availability
SpeechRecognitionClientBrowserInterface::CopyAvailabilityAndFlipLiveCaption(
    bool enabled) {
  return {enabled ? AvailabilityState::kReady : AvailabilityState::kDisabled,
          availability_.babel_orca_availability};
}

SpeechRecognitionClientBrowserInterface::Availability
SpeechRecognitionClientBrowserInterface::CopyAvailabilityAndFlipBabelOrca(
    bool enabled) {
  return {
      availability_.live_caption_availability,
      enabled ? AvailabilityState::kReady : AvailabilityState::kDisabled,
  };
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void SpeechRecognitionClientBrowserInterface::
    ChangeBabelOrcaSpeechRecognitionAvailability(bool enabled) {
  OnSpeechRecognitionAvailabilityChanged(
      CopyAvailabilityAndFlipBabelOrca(enabled));
}
#endif

void SpeechRecognitionClientBrowserInterface::OnLiveCaptionPrefChange() {
  OnSpeechRecognitionAvailabilityChanged(CopyAvailabilityAndFlipLiveCaption(
      profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled)));
}

void SpeechRecognitionClientBrowserInterface::
    OnSpeechRecognitionAvailabilityChanged(
        const Availability& new_availability) {
  Availability availability_copy = new_availability;

  if (new_availability.IsBabelOrcaAvailable() &&
      !IsBabelOrcaSodaPackAvailable()) {
    availability_copy.babel_orca_availability = AvailabilityState::kWaiting;
  }

  if (new_availability.IsLiveCaptionAvailable() &&
      !IsLiveCaptionSodaPackAvailable()) {
    availability_copy.live_caption_availability = AvailabilityState::kWaiting;
  }

  NotifyObservers(availability_copy);
}

void SpeechRecognitionClientBrowserInterface::OnLiveCaptionLanguageChanged() {
  if (!IsLiveCaptionSodaPackAvailable()) {
    availability_.live_caption_availability = AvailabilityState::kWaiting;
  }

  for (auto& observer : speech_recognition_availibility_observers_) {
    observer->SpeechRecognitionLanguageChanged(
        prefs::GetLiveCaptionLanguageCode(profile_prefs_));
  }
}

void SpeechRecognitionClientBrowserInterface::OnBabelOrcaLanguageChanged() {
// No-op on non ChromeOS builds.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!IsBabelOrcaSodaPackAvailable()) {
    availability_.babel_orca_availability = AvailabilityState::kWaiting;
  }

  for (auto& observer : babel_orca_speech_recognition_availability_observers_) {
    observer->SpeechRecognitionLanguageChanged(
        prefs::GetLiveCaptionLanguageCode(profile_prefs_));
  }
#endif
}

void SpeechRecognitionClientBrowserInterface::
    OnSpeechRecognitionMaskOffensiveWordsChanged() {
  bool mask_offensive_words =
      profile_prefs_->GetBoolean(prefs::kLiveCaptionMaskOffensiveWords);
  for (auto& observer : speech_recognition_availibility_observers_) {
    observer->SpeechRecognitionMaskOffensiveWordsChanged(mask_offensive_words);
  }
}

void SpeechRecognitionClientBrowserInterface::NotifyObservers(
    const Availability& new_availability) {
  // Only notify if availability has changed.
  if (new_availability.LiveCaptionChanged(availability_) &&
      !new_availability.IsWaitingOnLiveCaption()) {
    NotifyLiveCaptionObservers(new_availability);
  }
  if (new_availability.BabelOrcaChanged(availability_) &&
      !new_availability.IsWaitingOnBabelOrca()) {
    NotifyBabelOrcaObservers(new_availability);
  }
  // Copy availability back.
  availability_ = new_availability;
}

void SpeechRecognitionClientBrowserInterface::NotifyLiveCaptionObservers(
    const Availability& new_availability) {
  for (auto& observer : speech_recognition_availibility_observers_) {
    observer->SpeechRecognitionAvailabilityChanged(
        new_availability.IsLiveCaptionAvailable());
  }
}

void SpeechRecognitionClientBrowserInterface::NotifyBabelOrcaObservers(
    const Availability& new_availability) {
  for (auto& observer : babel_orca_speech_recognition_availability_observers_) {
    observer->SpeechRecognitionAvailabilityChanged(
        new_availability.IsBabelOrcaAvailable());
  }
}

bool SpeechRecognitionClientBrowserInterface::IsBabelOrcaSodaPackAvailable() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return speech::SodaInstaller::GetInstance()->IsSodaInstalled(
      speech::GetLanguageCode(
          prefs::GetUserMicrophoneCaptionLanguage(profile_prefs_)));
#else
  return false;
#endif
}

bool SpeechRecognitionClientBrowserInterface::IsLiveCaptionSodaPackAvailable() {
  return speech::SodaInstaller::GetInstance()->IsSodaInstalled(
      speech::GetLanguageCode(
          prefs::GetLiveCaptionLanguageCode(profile_prefs_)));
}

}  // namespace speech
