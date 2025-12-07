// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_IMPL_H_
#define CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_IMPL_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/document_user_data.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

#if !BUILDFLAG(IS_ANDROID)
#include <list>

#include "base/containers/flat_map.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/soda/soda_installer.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace content {
class RenderFrameHost;
}  // namespace content

namespace optimization_guide {
class ModelBrokerClient;
}  // namespace optimization_guide

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
  void Available(
      const std::vector<std::string>& languages,
      OnDeviceSpeechRecognitionImpl::AvailableCallback callback) override;
  void Install(
      const std::vector<std::string>& languages,
      OnDeviceSpeechRecognitionImpl::InstallCallback callback) override;

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
      const std::vector<std::string>& languages,
      OnDeviceSpeechRecognitionImpl::InstallCallback callback);
  void ProcessLanguageInstallationUpdate(const std::string& language,
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
  base::TimeDelta GetDownloadDelay(const std::vector<std::string>& languages);

  void OnModelClientAvailable(
      base::WeakPtr<optimization_guide::ModelClient> client);

  // A set of languages that have been downloaded for the current document. This
  // is used for origins that cannot persist content settings, e.g. opaque
  // origins or file schemes.
  std::set<std::string> transient_on_device_languages_downloaded_;

  base::flat_map<std::set<std::string>, std::list<InstallCallback>>
      language_installation_callbacks_;

  std::unique_ptr<optimization_guide::ModelBrokerClient> model_broker_client_;
#endif  // !BUILDFLAG(IS_ANDROID)

  mojo::Receiver<media::mojom::OnDeviceSpeechRecognition> receiver_{this};

  base::WeakPtrFactory<OnDeviceSpeechRecognitionImpl> weak_ptr_factory_{this};
  DOCUMENT_USER_DATA_KEY_DECL();
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_IMPL_H_
