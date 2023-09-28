// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_GUEST_UI_H_
#define ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_GUEST_UI_H_

#include <string>

#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ui {
class ColorChangeHandler;
}

namespace ash {

class MediaAppUntrustedPageHandler;

// A delegate used during data source creation to expose some //chrome
// functionality to the data source
class MediaAppGuestUIDelegate {
 public:
  // Takes a WebUI and WebUIDataSource, and populates its load-time data.
  virtual void PopulateLoadTimeData(content::WebUI* web_ui,
                                    content::WebUIDataSource* source) = 0;
};

// The webui for chrome-untrusted://media-app.
class MediaAppGuestUI
    : public ui::UntrustedWebUIController,
      public content::WebContentsObserver,
      public media_app_ui::mojom::UntrustedPageHandlerFactory {
 public:
  MediaAppGuestUI(content::WebUI* web_ui, MediaAppGuestUIDelegate* delegate);
  MediaAppGuestUI(const MediaAppGuestUI&) = delete;
  MediaAppGuestUI& operator=(const MediaAppGuestUI&) = delete;
  ~MediaAppGuestUI() override;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(content::NavigationHandle* handle) override;

  // Binds a PageHandler to MediaAppGuestUI. This handler grabs a reference to
  // the page and pushes a colorChangeEvent to the untrusted JS running there
  // when the OS color scheme has changed.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // Binds an UntrustedPageHandler to the MediaAppGuestUI. This handler is used
  // for communication between the untrusted MediaApp frame and the browser.
  void BindInterface(
      mojo::PendingReceiver<media_app_ui::mojom::UntrustedPageHandlerFactory>
          factory);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  // media_app_ui::mojom::UntrustedPageHandlerFactory:
  void CreateUntrustedPageHandler(
      mojo::PendingReceiver<media_app_ui::mojom::UntrustedPageHandler> receiver,
      mojo::PendingRemote<media_app_ui::mojom::UntrustedPage> page) override;

  void StartFontDataRequest(
      const std::string& path,
      content::WebUIDataSource::GotDataCallback got_data_callback);
  void StartFontDataRequestAfterPathExists(
      const base::FilePath& font_path,
      content::WebUIDataSource::GotDataCallback got_data_callback,
      bool path_exists);

  // The background task runner on which file I/O is performed.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Whether ReadyToCommitNavigation has occurred for the main `app.html`.
  bool app_navigation_committed_ = false;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  mojo::Receiver<media_app_ui::mojom::UntrustedPageHandlerFactory>
      untrusted_page_factory_{this};
  std::unique_ptr<MediaAppUntrustedPageHandler> untrusted_page_handler_;

  base::WeakPtrFactory<MediaAppGuestUI> weak_factory_{this};
};

struct MediaAppUserActions {
  bool clicked_edit_image_in_photos;
  bool clicked_edit_video_in_photos;
};
// Returns a snapshot of the user actions that are tracked whilst any MediaApp
// instance is running, in order to populate product-specific survey data.
MediaAppUserActions GetMediaAppUserActionsForHappinessTracking();

}  // namespace ash

#endif  // ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_GUEST_UI_H_
