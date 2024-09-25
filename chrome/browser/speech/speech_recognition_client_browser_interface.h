// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

class PrefChangeRegistrar;
class PrefService;

namespace captions {
class LiveCaptionController;
}  // namespace captions

namespace content {
class BrowserContext;
}  // namespace content

namespace speech {

class SpeechRecognitionClientBrowserInterface
    : public KeyedService,
      public media::mojom::SpeechRecognitionClientBrowserInterface,
      public speech::SodaInstaller::Observer {
 public:
  explicit SpeechRecognitionClientBrowserInterface(
      content::BrowserContext* context);
  SpeechRecognitionClientBrowserInterface(
      const SpeechRecognitionClientBrowserInterface&) = delete;
  SpeechRecognitionClientBrowserInterface& operator=(
      const SpeechRecognitionClientBrowserInterface&) = delete;
  ~SpeechRecognitionClientBrowserInterface() override;

  void BindReceiver(
      mojo::PendingReceiver<
          media::mojom::SpeechRecognitionClientBrowserInterface> receiver);

  // media::mojom::SpeechRecognitionClientBrowserInterface
  void BindSpeechRecognitionBrowserObserver(
      mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
          pending_remote) override;
  void BindRecognizerToRemoteClient(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
          client_receiver,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSurfaceClient>
          host_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSurface> origin_remote,
      media::mojom::SpeechRecognitionSurfaceMetadataPtr metadata) override;

  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override {}
  void OnSodaInstallError(
      speech::LanguageCode language_code,
      speech::SodaInstaller::ErrorCode error_code) override {}

 private:
  void OnSpeechRecognitionAvailabilityChanged();
  void OnSpeechRecognitionLanguageChanged();
  void OnSpeechRecognitionMaskOffensiveWordsChanged();
  void NotifyObservers(bool enabled);

  mojo::RemoteSet<media::mojom::SpeechRecognitionBrowserObserver>
      speech_recognition_availibility_observers_;

  mojo::ReceiverSet<media::mojom::SpeechRecognitionClientBrowserInterface>
      speech_recognition_client_browser_interface_;

  mojo::UniqueReceiverSet<media::mojom::SpeechRecognitionRecognizerClient>
      ui_drivers_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  raw_ptr<PrefService> profile_prefs_;
  raw_ptr<captions::LiveCaptionController> controller_;
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_H_
