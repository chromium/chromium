// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
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

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(profile_prefs_);

  // Unretained is safe because |this| owns the pref_change_registrar_.
  pref_change_registrar_->Add(
      prefs::kLiveCaptionEnabled,
      base::BindRepeating(&SpeechRecognitionClientBrowserInterface::
                              OnSpeechRecognitionAvailabilityChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kLiveCaptionLanguageCode,
      base::BindRepeating(&SpeechRecognitionClientBrowserInterface::
                              OnSpeechRecognitionLanguageChanged,
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
  speech_recognition_availibility_observers_.Add(std::move(pending_remote));
  OnSpeechRecognitionAvailabilityChanged();
}

void SpeechRecognitionClientBrowserInterface::OnSodaInstalled(
    speech::LanguageCode language_code) {
  if (!prefs::IsLanguageCodeForLiveCaption(language_code, profile_prefs_))
    return;
  NotifyObservers(profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled));

  if (base::FeatureList::IsEnabled(media::kLiveCaptionMultiLanguage)) {
    OnSpeechRecognitionLanguageChanged();
  }
}

void SpeechRecognitionClientBrowserInterface::
    OnSpeechRecognitionAvailabilityChanged() {
  if (speech_recognition_availibility_observers_.empty())
    return;

  bool enabled = profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled);

  if (enabled) {
    const std::string live_caption_locale =
        prefs::GetLiveCaptionLanguageCode(profile_prefs_);
    if (speech::SodaInstaller::GetInstance()->IsSodaInstalled(
            speech::GetLanguageCode(live_caption_locale))) {
      NotifyObservers(enabled);
    }
  } else {
    NotifyObservers(enabled);
  }
}

void SpeechRecognitionClientBrowserInterface::
    OnSpeechRecognitionLanguageChanged() {
  for (auto& observer : speech_recognition_availibility_observers_) {
    observer->SpeechRecognitionLanguageChanged(
        prefs::GetLiveCaptionLanguageCode(profile_prefs_));
  }
}

void SpeechRecognitionClientBrowserInterface::NotifyObservers(bool enabled) {
  for (auto& observer : speech_recognition_availibility_observers_) {
    observer->SpeechRecognitionAvailabilityChanged(enabled);
  }
}

}  // namespace speech
