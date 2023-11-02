// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption_speech_recognition_host.h"

#include <memory>
#include <utility>

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/caption_bubble_context_browser.h"
#include "chrome/browser/accessibility/live_caption_controller_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace captions {

// static
void LiveCaptionSpeechRecognitionHost::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        receiver) {
  CHECK(frame_host);
  // The object is bound to the lifetime of |host| and the mojo
  // connection. See DocumentService for details.
  new LiveCaptionSpeechRecognitionHost(*frame_host, std::move(receiver));
}

LiveCaptionSpeechRecognitionHost::LiveCaptionSpeechRecognitionHost(
    content::RenderFrameHost& frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        receiver)
    : DocumentService<media::mojom::SpeechRecognitionRecognizerClient>(
          frame_host,
          std::move(receiver)) {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;
  Observe(web_contents);
  context_ = CaptionBubbleContextBrowser::Create(web_contents);
}

LiveCaptionSpeechRecognitionHost::~LiveCaptionSpeechRecognitionHost() {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnAudioStreamEnd(context_.get());
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
      live_caption_controller->DispatchTranscription(context_.get(), result));
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
    live_caption_controller->OnError(
        context_.get(), CaptionBubbleErrorType::kGeneric,
        base::RepeatingClosure(),
        base::BindRepeating(
            [](CaptionBubbleErrorType error_type, bool checked) {}));
}

void LiveCaptionSpeechRecognitionHost::OnSpeechRecognitionStopped() {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnAudioStreamEnd(context_.get());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
void LiveCaptionSpeechRecognitionHost::MediaEffectivelyFullscreenChanged(
    bool is_fullscreen) {
  LiveCaptionController* live_caption_controller = GetLiveCaptionController();
  if (live_caption_controller)
    live_caption_controller->OnToggleFullscreen(context_.get());
}
#endif

content::WebContents* LiveCaptionSpeechRecognitionHost::GetWebContents() {
  return content::WebContents::FromRenderFrameHost(&render_frame_host());
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
