// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_PAGE_HANDLER_H_
#define ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_PAGE_HANDLER_H_

#include "ash/webui/media_app_ui/media_app_ui.mojom.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/mojom/url.mojom.h"

namespace ash {

class MediaAppUI;

// Implements the media_app mojom interface providing chrome://media-app
// with browser process functions to call from the renderer process.
class MediaAppPageHandler : public media_app_ui::mojom::PageHandler {
 public:
  MediaAppPageHandler(
      MediaAppUI* media_app_ui,
      mojo::PendingReceiver<media_app_ui::mojom::PageHandler> receiver);
  ~MediaAppPageHandler() override;

  MediaAppPageHandler(const MediaAppPageHandler&) = delete;
  MediaAppPageHandler& operator=(const MediaAppPageHandler&) = delete;

  // media_app_ui::mojom::PageHandler:
  void OpenFeedbackDialog(OpenFeedbackDialogCallback callback) override;
  void ToggleBrowserFullscreenMode(
      ToggleBrowserFullscreenModeCallback callback) override;
  void MaybeTriggerPdfHats(MaybeTriggerPdfHatsCallback callback) override;
  void IsFileArcWritable(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      IsFileArcWritableCallback callback) override;
  void IsFileBrowserWritable(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      IsFileBrowserWritableCallback callback) override;
  void EditInPhotos(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      const std::string& mime_type,
      EditInPhotosCallback callback) override;
  void SubmitForm(const GURL& url,
                  const std::vector<int8_t>& payload,
                  const std::string& header,
                  SubmitFormCallback callback) override;

 private:
  mojo::Receiver<media_app_ui::mojom::PageHandler> receiver_;
  raw_ptr<MediaAppUI> media_app_ui_;  // Owns |this|.
};

}  // namespace ash

#endif  // ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_PAGE_HANDLER_H_
