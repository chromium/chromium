// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/speech_recognition_ash.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#include "chrome/browser/speech/speech_recognition_service.h"
#include "components/live_caption/caption_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

SpeechRecognitionAsh::SpeechRecognitionAsh() = default;
SpeechRecognitionAsh::~SpeechRecognitionAsh() = default;

void SpeechRecognitionAsh::BindReceiver(
    mojo::PendingReceiver<mojom::SpeechRecognition> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void SpeechRecognitionAsh::BindSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  if (captions::IsLiveCaptionFeatureSupported()) {
    CrosSpeechRecognitionServiceFactory::GetForProfile(
        ProfileManager::GetPrimaryUserProfile())
        ->BindSpeechRecognitionContext(std::move(receiver));
  }
}

void SpeechRecognitionAsh::BindSpeechRecognitionClientBrowserInterface(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionClientBrowserInterface>
        receiver) {
  if (captions::IsLiveCaptionFeatureSupported()) {
    SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(
        ProfileManager::GetPrimaryUserProfile())
        ->BindReceiver(std::move(receiver));
  }
}

}  // namespace crosapi
