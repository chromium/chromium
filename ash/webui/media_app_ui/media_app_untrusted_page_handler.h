// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UNTRUSTED_PAGE_HANDLER_H_
#define ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UNTRUSTED_PAGE_HANDLER_H_

#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class MediaAppGuestUI;

// Implements the media_app mojom interface providing chrome://media-app
// with browser process functions to call from the renderer process.
class MediaAppUntrustedPageHandler
    : public media_app_ui::mojom::UntrustedPageHandler {
 public:
  MediaAppUntrustedPageHandler(
      MediaAppGuestUI& media_app_guest_ui,
      mojo::PendingReceiver<media_app_ui::mojom::UntrustedPageHandler> receiver,
      mojo::PendingRemote<media_app_ui::mojom::UntrustedPage> page);
  ~MediaAppUntrustedPageHandler() override;

  MediaAppUntrustedPageHandler(const MediaAppUntrustedPageHandler&) = delete;
  MediaAppUntrustedPageHandler& operator=(const MediaAppUntrustedPageHandler&) =
      delete;

 private:
  mojo::Receiver<media_app_ui::mojom::UntrustedPageHandler> untrusted_receiver_{
      this};
  mojo::Remote<media_app_ui::mojom::UntrustedPage> untrusted_page_;
  raw_ref<MediaAppGuestUI, ExperimentalAsh>
      media_app_guest_ui_;  // Owns |this|.
};

}  // namespace ash

#endif  // ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_UNTRUSTED_PAGE_HANDLER_H_
