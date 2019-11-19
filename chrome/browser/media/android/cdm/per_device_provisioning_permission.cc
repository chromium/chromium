// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/per_device_provisioning_permission.h"

#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/android/media_drm_bridge.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace {

// Only keep track of the last response for a short period of time.
constexpr base::TimeDelta kLastRequestDelta = base::TimeDelta::FromMinutes(15);

// Keep track of the last response. This is only kept in memory, so once Chrome
// quits it is forgotten.
class LastResponse {
 public:
  // If |origin| matches the previously saved |origin_| and this request is
  // before |expiry_time|, return true indicating that the previous value should
  // be used, and update |allowed| with the previous response. If the origin
  // doesn't match or the previous response was too long ago, return false.
  bool Matches(const url::Origin& origin, bool* allowed) {
    if (!origin_.IsSameOriginWith(origin) || base::Time::Now() > expiry_time_)
      return false;

    *allowed = allowed_;
    return true;
  }

  // Updates this object with the latest |origin| and |response|.
  void Update(const url::Origin& origin, bool response) {
    origin_ = origin;
    expiry_time_ = base::Time::Now() + kLastRequestDelta;
    allowed_ = response;
  }

 private:
  url::Origin origin_;
  base::Time expiry_time_;
  bool allowed_ = false;
};

// Returns an object containing the last response. We only keep track of one
// response (the latest). This is done for simplicity, as it is unlikely that
// there will different origins getting to this path at the same time. Requests
// could be from different |render_frame_host| objects, but this matches what
// normal permission requests do when the decision is persisted in user's
// profile.
LastResponse& GetLastResponse() {
  static base::NoDestructor<LastResponse> s_last_response;
  return *s_last_response;
}

// A PermissionRequest to allow MediaDrmBridge to use per-device provisioning.
class PerDeviceProvisioningPermissionRequest : public PermissionRequest {
 public:
  PerDeviceProvisioningPermissionRequest(
      const url::Origin& origin,
      base::OnceCallback<void(bool)> callback)
      : origin_(origin), callback_(std::move(callback)) {}

  PermissionRequest::IconId GetIconId() const final {
    return IDR_ANDROID_INFOBAR_PROTECTED_MEDIA_IDENTIFIER;
  }

  base::string16 GetTitleText() const final {
    return l10n_util::GetStringUTF16(
        IDS_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_TITLE);
  }

  base::string16 GetMessageText() const final {
    // Note that the string is specific to per-device provisioning.
    return l10n_util::GetStringFUTF16(
        IDS_PROTECTED_MEDIA_IDENTIFIER_PER_DEVICE_PROVISIONING_INFOBAR_TEXT,
        url_formatter::FormatUrlForSecurityDisplay(
            GetOrigin(), url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  }

  base::string16 GetMessageTextFragment() const final {
    return l10n_util::GetStringUTF16(
        IDS_PROTECTED_MEDIA_IDENTIFIER_PERMISSION_FRAGMENT);
  }

  GURL GetOrigin() const final { return origin_.GetURL(); }

  void PermissionGranted() final {
    UpdateLastResponse(true);
    std::move(callback_).Run(true);
  }

  void PermissionDenied() final {
    UpdateLastResponse(false);
    std::move(callback_).Run(false);
  }

  void Cancelled() final {
    UpdateLastResponse(false);
    std::move(callback_).Run(false);
  }

  void RequestFinished() final {
    // The |callback_| may not have run if the prompt was ignored, e.g. the tab
    // was closed while the prompt was displayed. Don't save this result as the
    // last response since it wasn't really a user action.
    if (callback_)
      std::move(callback_).Run(false);

    delete this;
  }

  PermissionRequestType GetPermissionRequestType() const final {
    return PermissionRequestType::PERMISSION_PROTECTED_MEDIA_IDENTIFIER;
  }

 private:
  // Can only be self-destructed. See RequestFinished().
  ~PerDeviceProvisioningPermissionRequest() final = default;

  void UpdateLastResponse(bool allowed) {
    GetLastResponse().Update(origin_, allowed);
  }

  const url::Origin origin_;
  base::OnceCallback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(PerDeviceProvisioningPermissionRequest);
};

}  // namespace

void RequestPerDeviceProvisioningPermission(
    content::RenderFrameHost* render_frame_host,
    base::OnceCallback<void(bool)> callback) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(render_frame_host);
  DCHECK(callback);
  DCHECK(media::MediaDrmBridge::IsPerOriginProvisioningSupported())
      << "RequestPerDeviceProvisioningPermission() should only be called when "
         "per-origin provisioning is supported.";

  // Return the previous response if it was for the same origin.
  bool last_response = false;
  if (GetLastResponse().Matches(render_frame_host->GetLastCommittedOrigin(),
                                &last_response)) {
    DVLOG(1) << "Using previous response: " << last_response;
    std::move(callback).Run(last_response);
    return;
  }

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents) << "WebContents not available.";

  auto* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);
  if (!permission_request_manager) {
    std::move(callback).Run(false);
    return;
  }

  // The created PerDeviceProvisioningPermissionRequest deletes itself once
  // complete. See PerDeviceProvisioningPermissionRequest::RequestFinished().
  permission_request_manager->AddRequest(
      new PerDeviceProvisioningPermissionRequest(
          render_frame_host->GetLastCommittedOrigin(), std::move(callback)));
}
