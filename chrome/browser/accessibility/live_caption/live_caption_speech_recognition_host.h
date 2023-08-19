// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SPEECH_RECOGNITION_HOST_H_
#define CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SPEECH_RECOGNITION_HOST_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class PrefService;

namespace content {
class RenderFrameHost;
}

namespace captions {

class CaptionBubbleContextBrowser;
class LiveCaptionController;
class LiveTranslateController;

///////////////////////////////////////////////////////////////////////////////
//  Live Caption Speech Recognition Host
//
//  A class that implements the Mojo interface
//  SpeechRecognitionRecognizerClient. There exists one
//  LiveCaptionSpeechRecognitionHost per render frame.
//
class LiveCaptionSpeechRecognitionHost
    : public content::DocumentService<
          media::mojom::SpeechRecognitionRecognizerClient>,
      public content::WebContentsObserver {
 public:
  LiveCaptionSpeechRecognitionHost(const LiveCaptionSpeechRecognitionHost&) =
      delete;
  LiveCaptionSpeechRecognitionHost& operator=(
      const LiveCaptionSpeechRecognitionHost&) = delete;

  // static
  static void Create(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
          receiver);

  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;
  void OnSpeechRecognitionError() override;
  void OnSpeechRecognitionStopped() override;

 protected:
  // Mac and ChromeOS move the fullscreened window into a new workspace. When
  // the WebContents associated with this RenderFrameHost goes fullscreen,
  // ensure that the Live Caption bubble moves to the new workspace.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  void MediaEffectivelyFullscreenChanged(bool is_fullscreen) override;
#endif

 private:
  LiveCaptionSpeechRecognitionHost(
      content::RenderFrameHost& frame_host,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
          pending_receiver);
  ~LiveCaptionSpeechRecognitionHost() override;
  void OnTranslationCallback(const std::string& cached_translation,
                             const std::string& original_transcription,
                             const std::string& source_language,
                             const std::string& target_language,
                             bool is_final,
                             const std::string& result);

  // Returns the WebContents if it exists. If it does not exist, sets the
  // RenderFrameHost reference to nullptr and returns nullptr.
  content::WebContents* GetWebContents();

  // Returns the LiveCaptionController for frame_host_. Returns nullptr if it
  // does not exist. Lifetime is tied to the BrowserContext.
  LiveCaptionController* GetLiveCaptionController();

  // Returns the LiveTranslateController for frame_host_. Returns nullptr if it
  // does not exist. Lifetime is tied to the BrowserContext.
  LiveTranslateController* GetLiveTranslateController();

  std::unique_ptr<CaptionBubbleContextBrowser> context_;

  // A flag used by the Live Translate feature indicating whether transcriptions
  // should stop.
  bool stop_transcriptions_ = false;

  // Used to cache translations to avoid retranslating the same string. The key
  // is the source and target language codes followed by a `|` separator
  // character and the original text. The value is the translated text. This
  // cache is cleared upon receiving a final recognition event. The size of this
  // cache depends on the frequency of partial and final recognition events, but
  // is typically under ~10.
  std::unordered_map<std::string, std::string> translation_cache_;

  // The source language code of the audio stream.
  std::string source_language_;

  // The user preferences containing the target and source language codes.
  raw_ptr<PrefService> prefs_;

  // The number of characters sent to the translation service.
  int characters_translated_ = 0;

  base::WeakPtrFactory<LiveCaptionSpeechRecognitionHost> weak_factory_{this};
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SPEECH_RECOGNITION_HOST_H_
