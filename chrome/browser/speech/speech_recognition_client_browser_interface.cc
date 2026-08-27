// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "media/base/media_switches.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/speech/speech_recognition_small_expert_model_installer.h"
#endif

class PrefChangeRegistrar;

namespace speech {

SpeechRecognitionClientBrowserInterface::
    SpeechRecognitionClientBrowserInterface(content::BrowserContext* context)
    : context_(context) {
  Profile* profile = Profile::FromBrowserContext(context);
  profile_prefs_ = profile->GetPrefs();

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_prefs_);

  // Unretained is safe because |this| owns the pref_change_registrar_.
  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(&SpeechRecognitionClientBrowserInterface::
                              OnLiveCaptionAvailabilityChanged,
                          base::Unretained(this)));
  // Reuse the same callback, since it does the same thing regardless of which
  // pref changed.  Ignore the pref if the feature is off, though.
  if (captions::IsHeadlessCaptionFeatureSupported()) {
    pref_change_registrar_->Add(
        prefs::kHeadlessCaptionEnabled,
        base::BindRepeating(&SpeechRecognitionClientBrowserInterface::
                                OnLiveCaptionAvailabilityChanged,
                            base::Unretained(this)));
  }
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
  if (speech::SodaInstaller::GetInstance()) {
    soda_installer_observation_.Observe(speech::SodaInstaller::GetInstance());
  }
#if !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          media::kLiveCaptionSpeechRecognitionSmallExpertModel) &&
      g_browser_process &&
      g_browser_process->speech_recognition_small_expert_model_installer()) {
    speech_recognition_small_expert_model_installer_observation_.Observe(
        g_browser_process->speech_recognition_small_expert_model_installer());
  }
#endif
  MaybeTriggerModelInstall();
}

SpeechRecognitionClientBrowserInterface::
    ~SpeechRecognitionClientBrowserInterface() = default;

void SpeechRecognitionClientBrowserInterface::BindReceiver(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionClientBrowserInterface>
        receiver) {
  speech_recognition_client_browser_interface_.Add(this, std::move(receiver));
}

void SpeechRecognitionClientBrowserInterface::
    BindSpeechRecognitionBrowserObserver(
        mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
            pending_remote) {
  live_caption_availability_observers_.Add(std::move(pending_remote));
  OnLiveCaptionAvailabilityChanged();
}

void SpeechRecognitionClientBrowserInterface::REMOVED_1() {
  NOTIMPLEMENTED();
}

#if BUILDFLAG(IS_CHROMEOS)
void SpeechRecognitionClientBrowserInterface::REMOVED_2(
    mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
        pending_remote) {
  NOTIMPLEMENTED();
}
#endif

void SpeechRecognitionClientBrowserInterface::OnSodaInstalled(
    speech::LanguageCode /*language_code*/) {
  NotifyLiveCaptionObserversIfNeeded();
}

#if !BUILDFLAG(IS_CHROMEOS)
void SpeechRecognitionClientBrowserInterface::
    OnSpeechRecognitionSmallExpertModelStateChanged(
        speech::SpeechRecognitionSmallExpertModelInstaller::
            SpeechRecognitionSmallExpertModelState /*state*/) {
  NotifyLiveCaptionObserversIfNeeded();
}
#endif

void SpeechRecognitionClientBrowserInterface::
    OnLiveCaptionAvailabilityChanged() {
  MaybeTriggerModelInstall();
  NotifyLiveCaptionObserversIfNeeded();
}

void SpeechRecognitionClientBrowserInterface::OnLiveCaptionLanguageChanged() {
  MaybeTriggerModelInstall();
  NotifyLiveCaptionObserversIfNeeded();
  const std::string language =
      prefs::GetLiveCaptionLanguageCode(profile_prefs_);
  for (auto& observer : live_caption_availability_observers_) {
    observer->SpeechRecognitionLanguageChanged(language);
  }
}

void SpeechRecognitionClientBrowserInterface::
    OnSpeechRecognitionMaskOffensiveWordsChanged() {
  bool mask_offensive_words =
      profile_prefs_->GetBoolean(prefs::kLiveCaptionMaskOffensiveWords);
  for (auto& observer : live_caption_availability_observers_) {
    observer->SpeechRecognitionMaskOffensiveWordsChanged(mask_offensive_words);
  }
}

void SpeechRecognitionClientBrowserInterface::MaybeTriggerModelInstall() {
  bool enabled = profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled);
  if (captions::IsHeadlessCaptionFeatureSupported()) {
    enabled |= profile_prefs_->GetBoolean(prefs::kHeadlessCaptionEnabled);
  }
  if (!enabled) {
    return;
  }

  const std::string language_name =
      prefs::GetLiveCaptionLanguageCode(profile_prefs_);

#if !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          media::kLiveCaptionSpeechRecognitionSmallExpertModel)) {
    auto* installer =
        g_browser_process
            ? g_browser_process
                  ->speech_recognition_small_expert_model_installer()
            : nullptr;
    if (installer &&
        !installer->IsSpeechRecognitionSmallExpertModelInstalled() &&
        !installer->IsSpeechRecognitionSmallExpertModelDownloading() &&
        installer->GetSpeechRecognitionSmallExpertModelState() !=
            SpeechRecognitionSmallExpertModelInstaller::
                SpeechRecognitionSmallExpertModelState::kError &&
        installer->GetSpeechRecognitionSmallExpertModelState() !=
            SpeechRecognitionSmallExpertModelInstaller::
                SpeechRecognitionSmallExpertModelState::
                    kErrorCorruptPersistent) {
      Profile* profile = Profile::FromBrowserContext(context_);
      installer->InstallSpeechRecognitionSmallExpertModel(profile);
    }
    return;
  }
#endif
  if (speech::SodaInstaller::GetInstance()) {
    speech::SodaInstaller* soda_installer =
        speech::SodaInstaller::GetInstance();
    PrefService* global_prefs =
        g_browser_process ? g_browser_process->local_state() : nullptr;
    if (!soda_installer->IsSodaBinaryInstalled()) {
      soda_installer->InstallSoda(global_prefs);
    }
    if (!soda_installer->IsSodaInstalled(
            speech::GetLanguageCode(language_name))) {
      soda_installer->InstallLanguage(language_name, global_prefs);
    }
  }
}

void SpeechRecognitionClientBrowserInterface::
    NotifyLiveCaptionObserversIfNeeded() {
  if (live_caption_availability_observers_.empty()) {
    return;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          media::kLiveCaptionSpeechRecognitionSmallExpertModel)) {
    if (!g_browser_process ||
        !g_browser_process->speech_recognition_small_expert_model_installer()) {
      return;
    }
  } else if (!speech::SodaInstaller::GetInstance()) {
    return;
  }
#else
  if (!speech::SodaInstaller::GetInstance()) {
    return;
  }
#endif

  // Captioning is enabled if either Live Caption or Headless Caption.
  bool enabled = profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled);
  if (captions::IsHeadlessCaptionFeatureSupported()) {
    enabled |= profile_prefs_->GetBoolean(prefs::kHeadlessCaptionEnabled);
  }

  const std::string language_name =
      prefs::GetLiveCaptionLanguageCode(profile_prefs_);
  bool is_installed = false;
#if !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          media::kLiveCaptionSpeechRecognitionSmallExpertModel)) {
    auto* installer =
        g_browser_process
            ? g_browser_process
                  ->speech_recognition_small_expert_model_installer()
            : nullptr;
    if (installer &&
        installer->IsSpeechRecognitionSmallExpertModelInstalled()) {
      is_installed = true;
    }
  } else {
    if (speech::SodaInstaller::GetInstance()) {
      is_installed = speech::SodaInstaller::GetInstance()->IsSodaInstalled(
          speech::GetLanguageCode(language_name));
    }
  }
#else
  if (speech::SodaInstaller::GetInstance()) {
    is_installed = speech::SodaInstaller::GetInstance()->IsSodaInstalled(
        speech::GetLanguageCode(language_name));
  }
#endif

  bool available = is_installed && enabled;
  for (auto& observer : live_caption_availability_observers_) {
    observer->SpeechRecognitionAvailabilityChanged(available);
  }
}

}  // namespace speech
