// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/video_conference/video_conference_web_app.h"

#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/page.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/process_manager.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace video_conference {

WEB_CONTENTS_USER_DATA_KEY_IMPL(VideoConferenceWebApp);

VideoConferenceWebApp::~VideoConferenceWebApp() = default;

void VideoConferenceWebApp::ActivateApp() {
  auto& web_contents = GetWebContents();
  web_contents.GetDelegate()->ActivateContents(&web_contents);
}

VideoConferencePermissions VideoConferenceWebApp::GetPermissions() {
  // Permissions don't work the same way for extensions so we equate permissions
  // to capturing status for them.
  if (state_.is_extension) {
    return {.has_camera_permission = state_.is_capturing_camera,
            .has_microphone_permission = state_.is_capturing_microphone};
  }

  auto& web_contents = GetWebContents();

  auto* permission_controller =
      web_contents.GetBrowserContext()->GetPermissionController();
  DCHECK(permission_controller);

  auto* rfh = web_contents.GetPrimaryMainFrame();
  DCHECK(rfh);

  auto camera_status =
      permission_controller->GetPermissionStatusForCurrentDocument(
          blink::PermissionType::VIDEO_CAPTURE, rfh);
  auto microphone_status =
      permission_controller->GetPermissionStatusForCurrentDocument(
          blink::PermissionType::AUDIO_CAPTURE, rfh);

  return {.has_camera_permission =
              (camera_status == blink::mojom::PermissionStatus::GRANTED),
          .has_microphone_permission =
              (microphone_status == blink::mojom::PermissionStatus::GRANTED)};
}

bool VideoConferenceWebApp::IsInactiveExtension() {
  return state_.is_extension &&
         !(state_.is_capturing_camera || state_.is_capturing_microphone ||
           state_.is_capturing_screen);
}

void VideoConferenceWebApp::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  state_.last_activity_time = base::Time::Now();
}

void VideoConferenceWebApp::WebContentsDestroyed() {
  remove_media_app_callback_.Run(state_.id);
}

void VideoConferenceWebApp::PrimaryPageChanged(content::Page& page) {
  remove_media_app_callback_.Run(state_.id);
}

VideoConferenceWebApp::VideoConferenceWebApp(
    content::WebContents* web_contents,
    base::UnguessableToken id,
    base::RepeatingCallback<void(const base::UnguessableToken&)>
        remove_media_app_callback)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<VideoConferenceWebApp>(*web_contents),
      remove_media_app_callback_(std::move(remove_media_app_callback)),
      state_{.id = std::move(id),
             .last_activity_time = base::Time::Now(),
             .is_capturing_microphone = false,
             .is_capturing_camera = false,
             .is_capturing_screen = false} {
  DCHECK(remove_media_app_callback_);

  auto* source =
      extensions::ProcessManager::Get(web_contents->GetBrowserContext());
  state_.is_extension = !!source->GetExtensionForWebContents(web_contents);
}

}  // namespace video_conference
