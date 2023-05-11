// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PROJECTOR_MESSAGE_HANDLER_H_
#define ASH_WEBUI_PROJECTOR_APP_PROJECTOR_MESSAGE_HANDLER_H_

#include <memory>

#include "ash/public/cpp/projector/projector_new_screencast_precondition.h"
#include "ash/webui/projector_app/projector_oauth_token_fetcher.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace ash {

struct ProjectorScreencastVideo;

// Handles messages from the Projector WebUIs (i.e. chrome://projector).
class ProjectorMessageHandler : public content::WebUIMessageHandler {
 public:
  ProjectorMessageHandler();
  ProjectorMessageHandler(const ProjectorMessageHandler&) = delete;
  ProjectorMessageHandler& operator=(const ProjectorMessageHandler&) = delete;
  ~ProjectorMessageHandler() override;

  base::WeakPtr<ProjectorMessageHandler> GetWeakPtr();

  // content::WebUIMessageHandler:
  // TODO(b/237337607): chrome.send() is banned on ash. Migrate to Mojo instead.
  void RegisterMessages() override;

  void set_web_ui_for_test(content::WebUI* web_ui) { set_web_ui(web_ui); }

 private:
  // Requested by the Projector SWA to fetch a single video from DriveFS with
  // the Drive item id specified by `args`.
  void GetVideo(const base::Value::List& args);

  // Called when video file fetch by item id request is complete. Resolves the
  // javascript promise created by ProjectorBrowserProxy.getScreencast by
  // calling the `js_callback_id`.
  void OnVideoLocated(const std::string& js_callback_id,
                      std::unique_ptr<ProjectorScreencastVideo> video,
                      const std::string& error_message);

  base::WeakPtrFactory<ProjectorMessageHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_MESSAGE_HANDLER_H_
