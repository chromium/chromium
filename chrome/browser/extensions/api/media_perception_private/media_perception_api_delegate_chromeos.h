// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_DELEGATE_CHROMEOS_H_

#include "base/functional/callback_forward.h"
#include "extensions/browser/api/media_perception_private/media_perception_api_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace extensions {

class MediaPerceptionAPIDelegateChromeOS
    : public extensions::MediaPerceptionAPIDelegate {
 public:
  MediaPerceptionAPIDelegateChromeOS();

  MediaPerceptionAPIDelegateChromeOS(
      const MediaPerceptionAPIDelegateChromeOS&) = delete;
  MediaPerceptionAPIDelegateChromeOS& operator=(
      const MediaPerceptionAPIDelegateChromeOS&) = delete;

  ~MediaPerceptionAPIDelegateChromeOS() override;

  // extensions::MediaPerceptionAPIDelegate:
  void LoadCrOSComponent(
      const api::media_perception_private::ComponentType& type,
      LoadCrOSComponentCallback load_callback) override;
  void BindVideoSourceProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver)
      override;
  void SetMediaPerceptionRequestHandler(
      MediaPerceptionRequestHandler handler) override;
  void ForwardMediaPerceptionReceiver(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<chromeos::media_perception::mojom::MediaPerception>
          receiver) override;

 private:
  MediaPerceptionRequestHandler handler_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MEDIA_PERCEPTION_PRIVATE_MEDIA_PERCEPTION_API_DELEGATE_CHROMEOS_H_
