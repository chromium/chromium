// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption_speech_recognition_host.h"

#include <memory>
#include <utility>

#include "chrome/browser/accessibility/live_caption_controller.h"
#include "chrome/browser/accessibility/live_caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace captions {

// static
void LiveCaptionSpeechRecognitionHost::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<LiveCaptionSpeechRecognitionHost>(frame_host),
      std::move(receiver));
}

LiveCaptionSpeechRecognitionHost::LiveCaptionSpeechRecognitionHost(
    content::RenderFrameHost* frame_host)
    : frame_host_(frame_host) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  Observe(web_contents);
}

LiveCaptionSpeechRecognitionHost::~LiveCaptionSpeechRecognitionHost() {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnAudioStreamEnd(this);
}

void LiveCaptionSpeechRecognitionHost::OnSpeechRecognitionRecognitionEvent(
    const media::SpeechRecognitionResult& result,
    OnSpeechRecognitionRecognitionEventCallback reply) {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (!live_caption_controller) {
    std::move(reply).Run(false);
    return;
  }
  std::move(reply).Run(
      live_caption_controller->DispatchTranscription(this, result));
}

void LiveCaptionSpeechRecognitionHost::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (!live_caption_controller)
    return;

  live_caption_controller->OnLanguageIdentificationEvent(std::move(event));
}

void LiveCaptionSpeechRecognitionHost::OnSpeechRecognitionError() {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnError(this);
}

void LiveCaptionSpeechRecognitionHost::RenderFrameDeleted(
    content::RenderFrameHost* frame_host) {
  if (frame_host == frame_host_) {
    LiveCaptionController* live_caption_controller = GetLiveCaptionController();
    if (live_caption_controller)
      live_caption_controller->OnAudioStreamEnd(this);
    frame_host_ = nullptr;
  }
}

#if defined(OS_MAC) || defined(OS_CHROMEOS)
void LiveCaptionSpeechRecognitionHost::MediaEffectivelyFullscreenChanged(
    bool is_fullscreen) {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnToggleFullscreen(this);
}
#endif

content::WebContents* LiveCaptionSpeechRecognitionHost::GetWebContents() {
  if (!frame_host_)
    return nullptr;
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host_);
  if (!web_contents)
    frame_host_ = nullptr;
  return web_contents;
}

LiveCaptionController*
LiveCaptionSpeechRecognitionHost::GetLiveCaptionController() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return nullptr;
  return LiveCaptionControllerFactory::GetForProfile(profile);
}

}  // namespace captions
