// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SPEECH_RECOGNITION_HOST_H_
#define CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SPEECH_RECOGNITION_HOST_H_

#include <memory>

#include "build/build_config.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace captions {

class CaptionBubbleContextBrowser;
class LiveCaptionController;

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
  explicit LiveCaptionSpeechRecognitionHost(
      content::RenderFrameHost& frame_host,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
          pending_receiver);
  ~LiveCaptionSpeechRecognitionHost() override;

  // Returns the WebContents if it exists. If it does not exist, sets the
  // RenderFrameHost reference to nullptr and returns nullptr.
  content::WebContents* GetWebContents();

  // Returns the LiveCaptionController for frame_host_. Returns nullptr if it
  // does not exist.
  LiveCaptionController* GetLiveCaptionController();

  std::unique_ptr<CaptionBubbleContextBrowser> context_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SPEECH_RECOGNITION_HOST_H_
