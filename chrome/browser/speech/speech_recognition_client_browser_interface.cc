// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"

#include <memory>

#include "chrome/browser/accessibility/soda_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
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
  speech_recognition_availibility_observers_.Add(std::move(pending_remote));
  OnSpeechRecognitionAvailabilityChanged();
}

void SpeechRecognitionClientBrowserInterface::OnSodaInstalled() {
  speech::SodaInstaller::GetInstance()->RemoveObserver(this);
  NotifyObservers(profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled));
}

void SpeechRecognitionClientBrowserInterface::
    OnSpeechRecognitionAvailabilityChanged() {
  if (speech_recognition_availibility_observers_.empty())
    return;

  bool enabled = profile_prefs_->GetBoolean(prefs::kLiveCaptionEnabled);

  if (enabled) {
    if (!base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption) ||
        speech::SodaInstaller::GetInstance()->IsSodaInstalled()) {
      NotifyObservers(enabled);
    } else {
      speech::SodaInstaller::GetInstance()->AddObserver(this);
    }
  } else {
    speech::SodaInstaller::GetInstance()->RemoveObserver(this);
    NotifyObservers(enabled);
  }
}

void SpeechRecognitionClientBrowserInterface::
    OnSpeechRecognitionLanguageChanged() {
  for (auto& observer : speech_recognition_availibility_observers_) {
    observer->SpeechRecognitionLanguageChanged(
        profile_prefs_->GetString(prefs::kLiveCaptionLanguageCode));
  }
}

void SpeechRecognitionClientBrowserInterface::NotifyObservers(bool enabled) {
  for (auto& observer : speech_recognition_availibility_observers_) {
    observer->SpeechRecognitionAvailabilityChanged(enabled);
  }
}

}  // namespace speech
