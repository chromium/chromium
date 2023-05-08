// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_GUEST_UI_H_
#define ASH_WEBUI_MEDIA_APP_UI_MEDIA_APP_GUEST_UI_H_

#include <string>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

// A delegate used during data source creation to expose some //chrome
// functionality to the data source
class MediaAppGuestUIDelegate {
 public:
  // Takes a WebUI and WebUIDataSource, and populates its load-time data.
  virtual void PopulateLoadTimeData(content::WebUI* web_ui,
                                    content::WebUIDataSource* source) = 0;
};

// The webui for chrome-untrusted://media-app.
class MediaAppGuestUI : public ui::UntrustedWebUIController,
                        public content::WebContentsObserver {
 public:
  MediaAppGuestUI(content::WebUI* web_ui, MediaAppGuestUIDelegate* delegate);
  MediaAppGuestUI(const MediaAppGuestUI&) = delete;
  MediaAppGuestUI& operator=(const MediaAppGuestUI&) = delete;
  ~MediaAppGuestUI() override;

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(content::NavigationHandle* handle) override;

 private:
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
