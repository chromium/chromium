// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/controlled_frame/controlled_frame_media_access_handler.h"

#include "base/types/expected.h"
#include "chrome/browser/media/webrtc/media_stream_device_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/browser_frame_context_data.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace controlled_frame {

namespace {

blink::mojom::PermissionsPolicyFeature
GetPermissionPolicyFeatureForMediaStreamType(
    blink::mojom::MediaStreamType type) {
  switch (type) {
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return blink::mojom::PermissionsPolicyFeature::kMicrophone;
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
      return blink::mojom::PermissionsPolicyFeature::kCamera;
    default:
      return blink::mojom::PermissionsPolicyFeature::kNotFound;
  }
}

}  // namespace

ControlledFrameMediaAccessHandler::PendingMediaAccessRequestDetails::
    PendingMediaAccessRequestDetails(const url::Origin& embedded_frame_origin,
                                     blink::mojom::MediaStreamType type)
    : embedded_frame_origin(embedded_frame_origin), type(type) {}

ControlledFrameMediaAccessHandler::ControlledFrameMediaAccessHandler() =
    default;
ControlledFrameMediaAccessHandler::~ControlledFrameMediaAccessHandler() =
    default;

bool ControlledFrameMediaAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  if (!web_contents || extension) {
    return false;
  }

  extensions::BrowserFrameContextData context_data(
      web_contents->GetPrimaryMainFrame());
  content::RenderFrameHost* embedder_rfh =
      web_contents->GetOutermostWebContents()->GetPrimaryMainFrame();
  extensions::BrowserFrameContextData embedder_context_data(embedder_rfh);

  // This first call to this function will be from the embedded frame. Store
  // a mapping of the IWA URL to the embedded page's URL and return false to
  // allow for the permission request event to be fired to the Isolated Web App
  // that embeds this frame.
  bool is_controlled_frame = !context_data.IsIsolatedApplication() &&
                             embedder_context_data.IsIsolatedApplication();
  if (is_controlled_frame) {
    pending_requests_[embedder_rfh->GetLastCommittedOrigin()].emplace_back(
        web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(), type);
    return false;
  }

  // The second call to this function will be from the Isolated Web App after
  // handling and allowing the permission request event that was fired from the
  // Controlled Frame.
  bool is_isolated_web_app = context_data.IsIsolatedApplication() &&
                             embedder_context_data.IsIsolatedApplication();
  return is_isolated_web_app &&
         pending_requests_.contains(
             web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin()) &&
         (type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
          type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
}

bool ControlledFrameMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  CHECK(!extension);
  return IsPermissionAllowed(render_frame_host, security_origin, type);
}

void ControlledFrameMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  CHECK(!extension);
  if (!pending_requests_.contains(
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin())) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bool audio_allowed =
      request.audio_type ==
          blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
      IsPermissionAllowed(web_contents->GetPrimaryMainFrame(),
                          request.url_origin, request.audio_type) &&
      GetDevicePolicy(profile, web_contents->GetLastCommittedURL(),
                      prefs::kAudioCaptureAllowed,
                      prefs::kAudioCaptureAllowedUrls) != ALWAYS_DENY;

  bool video_allowed =
      request.video_type ==
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE &&
      IsPermissionAllowed(web_contents->GetPrimaryMainFrame(),
                          request.url_origin, request.video_type) &&
      GetDevicePolicy(profile, web_contents->GetLastCommittedURL(),
                      prefs::kVideoCaptureAllowed,
                      prefs::kVideoCaptureAllowedUrls) != ALWAYS_DENY;

  CheckDevicesAndRunCallback(web_contents, request, std::move(callback),
                             audio_allowed, video_allowed);
}

bool ControlledFrameMediaAccessHandler::IsPermissionAllowed(
    content::RenderFrameHost* embedder_rfh,
    const url::Origin& request_origin,
    blink::mojom::MediaStreamType type) {
  extensions::BrowserFrameContextData embedder_context_data(embedder_rfh);
  if (!embedder_context_data.IsIsolatedApplication()) {
    return false;
  }

  const url::Origin& embedder_origin = embedder_rfh->GetLastCommittedOrigin();
  if (!pending_requests_.contains(embedder_origin)) {
    return false;
  }

  // Verify that there is a pending request for the given permission type and
  // remove the request.
  std::vector<PendingMediaAccessRequestDetails> details =
      pending_requests_.at(embedder_origin);
  auto it = std::find_if(details.cbegin(), details.cend(),
                         [&](const PendingMediaAccessRequestDetails& detail) {
                           return detail.type == type;
                         });
  if (it == details.cend() || it->embedded_frame_origin != request_origin) {
    return false;
  }
  details.erase(it);

  const blink::PermissionsPolicy* permissions_policy =
      embedder_rfh->GetPermissionsPolicy();
  return permissions_policy->IsFeatureEnabledForOrigin(
      GetPermissionPolicyFeatureForMediaStreamType(type), request_origin);
}

}  // namespace controlled_frame
