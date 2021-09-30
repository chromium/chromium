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

namespace {
const char kSpeechRecognitionOffError[] = "Speech recognition already stopped";
const char kSpeechRecognitionError[] = "A speech recognition error occurred";
}  // namespace

namespace extensions {

SpeechRecognitionPrivateRecognizer::SpeechRecognitionPrivateRecognizer(
    base::RepeatingClosure on_stop_callback,
    OnResultCallback on_result_callback,
    OnErrorCallback on_error_callback)
    : on_stop_callback_(std::move(on_stop_callback)),
      on_result_callback_(std::move(on_result_callback)),
      on_error_callback_(std::move(on_error_callback)) {}

SpeechRecognitionPrivateRecognizer::~SpeechRecognitionPrivateRecognizer() {}

void SpeechRecognitionPrivateRecognizer::OnSpeechResult(
    const std::u16string& text,
    bool is_final,
    const absl::optional<media::SpeechRecognitionResult>& full_result) {
  // TODO(crbug.com/1220107): NetworkSpeechRecognizer adds spaces between
  // results, but OnDeviceSpeechRecognizer doesn't. Add behavior in
  // OnDeviceSpeechRecognizer so it's consistent with NetworkSpeechRecognizer.
  if (!interim_results_ && !is_final) {
    // If |interim_results_| is false, then don't run on_result_callback_
    // unless this is a final result.
    return;
  }

  DCHECK(!on_result_callback_.is_null());
  on_result_callback_.Run(text, is_final);
}

void SpeechRecognitionPrivateRecognizer::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  SpeechRecognizerStatus next_state = new_state;
  if (new_state == SPEECH_RECOGNIZER_READY) {
    if (current_state_ == SPEECH_RECOGNIZER_OFF && speech_recognizer_) {
      // The SpeechRecognizer is ready to start recognizing speech.
      speech_recognizer_->Start();
    } else {
      // Turn the recognizer off and run on_stop_callback_ to notify
      // listeners of the API that speech recognition has stopped.
      next_state = SPEECH_RECOGNIZER_OFF;
      RecognizerOff();
      DCHECK(!on_stop_callback_.is_null());
      on_stop_callback_.Run();
    }
  } else if (new_state == SPEECH_RECOGNIZER_RECOGNIZING) {
    DCHECK(!on_start_callback_.is_null());
    std::move(on_start_callback_).Run();
  } else if (new_state == SPEECH_RECOGNIZER_ERROR) {
    // When a speech recognition error occurs, run callbacks for both error and
    // stop.
    next_state = SPEECH_RECOGNIZER_OFF;
    RecognizerOff();
    DCHECK(!on_error_callback_.is_null());
    on_error_callback_.Run(kSpeechRecognitionError);
    DCHECK(!on_stop_callback_.is_null());
    on_stop_callback_.Run();
  }
  current_state_ = next_state;
}

void SpeechRecognitionPrivateRecognizer::HandleStart(
    absl::optional<std::string> locale,
    absl::optional<bool> interim_results,
    base::OnceClosure callback) {
  MaybeUpdateProperties(locale, interim_results, std::move(callback));

  if (speech_recognizer_) {
    // Create a new speech recognizer, since some properties, e.g. locale, could
    // have changed.
    RecognizerOff();
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

void SpeechRecognitionPrivateRecognizer::HandleStop(
    base::OnceCallback<void(absl::optional<std::string>)> callback) {
  if (current_state_ == SPEECH_RECOGNIZER_OFF) {
    // If speech recognition is already off, trigger the callback with an error
    // message.
    std::move(callback).Run(
        /*error=*/absl::optional<std::string>(kSpeechRecognitionOffError));
    return;
  }

  RecognizerOff();

  DCHECK(!on_stop_callback_.is_null());
  on_stop_callback_.Run();

  DCHECK(!callback.is_null());
  std::move(callback).Run(/*error=*/absl::optional<std::string>());
}

void SpeechRecognitionPrivateRecognizer::RecognizerOff() {
  current_state_ = SPEECH_RECOGNIZER_OFF;
  if (speech_recognizer_)
    speech_recognizer_.reset();
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
