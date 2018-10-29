// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_permission_helper.h"

#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_result.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace device {

ArCorePermissionHelper::ArCorePermissionHelper() : weak_ptr_factory_(this) {}

ArCorePermissionHelper::~ArCorePermissionHelper() {}

void ArCorePermissionHelper::RequestCameraPermission(
    int render_process_id,
    int render_frame_id,
    bool has_user_activation,
    base::OnceCallback<void(bool)> callback) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);

  DCHECK(rfh);
  // The RFH may have been destroyed by the time the request is processed.
  // We have to do a runtime check in addition to the DCHECK as it doesn't
  // trigger in release.
  if (!rfh) {
    DLOG(ERROR) << "The RenderFrameHost was destroyed prior to permission";
    std::move(callback).Run(false);
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  DCHECK(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  PermissionManager* permission_manager = PermissionManager::Get(profile);

  permission_manager->RequestPermission(
      CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, rfh, web_contents->GetURL(),
      has_user_activation,
      base::BindRepeating(
          &ArCorePermissionHelper::OnRequestCameraPermissionResult,
          GetWeakPtr(), web_contents, base::Passed(&callback)));
}

void ArCorePermissionHelper::OnRequestCameraPermissionResult(
    content::WebContents* web_contents,
    base::OnceCallback<void(bool)> callback,
    ContentSetting content_setting) {
  // If the camera permission is not allowed, abort the request.
  if (content_setting != CONTENT_SETTING_ALLOW) {
    std::move(callback).Run(false);
    return;
  }

  // Even if the content setting stated that the camera access is allowed,
  // the Android camera permission might still need to be requested, so check
  // if the OS level permission infobar should be shown.
  std::vector<ContentSettingsType> content_settings_types;
  content_settings_types.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  ShowPermissionInfoBarState show_permission_info_bar_state =
      PermissionUpdateInfoBarDelegate::ShouldShowPermissionInfoBar(
          web_contents, content_settings_types);
  switch (show_permission_info_bar_state) {
    case ShowPermissionInfoBarState::NO_NEED_TO_SHOW_PERMISSION_INFOBAR:
      std::move(callback).Run(true);
      return;
    case ShowPermissionInfoBarState::SHOW_PERMISSION_INFOBAR:
      // Show the Android camera permission info bar.
      PermissionUpdateInfoBarDelegate::Create(
          web_contents, content_settings_types,
          base::BindOnce(
              &ArCorePermissionHelper::OnRequestAndroidCameraPermissionResult,
              GetWeakPtr(), base::Passed(&callback)));
      return;
    case ShowPermissionInfoBarState::CANNOT_SHOW_PERMISSION_INFOBAR:
      std::move(callback).Run(false);
      return;
  }

  NOTREACHED() << "Unknown show permission infobar state.";
}

void ArCorePermissionHelper::OnRequestAndroidCameraPermissionResult(
    base::OnceCallback<void(bool)> callback,
    bool was_android_camera_permission_granted) {
  std::move(callback).Run(was_android_camera_permission_granted);
}

}  // namespace device
