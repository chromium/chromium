// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_CAPTION_HOST_IMPL_H_
#define CHROME_BROWSER_ACCESSIBILITY_CAPTION_HOST_IMPL_H_

#include "content/public/browser/web_contents_observer.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace captions {

class CaptionController;

///////////////////////////////////////////////////////////////////////////////
// Caption Host Impl
//
//  A class that implements the Mojo interface
//  SpeechRecognitionRecognizerClient. There exists one CaptionHostImpl per
//  render frame.
//
class CaptionHostImpl : public media::mojom::SpeechRecognitionRecognizerClient,
                        public content::WebContentsObserver {
 public:
  explicit CaptionHostImpl(content::RenderFrameHost* frame_host);
  CaptionHostImpl(const CaptionHostImpl&) = delete;
  CaptionHostImpl& operator=(const CaptionHostImpl&) = delete;
  ~CaptionHostImpl() override;

  // static
  static void Create(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
          receiver);

  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      media::mojom::SpeechRecognitionResultPtr result,
      OnSpeechRecognitionRecognitionEventCallback reply) override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;
  void OnSpeechRecognitionError() override;

  // Returns the WebContents if it exists. If it does not exist, sets the
  // RenderFrameHost reference to nullptr and returns nullptr.
  content::WebContents* GetWebContents();

 protected:
  // content::WebContentsObserver:
  void RenderFrameDeleted(content::RenderFrameHost* frame_host) override;

 private:
  // Returns the CaptionController for frame_host_. Returns nullptr if it does
  // not exist.
  CaptionController* GetCaptionController();

  content::RenderFrameHost* frame_host_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_CAPTION_HOST_IMPL_H_
