// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/cros_speech_recognition_service.h"

#include "chrome/services/speech/cros_speech_recognition_recognizer_impl.h"
#include "components/soda/constants.h"
#include "media/base/media_switches.h"

namespace speech {

CrosSpeechRecognitionService::CrosSpeechRecognitionService(
    content::BrowserContext* context)
    : ChromeSpeechRecognitionService(context),
      enable_soda_(
          base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)) {}

CrosSpeechRecognitionService::~CrosSpeechRecognitionService() {}

void CrosSpeechRecognitionService::Create(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  if (enable_soda_) {
    speech_recognition_contexts_.Add(this, std::move(receiver));
  } else {
    // If soda is not enabled, do the same thing as chrome.
    ChromeSpeechRecognitionService::Create(std::move(receiver));
  }
}

void CrosSpeechRecognitionService::BindRecognizer(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> client,
    BindRecognizerCallback callback) {
  // TODO(robsc): Create this with appropriate file locations.
  CrosSpeechRecognitionRecognizerImpl::Create(
      std::move(receiver), std::move(client), nullptr, base::FilePath(),
      base::FilePath());
  std::move(callback).Run(
      CrosSpeechRecognitionRecognizerImpl::IsMultichannelSupported());
}

}  // namespace speech
