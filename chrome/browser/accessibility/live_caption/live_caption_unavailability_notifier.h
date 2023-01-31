// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_UNAVAILABILITY_NOTIFIER_H_
#define CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_UNAVAILABILITY_NOTIFIER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class PrefChangeRegistrar;
class PrefService;

namespace content {
class RenderFrameHost;
}

namespace captions {

class CaptionBubbleContextBrowser;
class LiveCaptionController;

// Used to notify the browser that the renderer does not support Live Caption.
class LiveCaptionUnavailabilityNotifier
    : public content::DocumentService<
          media::mojom::MediaFoundationRendererNotifier>,
      public media::mojom::MediaFoundationRendererObserver {
 public:
  LiveCaptionUnavailabilityNotifier(const LiveCaptionUnavailabilityNotifier&) =
      delete;
  LiveCaptionUnavailabilityNotifier& operator=(
      const LiveCaptionUnavailabilityNotifier&) = delete;

  // static
  static void Create(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererNotifier>
          receiver);

  // media::mojom::MediaFoundationRendererNotifier:
  void MediaFoundationRendererCreated(
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererObserver>
          observer) override;

 private:
  LiveCaptionUnavailabilityNotifier(
      content::RenderFrameHost& frame_host,
      mojo::PendingReceiver<media::mojom::MediaFoundationRendererNotifier>
          pending_receiver);
  ~LiveCaptionUnavailabilityNotifier() override;

  friend class LiveCaptionUnavailabilityNotifierTest;
  content::WebContents* GetWebContents();

  // Returns the LiveCaptionController for frame_host_. Returns nullptr if it
  // does not exist.
  LiveCaptionController* GetLiveCaptionController();

  bool ShouldDisplayMediaFoundationRendererError();
  bool ErrorSilencedForOrigin();
  void DisplayMediaFoundationRendererError();
  void OnMediaFoundationRendererErrorDoNotShowAgainCheckboxClicked(
      CaptionBubbleErrorType error_type,
      bool checked);
  void OnMediaFoundationRendererErrorClicked();
  void OnSpeechRecognitionAvailabilityChanged();

  mojo::ReceiverSet<media::mojom::MediaFoundationRendererObserver> observers_;
  std::unique_ptr<CaptionBubbleContextBrowser> context_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  raw_ptr<PrefService> profile_prefs_;

  base::WeakPtrFactory<LiveCaptionUnavailabilityNotifier> weak_factory_{this};
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_UNAVAILABILITY_NOTIFIER_H_
