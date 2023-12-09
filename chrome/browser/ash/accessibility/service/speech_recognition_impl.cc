// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/speech_recognition_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_manager.h"
#include "chrome/browser/ash/extensions/speech/speech_recognition_private_recognizer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"

namespace ash {

namespace {
ax::mojom::SpeechRecognitionType ToMojo(speech::SpeechRecognitionType type) {
  switch (type) {
    case speech::SpeechRecognitionType::kNetwork:
      return ax::mojom::SpeechRecognitionType::kNetwork;
    case speech::SpeechRecognitionType::kOnDevice:
      return ax::mojom::SpeechRecognitionType::kOnDevice;
  }
}
}  // namespace

//
// SpeechRecognitionEventObserverWrapper implementation.
//

SpeechRecognitionImpl::SpeechRecognitionEventObserverWrapper::
    SpeechRecognitionEventObserverWrapper() = default;
SpeechRecognitionImpl::SpeechRecognitionEventObserverWrapper::
    ~SpeechRecognitionEventObserverWrapper() = default;

void SpeechRecognitionImpl::SpeechRecognitionEventObserverWrapper::OnStop() {
  observer_->OnStop();
}

void SpeechRecognitionImpl::SpeechRecognitionEventObserverWrapper::OnResult(
    ax::mojom::SpeechRecognitionResultEventPtr event) {
  observer_->OnResult(std::move(event));
}

void SpeechRecognitionImpl::SpeechRecognitionEventObserverWrapper::OnError(
    ax::mojom::SpeechRecognitionErrorEventPtr event) {
  observer_->OnError(std::move(event));
}

mojo::PendingReceiver<ax::mojom::SpeechRecognitionEventObserver>
SpeechRecognitionImpl::SpeechRecognitionEventObserverWrapper::PassReceiver() {
  return observer_.BindNewPipeAndPassReceiver();
}

//
// SpeechRecognitionImpl implementation.
//

SpeechRecognitionImpl::SpeechRecognitionImpl(content::BrowserContext* profile)
    : profile_(profile) {
  CHECK(profile_);
}

SpeechRecognitionImpl::~SpeechRecognitionImpl() = default;

void SpeechRecognitionImpl::Bind(
    mojo::PendingReceiver<ax::mojom::SpeechRecognition> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SpeechRecognitionImpl::Start(ax::mojom::StartOptionsPtr options,
                                  StartCallback callback) {
  // Extract arguments.
  std::optional<std::string> locale;
  std::optional<bool> interim_results;
  if (options->locale) {
    locale = *options->locale;
  }
  if (options->interim_results) {
    interim_results = *options->interim_results;
  }

  std::string key = CreateKey(options->type);
  GetSpeechRecognizer(key)->HandleStart(
      locale, interim_results,
      base::BindOnce(&SpeechRecognitionImpl::StartHelper, GetWeakPtr(),
                     std::move(callback), key));
}

void SpeechRecognitionImpl::Stop(ax::mojom::StopOptionsPtr options,
                                 StopCallback callback) {
  std::string key = CreateKey(options->type);
  GetSpeechRecognizer(key)->HandleStop(
      base::BindOnce(&SpeechRecognitionImpl::StopHelper, GetWeakPtr(),
                     std::move(callback), key));
}

void SpeechRecognitionImpl::HandleSpeechRecognitionStopped(
    const std::string& key) {
  auto* observer = GetEventObserverWrapper(key);
  if (!observer) {
    return;
  }

  observer->OnStop();
  RemoveEventObserverWrapper(key);
}

void SpeechRecognitionImpl::HandleSpeechRecognitionResult(
    const std::string& key,
    const std::u16string& transcript,
    bool is_final) {
  auto* observer = GetEventObserverWrapper(key);
  if (!observer) {
    return;
  }

  auto result = ax::mojom::SpeechRecognitionResultEvent::New();
  result->transcript = base::UTF16ToUTF8(transcript);
  result->is_final = is_final;
  observer->OnResult(std::move(result));
}

void SpeechRecognitionImpl::HandleSpeechRecognitionError(
    const std::string& key,
    const std::string& error) {
  auto* observer = GetEventObserverWrapper(key);
  if (!observer) {
    return;
  }

  auto event = ax::mojom::SpeechRecognitionErrorEvent::New();
  event->message = error;
  observer->OnError(std::move(event));
  RemoveEventObserverWrapper(key);
}

void SpeechRecognitionImpl::StartHelper(StartCallback callback,
                                        const std::string& key,
                                        speech::SpeechRecognitionType type,
                                        std::optional<std::string> error) {
  // Send relevant information back to the caller.
  auto info = ax::mojom::SpeechRecognitionStartInfo::New();
  info->type = ToMojo(type);
  if (error.has_value()) {
    info->observer_or_error =
        ax::mojom::ObserverOrError::NewError(error.value());
    std::move(callback).Run(std::move(info));
    return;
  }

  CreateEventObserverWrapper(key);
  auto* observer = GetEventObserverWrapper(key);
  info->observer_or_error =
      ax::mojom::ObserverOrError::NewObserver(observer->PassReceiver());
  std::move(callback).Run(std::move(info));
}

void SpeechRecognitionImpl::StopHelper(StopCallback callback,
                                       const std::string& key,
                                       std::optional<std::string> error) {
  RemoveEventObserverWrapper(key);
  if (error.has_value()) {
    std::move(callback).Run(std::move(error));
    return;
  }

  std::move(callback).Run(std::optional<std::string>());
}

std::string SpeechRecognitionImpl::CreateKey(
    ax::mojom::AssistiveTechnologyType type) {
  // Dictation is the only accessibility feature that uses speech recognition.
  // In the future, we can add support other accessibility features e.g.
  // Voice Access.
  DCHECK(type == ax::mojom::AssistiveTechnologyType::kDictation);
  return "AtpSpeechRecognition.Dictation";
}

extensions::SpeechRecognitionPrivateRecognizer*
SpeechRecognitionImpl::GetSpeechRecognizer(const std::string& key) {
  auto& recognizer = recognizers_[key];
  if (!recognizer) {
    recognizer =
        std::make_unique<extensions::SpeechRecognitionPrivateRecognizer>(
            this, profile_, key);
  }

  return recognizer.get();
}

void SpeechRecognitionImpl::CreateEventObserverWrapper(const std::string& key) {
  auto& observer = event_observer_wrappers_[key];
  if (!observer) {
    observer = std::make_unique<SpeechRecognitionEventObserverWrapper>();
  }
}

SpeechRecognitionImpl::SpeechRecognitionEventObserverWrapper*
SpeechRecognitionImpl::GetEventObserverWrapper(const std::string& key) {
  auto& observer = event_observer_wrappers_[key];
  if (!observer) {
    return nullptr;
  }

  return observer.get();
}

void SpeechRecognitionImpl::RemoveEventObserverWrapper(const std::string& key) {
  auto& observer = event_observer_wrappers_[key];
  if (observer) {
    observer.reset();
  }

  event_observer_wrappers_.erase(key);
}

}  // namespace ash
