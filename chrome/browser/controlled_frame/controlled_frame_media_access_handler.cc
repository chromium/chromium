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
#include "content/public/browser/web_contents_delegate.h"
#include "extensions/browser/browser_frame_context_data.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
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

bool IsMediaStreamTypeSupported(blink::mojom::MediaStreamType type) {
  return type == blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE ||
         type == blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE;
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

  // TODO(b/40238394): |GuestView| should be looked up from |RenderFrameHost|
  // instead of |WebContents|. To fix this, |SupportsStreamType| could pass
  // |RenderFrameHost| instead of |WebContents|.
  extensions::WebViewGuest* web_view =
      extensions::WebViewGuest::FromWebContents(web_contents);

  bool is_controlled_frame = web_view && web_view->attached() &&
                             web_view->IsOwnedByControlledFrameEmbedder();

  return is_controlled_frame && IsMediaStreamTypeSupported(type);
}

bool ControlledFrameMediaAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  CHECK(!extension);

  extensions::WebViewGuest* web_view =
      extensions::WebViewGuest::FromRenderFrameHost(render_frame_host);
  CHECK(web_view);

  if (!IsAllowedByPermissionsPolicy(web_view, security_origin, type)) {
    return false;
  }

  const url::Origin& embedder_origin =
      web_view->embedder_rfh()->GetLastCommittedOrigin();
  const url::Origin& requesting_origin =
      render_frame_host->GetLastCommittedOrigin();

  // Technically, Controlled Frame permission check needs to be done
  // asynchronously (via an event handled by the embedder). However, this method
  // must return immediately. |requests_| is used as a caching mechanism. An
  // embedder origin + requesting origin pair in |requests_| must have already
  // passed the asynchronous checks at least once. Unfortunately, this means
  // once a permission is granted, it cannot be revoked in the same session.
  // Note that the type check can be omitted here because WebView Permission
  // Request API does not differentiate audio and video requests, they are both
  // treated as "media".
  if (!requests_[embedder_origin].contains(requesting_origin)) {
    return false;
  }

  return web_view->embedder_web_contents()->GetDelegate() &&
         web_view->embedder_web_contents()
             ->GetDelegate()
             ->CheckMediaAccessPermission(web_view->embedder_rfh(),
                                          embedder_origin, type);
}

void ControlledFrameMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  CHECK(!extension);

  content::RenderFrameHost* requesting_rfh = content::RenderFrameHost::FromID(
      request.render_process_id, request.render_frame_id);
  CHECK(requesting_rfh);
  extensions::WebViewGuest* web_view =
      extensions::WebViewGuest::FromRenderFrameHost(requesting_rfh);
  CHECK(web_view);
  CHECK(web_view->attached());
  CHECK(web_view->IsOwnedByControlledFrameEmbedder());

  const url::Origin& embedder_origin =
      web_view->embedder_rfh()->GetLastCommittedOrigin();
  const url::Origin& requesting_origin = request.url_origin;

  requests_[embedder_origin].insert(requesting_origin);

  if (request.audio_type !=
          blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
      request.video_type !=
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DISMISSED,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  bool audio_denied =
      request.audio_type ==
          blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
      (!IsAllowedByPermissionsPolicy(web_view, requesting_origin,
                                     request.audio_type) ||
       GetDevicePolicy(profile,
                       web_view->GetGuestMainFrame()->GetLastCommittedURL(),
                       prefs::kAudioCaptureAllowed,
                       prefs::kAudioCaptureAllowedUrls) == ALWAYS_DENY);

  bool video_denied =
      request.video_type ==
          blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE &&
      (!IsAllowedByPermissionsPolicy(web_view, requesting_origin,
                                     request.video_type) ||
       GetDevicePolicy(profile,
                       web_view->GetGuestMainFrame()->GetLastCommittedURL(),
                       prefs::kVideoCaptureAllowed,
                       prefs::kVideoCaptureAllowedUrls) == ALWAYS_DENY);

  if (audio_denied || video_denied) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  content::GlobalRenderFrameHostId embedder_rfh_id =
      web_view->embedder_rfh()->GetGlobalId();
  content::MediaStreamRequest embedder_request = request;
  embedder_request.render_process_id = embedder_rfh_id.child_id;
  embedder_request.render_frame_id = embedder_rfh_id.frame_routing_id;
  embedder_request.url_origin = embedder_origin;
  embedder_request.security_origin = embedder_request.url_origin.GetURL();

  if (!web_view->embedder_web_contents()->GetDelegate()) {
    std::move(callback).Run(
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }
  web_view->embedder_web_contents()
      ->GetDelegate()
      ->RequestMediaAccessPermission(web_view->embedder_web_contents(),
                                     embedder_request, std::move(callback));
}

bool ControlledFrameMediaAccessHandler::IsAllowedByPermissionsPolicy(
    extensions::WebViewGuest* web_view,
    const url::Origin& requesting_origin,
    blink::mojom::MediaStreamType type) {
  if (!IsMediaStreamTypeSupported(type)) {
    return false;
  }

  // Checks that embedder's permissions policy allows for both the embedder
  // origin and requesting origin.
  content::RenderFrameHost* embedder_rfh = web_view->embedder_rfh();
  CHECK(embedder_rfh);

  const blink::PermissionsPolicy* permissions_policy =
      embedder_rfh->GetPermissionsPolicy();
  CHECK(permissions_policy);
  if (!permissions_policy->IsFeatureEnabledForOrigin(
          GetPermissionPolicyFeatureForMediaStreamType(type),
          requesting_origin)) {
    return false;
  }

  if (!permissions_policy->IsFeatureEnabledForOrigin(
          GetPermissionPolicyFeatureForMediaStreamType(type),
          embedder_rfh->GetLastCommittedOrigin())) {
    return false;
  }
  return true;
}

}  // namespace controlled_frame
