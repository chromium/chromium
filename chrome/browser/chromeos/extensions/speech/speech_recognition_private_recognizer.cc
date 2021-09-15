// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_recognizer.h"

#include "chrome/browser/chromeos/extensions/speech/speech_recognition_private_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/network_speech_recognizer.h"
#include "chrome/browser/speech/on_device_speech_recognizer.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"

namespace extensions {

SpeechRecognitionPrivateRecognizer::SpeechRecognitionPrivateRecognizer() {}

SpeechRecognitionPrivateRecognizer::~SpeechRecognitionPrivateRecognizer() {}

void SpeechRecognitionPrivateRecognizer::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  if (new_state == SPEECH_RECOGNIZER_READY) {
    if (current_state_ == SPEECH_RECOGNIZER_OFF && speech_recognizer_) {
      // The SpeechRecognizer is ready to start recognizing speech.
      speech_recognizer_->Start();
    } else {
      // TODO(crbug.com/1246044): Turn the speech recognizer off. Implement this
      // when working on stop() and onStop().
    }
  } else if (new_state == SPEECH_RECOGNIZER_RECOGNIZING) {
    DCHECK(!on_start_callback_.is_null());
    std::move(on_start_callback_).Run();
  }
  current_state_ = new_state;
}

void SpeechRecognitionPrivateRecognizer::HandleStart(
    absl::optional<std::string> locale,
    absl::optional<bool> interim_results,
    base::OnceClosure callback) {
  MaybeUpdateProperties(locale, interim_results, std::move(callback));

  if (speech_recognizer_) {
    // Create a new speech recognizer, since some properties, e.g. locale, could
    // have changed.
    current_state_ = SPEECH_RECOGNIZER_OFF;
    speech_recognizer_.reset();
  }

  // Choose which type of speech recognition, either on-device or network.
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (OnDeviceSpeechRecognizer::IsOnDeviceSpeechRecognizerAvailable(locale_)) {
    speech_recognizer_ = std::make_unique<OnDeviceSpeechRecognizer>(
        GetWeakPtr(), profile, locale_,
        /*recognition_mode_ime=*/true, /*enable_formatting=*/false);
  } else {
    speech_recognizer_ = std::make_unique<NetworkSpeechRecognizer>(
        GetWeakPtr(),
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcessIOThread(),
        profile->GetPrefs()->GetString(language::prefs::kAcceptLanguages),
        locale_);
  }
}

void SpeechRecognitionPrivateRecognizer::MaybeUpdateProperties(
    absl::optional<std::string> locale,
    absl::optional<bool> interim_results,
    base::OnceClosure callback) {
  if (locale.has_value())
    locale_ = locale.value();
  if (interim_results.has_value())
    interim_results_ = interim_results.value();
  on_start_callback_ = std::move(callback);
  DCHECK(!on_start_callback_.is_null());
}

}  // namespace extensions
