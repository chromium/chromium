// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SURFACE_H_
#define CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SURFACE_H_

#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace captions {

// Represents the document as a user-facing surface that produces live captions.
// This interface allows remote processes to manipulate the document as required
// for live caption rendering (e.g. by bringing it into focus).
//
// An instance of this object is owned by a `WebContents` and its lifetime is
// bound to the `WebContents`' lifetime. Up to one instance of this class can
// be instantiated for a single `WebContents`.
class LiveCaptionSurface
    : public media::mojom::SpeechRecognitionSurface,
      public content::WebContentsUserData<LiveCaptionSurface>,
      public content::WebContentsObserver {
 public:
  LiveCaptionSurface(const LiveCaptionSurface&) = delete;
  LiveCaptionSurface& operator=(const LiveCaptionSurface&) = delete;
  ~LiveCaptionSurface() override;

  // static
  static LiveCaptionSurface* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Set up coordintation with a new surface client in Ash.
  void BindToSurfaceClient(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSurface> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSurfaceClient> remote);

  // media::mojom::SpeechRecognitionSurface:
  void Activate() override;
  void GetBounds(GetBoundsCallback callback) override;

  // content::WebContentsObserver:
  void MediaEffectivelyFullscreenChanged(bool is_fullscreen) override;
  void PrimaryPageChanged(content::Page& page) override;

  // Returns a unique identifier for the current web contents.
  base::UnguessableToken session_id() const;

 private:
  friend content::WebContentsUserData<LiveCaptionSurface>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  explicit LiveCaptionSurface(content::WebContents* web_contents);

  const base::UnguessableToken session_id_;

  mojo::ReceiverSet<media::mojom::SpeechRecognitionSurface> receivers_;
  mojo::RemoteSet<media::mojom::SpeechRecognitionSurfaceClient> clients_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_ACCESSIBILITY_LIVE_CAPTION_LIVE_CAPTION_SURFACE_H_
