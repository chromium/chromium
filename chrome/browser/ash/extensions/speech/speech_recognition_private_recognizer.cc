// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/speech/speech_recognition_private_recognizer.h"

#include "ash/public/cpp/projector/speech_recognition_availability.h"
#include "base/debug/crash_logging.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/network_speech_recognizer.h"
#include "chrome/browser/speech/speech_recognition_recognizer_client_impl.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "media/audio/audio_device_description.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"

namespace {
const char kSpeechRecognitionError[] = "A speech recognition error occurred";
const char kSpeechRecognitionNeverStartedError[] =
    "Speech recognition never started";
const char kSpeechRecognitionStartError[] =
    "Speech recognition already started";
const char kSpeechRecognitionStopError[] = "Speech recognition already stopped";
}  // namespace

namespace extensions {

SpeechRecognitionPrivateRecognizer::SpeechRecognitionPrivateRecognizer(
    SpeechRecognitionPrivateDelegate* delegate,
    content::BrowserContext* context,
    const std::string& id)
    : delegate_(delegate), context_(context), id_(id) {
  DCHECK(delegate);
}

SpeechRecognitionPrivateRecognizer::~SpeechRecognitionPrivateRecognizer() {}

void SpeechRecognitionPrivateRecognizer::OnSpeechResult(
    const std::u16string& text,
    bool is_final,
    const std::optional<media::SpeechRecognitionResult>& full_result) {
  // TODO(crbug.com/40186322): NetworkSpeechRecognizer adds spaces between
  // results, but SpeechRecognitionRecognizerClientImpl doesn't. Add behavior in
  // SpeechRecognitionRecognizerClientImpl so it's consistent with
  // NetworkSpeechRecognizer.
  if (!interim_results_ && !is_final) {
    // If |interim_results_| is false, then don't handle the result unless this
    // is a final result.
    return;
  }

  delegate_->HandleSpeechRecognitionResult(id_, text, is_final);
}

void SpeechRecognitionPrivateRecognizer::OnSpeechRecognitionStateChanged(
    SpeechRecognizerStatus new_state) {
  // Crash keys for https://crbug.com/1296304.
  SCOPED_CRASH_KEY_NUMBER("Accessibility", "Speech recognition state",
                          new_state);
  SpeechRecognizerStatus next_state = new_state;
  if (new_state == SPEECH_RECOGNIZER_READY) {
    if (current_state_ == SPEECH_RECOGNIZER_OFF && speech_recognizer_) {
      // The SpeechRecognizer is ready to start recognizing speech.
      speech_recognizer_->Start();
    } else {
      // Turn the recognizer off and use |delegate_| to notify listeners of the
      // API that speech recognition has stopped.
      next_state = SPEECH_RECOGNIZER_OFF;
      RecognizerOff();
      delegate_->HandleSpeechRecognitionStopped(id_);
    }
  } else if (new_state == SPEECH_RECOGNIZER_RECOGNIZING) {
    if (!on_start_callback_.is_null()) {
      std::move(on_start_callback_)
          .Run(/*type=*/type_, /*error=*/std::optional<std::string>());
    } else {
      // If we get here, we are unintentionally recognizing speech. Turn off
      // the recognizer.
      next_state = SPEECH_RECOGNIZER_OFF;
      RecognizerOff();
    }
  } else if (new_state == SPEECH_RECOGNIZER_ERROR) {
    // When a speech recognition error occurs, ask the delegate to handle both
    // error and stop events.
    next_state = SPEECH_RECOGNIZER_OFF;
    RecognizerOff();
    delegate_->HandleSpeechRecognitionError(id_, kSpeechRecognitionError);
    delegate_->HandleSpeechRecognitionStopped(id_);
  }
  current_state_ = next_state;
}

void SpeechRecognitionPrivateRecognizer::HandleStart(
    std::optional<std::string> locale,
    std::optional<bool> interim_results,
    OnStartCallback callback) {
  if (speech_recognizer_) {
    std::move(callback).Run(
        /*type=*/type_,
        /*error=*/std::optional<std::string>(kSpeechRecognitionStartError));
    RecognizerOff();
    return;
  }

  MaybeUpdateProperties(locale, interim_results, std::move(callback));

  // Choose which type of speech recognition, either on-device or network.
  Profile* profile = Profile::FromBrowserContext(context_);
  if (SpeechRecognitionRecognizerClientImpl::
          GetOnDeviceSpeechRecognitionAvailability(locale_) ==
      ash::OnDeviceRecognitionAvailability::kAvailable) {
    type_ = speech::SpeechRecognitionType::kOnDevice;
    speech_recognizer_ =
        std::make_unique<SpeechRecognitionRecognizerClientImpl>(
            GetWeakPtr(), profile,
            media::AudioDeviceDescription::kDefaultDeviceId,
            media::mojom::SpeechRecognitionOptions::New(
                media::mojom::SpeechRecognitionMode::kIme,
                /*enable_formatting=*/false,
                /*language=*/locale_,
                /*is_server_based=*/false,
                media::mojom::RecognizerClientType::kDictation));
  } else {
    type_ = speech::SpeechRecognitionType::kNetwork;
    speech_recognizer_ = std::make_unique<NetworkSpeechRecognizer>(
        GetWeakPtr(),
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcessIOThread(),
        profile->GetPrefs()->GetString(language::prefs::kAcceptLanguages),
        locale_);
  }
}

void SpeechRecognitionPrivateRecognizer::HandleStop(OnStopCallback callback) {
  bool has_error = false;
  if (!on_start_callback_.is_null()) {
    // If speech recognition never successfully started, then run the necessary
    // callbacks with an error to avoid dangling callbacks (Chrome
    // will crash if an extension function is destroyed and hasn't responded).
    has_error = true;
    std::move(on_start_callback_)
        .Run(/*type=*/type_, /*error=*/std::optional<std::string>(
                 kSpeechRecognitionNeverStartedError));
    std::move(callback).Run(
        /*error=*/std::optional<std::string>(kSpeechRecognitionStopError));
  } else if (current_state_ == SPEECH_RECOGNIZER_OFF) {
    // If speech recognition is already off, run `callback` with an error
    // message.
    has_error = true;
    std::move(callback).Run(
        /*error=*/std::optional<std::string>(kSpeechRecognitionStopError));
  }

  RecognizerOff();
  if (has_error)
    return;

  delegate_->HandleSpeechRecognitionStopped(id_);
  DCHECK(!callback.is_null());
  std::move(callback).Run(/*error=*/std::optional<std::string>());
}

void SpeechRecognitionPrivateRecognizer::RecognizerOff() {
  current_state_ = SPEECH_RECOGNIZER_OFF;
  if (speech_recognizer_)
    speech_recognizer_.reset();
}

void SpeechRecognitionPrivateRecognizer::MaybeUpdateProperties(
    std::optional<std::string> locale,
    std::optional<bool> interim_results,
    OnStartCallback callback) {
  if (locale.has_value())
    locale_ = locale.value();
  if (interim_results.has_value())
    interim_results_ = interim_results.value();
  on_start_callback_ = std::move(callback);
  DCHECK(!on_start_callback_.is_null());
}

}  // namespace extensions
