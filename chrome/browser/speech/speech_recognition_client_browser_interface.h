// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "content/public/browser/global_routing_id.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/speech/speech_recognition_small_expert_model_installer.h"
#endif

class PrefChangeRegistrar;
class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace speech {

class SpeechRecognitionClientBrowserInterface
    : public KeyedService,
      public media::mojom::SpeechRecognitionClientBrowserInterface,
      public speech::SodaInstaller::Observer
#if !BUILDFLAG(IS_CHROMEOS)
    ,
      public speech::SpeechRecognitionSmallExpertModelInstaller::Observer
#endif
{
 public:
  explicit SpeechRecognitionClientBrowserInterface(
      content::BrowserContext* context);
  SpeechRecognitionClientBrowserInterface(
      const SpeechRecognitionClientBrowserInterface&) = delete;
  SpeechRecognitionClientBrowserInterface& operator=(
      const SpeechRecognitionClientBrowserInterface&) = delete;
  ~SpeechRecognitionClientBrowserInterface() override;

  // Binds the browser interface receiver for a specific renderer frame.
  void BindReceiver(
      mojo::PendingReceiver<
          media::mojom::SpeechRecognitionClientBrowserInterface> receiver,
      content::GlobalRenderFrameHostId frame_id);

  // Binds a speech recognition availability observer directly in the browser
  // process (pass an empty `frame_id` if not associated with a specific frame).
  void BindSpeechRecognitionBrowserObserver(
      mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
          pending_remote,
      content::GlobalRenderFrameHostId frame_id);

  // media::mojom::SpeechRecognitionClientBrowserInterface:
  // Binds a speech recognition availability observer from a renderer client
  // over Mojo (associating it with the frame bound via `BindReceiver()`).
  void BindSpeechRecognitionBrowserObserver(
      mojo::PendingRemote<media::mojom::SpeechRecognitionBrowserObserver>
          pending_remote) override;
  void REMOVED_1() override;
#if BUILDFLAG(IS_CHROMEOS)
  void REMOVED_2(
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
#if !BUILDFLAG(IS_CHROMEOS)
  void OnSpeechRecognitionSmallExpertModelStateChanged(
      speech::SpeechRecognitionSmallExpertModelInstaller::
          SpeechRecognitionSmallExpertModelState state) override;
#endif

  // Live Caption Event handling
  void OnLiveCaptionAvailabilityChanged();

 private:
  void OnSpeechRecognitionMaskOffensiveWordsChanged();
  void OnLiveCaptionLanguageChanged();

  // Notify our observers whether recognition is enabled or not, if the correct
  // language pack is installed.  If not, then defer until it is.
  void NotifyLiveCaptionObserversIfNeeded();
  void MaybeTriggerModelInstall();
  void OnObserverDisconnected(mojo::RemoteSetElementId id);
  bool IsTranscriptionAllowed(mojo::RemoteSetElementId id) const;

  mojo::RemoteSet<media::mojom::SpeechRecognitionBrowserObserver>
      live_caption_availability_observers_;
  std::map<mojo::RemoteSetElementId, content::GlobalRenderFrameHostId>
      observer_frame_ids_;

  mojo::ReceiverSet<media::mojom::SpeechRecognitionClientBrowserInterface,
                    content::GlobalRenderFrameHostId>
      speech_recognition_client_browser_interface_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  raw_ptr<PrefService> profile_prefs_;
  raw_ptr<content::BrowserContext> context_;

  base::ScopedObservation<speech::SodaInstaller,
                          speech::SodaInstaller::Observer>
      soda_installer_observation_{this};
#if !BUILDFLAG(IS_CHROMEOS)
  base::ScopedObservation<
      speech::SpeechRecognitionSmallExpertModelInstaller,
      speech::SpeechRecognitionSmallExpertModelInstaller::Observer>
      speech_recognition_small_expert_model_installer_observation_{this};
#endif
};

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_H_
