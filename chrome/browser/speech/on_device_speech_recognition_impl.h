// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_IMPL_H_
#define CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_IMPL_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/document_user_data.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

#if !BUILDFLAG(IS_ANDROID)
#include <list>

#include "base/containers/flat_map.h"
#include "components/soda/soda_installer.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {
class RenderFrameHost;
}  // namespace content

namespace speech {

class OnDeviceSpeechRecognitionImpl
    : public content::DocumentUserData<OnDeviceSpeechRecognitionImpl>,
#if !BUILDFLAG(IS_ANDROID)
      public speech::SodaInstaller::Observer,
#endif  // !BUILDFLAG(IS_ANDROID)
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

#if !BUILDFLAG(IS_ANDROID)
  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int combined_progress) override {}
#endif  // !BUILDFLAG(IS_ANDROID)

 private:
  friend class content::DocumentUserData<OnDeviceSpeechRecognitionImpl>;
  explicit OnDeviceSpeechRecognitionImpl(content::RenderFrameHost* frame_host);

  // Returns whether the render frame host can use on-device speech recognition.
  // HTTP(s) origins not scoped to the default storage partition may not use
  // on-device speech recognition.
  bool CanRenderFrameHostUseOnDeviceSpeechRecognition();

#if !BUILDFLAG(IS_ANDROID)
  void InstallLanguageInternal(
      const std::string& language,
      OnDeviceSpeechRecognitionImpl::InstallOnDeviceSpeechRecognitionCallback
          callback);
  void RunAndRemoveInstallationCallbacks(const std::string& language,
                                         bool installation_success);
  base::Value GetOnDeviceLanguagesDownloadedValue();
  void SetOnDeviceLanguagesDownloadedContentSetting(
      base::Value on_device_languages_downloaded);
  bool HasOnDeviceLanguageDownloaded(const std::string& language);
  void SetOnDeviceLanguageDownloaded(const std::string&);

  // Mask on-device speech recognition availability by requiring a call to
  // installOnDevice() for a language before the language is available to the
  // origin.
  media::mojom::AvailabilityStatus GetMaskedAvailabilityStatus(
      const std::string& language);

  // Returns a delay when installing on-device speech recognition language packs
  // to safeguard against fingerprinting resulting from timing the installation.
  base::TimeDelta GetDownloadDelay(const std::string& language);

  base::flat_map<std::string,
                 std::list<InstallOnDeviceSpeechRecognitionCallback>>
      language_installation_callbacks_;
#endif  // !BUILDFLAG(IS_ANDROID)

  mojo::Receiver<media::mojom::OnDeviceSpeechRecognition> receiver_{this};

  base::WeakPtrFactory<OnDeviceSpeechRecognitionImpl> weak_ptr_factory_{this};
  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_IMPL_H_
