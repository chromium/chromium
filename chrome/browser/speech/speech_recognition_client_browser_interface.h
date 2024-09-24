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
#include "mojo/public/cpp/bindings/pending_remote.h"
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void BindBabelOrcaSpeechRecognitionBrowserObserver(
      mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
          pending_remote) override;
#endif
  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override {}
  void OnSodaInstallError(
      speech::LanguageCode language_code,
      speech::SodaInstaller::ErrorCode error_code) override {}
  // You can only toggle BabelOrca on ash.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ChangeBabelOrcaSpeechRecognitionAvailability(bool enabled);
#endif
 private:
  // Availability state represents the potential state of a given feature
  // whether that be LiveCaption or BabelOrca. This helps us simplify The
  // logic around determining whether the proper preconditions have been
  // satisfied in order to notify listeners.
  enum class AvailabilityState {
    // The feature is ready, SODA is installed and the feature's state
    // has been toggled to enabled.
    kReady,
    // The feature is waiting on SODA to install.
    kWaiting,
    // The feature is disabled.
    kDisabled,
  };

  // The Availability struct holds the potential state of both features.
  // By default each feature is set to disabled, but we define here a set
  // of methods to help us determine what the state of a feature should be.
  // On an event that may update a feature's availability we take in a new
  // instance of this struct and pass it through a series of helpers that
  // determine if the feature should be enabled.  For more information
  // on the necessary preconditions see go/babel-orca-mic-integration
  // subsection Detailed Design -> live caption changes
  // -> speech recognition client browser interface changes
  // Managing State.
  struct Availability {
    AvailabilityState live_caption_availability = AvailabilityState::kDisabled;
    AvailabilityState babel_orca_availability = AvailabilityState::kDisabled;

    bool IsLiveCaptionAvailable() const {
      return live_caption_availability == AvailabilityState::kReady;
    }
    bool IsBabelOrcaAvailable() const {
      return babel_orca_availability == AvailabilityState::kReady;
    }
    bool IsWaitingOnLiveCaption() const {
      return live_caption_availability == AvailabilityState::kWaiting;
    }
    bool IsWaitingOnBabelOrca() const {
      return babel_orca_availability == AvailabilityState::kWaiting;
    }
    bool LiveCaptionChanged(const Availability& other) const {
      return live_caption_availability != other.live_caption_availability;
    }
    bool BabelOrcaChanged(const Availability& other) const {
      return babel_orca_availability != other.babel_orca_availability;
    }
  };
  Availability CopyAvailabilityAndFlipLiveCaption(bool enabled);
  Availability CopyAvailabilityAndFlipBabelOrca(bool enabled);

  void OnLiveCaptionPrefChange();
  void OnSpeechRecognitionAvailabilityChanged(
      const Availability& new_availability);
  void OnLiveCaptionLanguageChanged();
  void OnBabelOrcaLanguageChanged();
  void OnSpeechRecognitionMaskOffensiveWordsChanged();
  void NotifyObservers(const Availability& new_availability);
  void NotifyLiveCaptionObservers(const Availability& new_availability);
  void NotifyBabelOrcaObservers(const Availability& new_availability);
  bool IsBabelOrcaSodaPackAvailable();
  bool IsLiveCaptionSodaPackAvailable();

  mojo::RemoteSet<media::mojom::SpeechRecognitionBrowserObserver>
      speech_recognition_availibility_observers_;

  mojo::RemoteSet<media::mojom::SpeechRecognitionBrowserObserver>
      babel_orca_speech_recognition_availability_observers_;

  mojo::ReceiverSet<media::mojom::SpeechRecognitionClientBrowserInterface>
      speech_recognition_client_browser_interface_;

  mojo::UniqueReceiverSet<media::mojom::SpeechRecognitionRecognizerClient>
      ui_drivers_;

  Availability availability_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  raw_ptr<PrefService> profile_prefs_;
  raw_ptr<captions::LiveCaptionController> controller_;
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_H_
