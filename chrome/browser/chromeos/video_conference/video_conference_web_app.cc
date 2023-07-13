// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/video_conference/video_conference_web_app.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "chrome/browser/chromeos/video_conference/video_conference_ukm_helper.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-shared.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/process_manager.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace video_conference {

WEB_CONTENTS_USER_DATA_KEY_IMPL(VideoConferenceWebApp);

VideoConferenceWebApp::~VideoConferenceWebApp() = default;

void VideoConferenceWebApp::ActivateApp() {
  auto& web_contents = GetWebContents();
  web_contents.GetDelegate()->ActivateContents(&web_contents);
}

void VideoConferenceWebApp::SetCapturingStatus(VideoConferenceMediaType device,
                                               bool is_capturing) {
  vc_ukm_helper_->RegisterCapturingUpdate(device, is_capturing);

  switch (device) {
    case VideoConferenceMediaType::kCamera:
      state_.is_capturing_camera = is_capturing;
      break;
    case VideoConferenceMediaType::kMicrophone:
      state_.is_capturing_microphone = is_capturing;
      break;
    case VideoConferenceMediaType::kScreen:
      state_.is_capturing_screen = is_capturing;
      break;
  }
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
  CHECK(permission_controller);

  bool has_camera_permission = false;
  bool has_microphone_permission = false;

  // Get permission from each render frame host.
  web_contents.GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](content::RenderFrameHost* rfh) {
        auto camera_status =
            permission_controller->GetPermissionStatusForCurrentDocument(
                blink::PermissionType::VIDEO_CAPTURE, rfh);
        auto microphone_status =
            permission_controller->GetPermissionStatusForCurrentDocument(
                blink::PermissionType::AUDIO_CAPTURE, rfh);

        has_camera_permission |=
            camera_status == blink::mojom::PermissionStatus::GRANTED;

        has_microphone_permission |=
            microphone_status == blink::mojom::PermissionStatus::GRANTED;
      });

  return {has_camera_permission, has_microphone_permission};
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

void VideoConferenceWebApp::TitleWasSet(content::NavigationEntry* entry) {
  std::u16string new_title = std::u16string{entry->GetTitle()};

  auto title_change_info = crosapi::mojom::TitleChangeInfo::New(
      /*id=*/state_.id, /*new_title=*/std::move(new_title));
  client_update_callback_.Run(crosapi::mojom::VideoConferenceClientUpdate::New(
      /*added_or_removed_app=*/crosapi::mojom::VideoConferenceAppUpdate::kNone,
      /*title_change_info=*/std::move(title_change_info)));
}

VideoConferenceWebApp::VideoConferenceWebApp(
    content::WebContents* web_contents,
    base::UnguessableToken id,
    base::RepeatingCallback<void(const base::UnguessableToken&)>
        remove_media_app_callback,
    base::RepeatingCallback<void(
        crosapi::mojom::VideoConferenceClientUpdatePtr)> client_update_callback)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<VideoConferenceWebApp>(*web_contents),
      remove_media_app_callback_(std::move(remove_media_app_callback)),
      client_update_callback_(std::move(client_update_callback)),
      state_{.id = std::move(id),
             .last_activity_time = base::Time::Now(),
             .is_capturing_microphone = false,
             .is_capturing_camera = false,
             .is_capturing_screen = false},
      vc_ukm_helper_(std::make_unique<VideoConferenceUkmHelper>(
          ukm::UkmRecorder::Get(),
          web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId())) {
  CHECK(remove_media_app_callback_);

  auto* source =
      extensions::ProcessManager::Get(web_contents->GetBrowserContext());
  state_.is_extension = !!source->GetExtensionForWebContents(web_contents);
}

}  // namespace video_conference
