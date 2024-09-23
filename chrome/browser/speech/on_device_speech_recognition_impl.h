// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_IMPL_H_
#define CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_IMPL_H_

#include <string>

#include "content/public/browser/document_user_data.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class PrefService;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace language {
class LanguagePrefs;
}  // namespace language

namespace speech {

class OnDeviceSpeechRecognitionImpl
    : public content::DocumentUserData<OnDeviceSpeechRecognitionImpl>,
      public media::mojom::OnDeviceSpeechRecognition {
 public:
  OnDeviceSpeechRecognitionImpl(const OnDeviceSpeechRecognitionImpl&) = delete;
  OnDeviceSpeechRecognitionImpl& operator=(
      const OnDeviceSpeechRecognitionImpl&) = delete;

  ~OnDeviceSpeechRecognitionImpl() override;

  void Bind(
      mojo::PendingReceiver<media::mojom::OnDeviceSpeechRecognition> receiver);

  // speech::mojom::OnDeviceSpeechRecognition methods:
  void OnDeviceWebSpeechAvailable(
      const std::string& language,
      OnDeviceSpeechRecognitionImpl::OnDeviceWebSpeechAvailableCallback
          callback) override;
  void InstallOnDeviceSpeechRecognition(
      const std::string& language,
      OnDeviceSpeechRecognitionImpl::InstallOnDeviceSpeechRecognitionCallback
          callback) override;

 private:
  friend class content::DocumentUserData<OnDeviceSpeechRecognitionImpl>;
  explicit OnDeviceSpeechRecognitionImpl(content::RenderFrameHost* frame_host);

  // Returns whether or not a given language pack can be installed without
  // explicit user consent.
  bool CanInstallWithoutUserConsent(const std::string& language);

  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<language::LanguagePrefs> language_prefs_;

  mojo::Receiver<media::mojom::OnDeviceSpeechRecognition> receiver_{this};

  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_IMPL_H_
