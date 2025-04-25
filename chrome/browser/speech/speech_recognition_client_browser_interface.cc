// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/caption_util.h"
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
    speech::LanguageCode language_code) {
  NotifyLiveCaptionObserversIfNeeded();
}

void SpeechRecognitionClientBrowserInterface::
    OnLiveCaptionAvailabilityChanged() {
  NotifyLiveCaptionObserversIfNeeded();
}

void SpeechRecognitionClientBrowserInterface::OnLiveCaptionLanguageChanged() {
  const std::string language =
      prefs::GetLiveCaptionLanguageCode(profile_prefs_);
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

void SpeechRecognitionClientBrowserInterface::
    NotifyLiveCaptionObserversIfNeeded() {
  if (live_caption_availibility_observers_.empty()) {
    return;
  }

  bool is_language_installed =
      speech::SodaInstaller::GetInstance()->IsSodaInstalled(
          speech::GetLanguageCode(
              prefs::GetLiveCaptionLanguageCode(profile_prefs_)));
  if (!is_language_installed) {
    // Don't notify until the language pack is installed, regardless of whether
    // recognition is enabled or not.
    return;
  }

  // Captioning is enabled if either Live Caption or Headless Caption.
  bool enabled = profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled);
  if (captions::IsHeadlessCaptionFeatureSupported()) {
    enabled |= profile_prefs_->GetBoolean(prefs::kHeadlessCaptionEnabled);
  }

  for (auto& observer : live_caption_availibility_observers_) {
    observer->SpeechRecognitionAvailabilityChanged(enabled);
  }
}

}  // namespace speech
