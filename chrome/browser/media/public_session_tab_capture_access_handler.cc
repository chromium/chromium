// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/public_session_tab_capture_access_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/extensions/public_session_permission_helper.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/login/login_state/login_state.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern_set.h"

PublicSessionTabCaptureAccessHandler::PublicSessionTabCaptureAccessHandler() {}

PublicSessionTabCaptureAccessHandler::~PublicSessionTabCaptureAccessHandler() {}

bool PublicSessionTabCaptureAccessHandler::SupportsStreamType(
    content::WebContents* web_contents,
    const blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return tab_capture_access_handler_.SupportsStreamType(web_contents, type,
                                                        extension);
}

bool PublicSessionTabCaptureAccessHandler::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const extensions::Extension* extension) {
  return tab_capture_access_handler_.CheckMediaAccessPermission(
      render_frame_host, security_origin, type, extension);
}

void PublicSessionTabCaptureAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension) {
  // This class handles requests for Public Sessions only, outside of them just
  // pass the request through to the original class.
  if (!profiles::ArePublicSessionRestrictionsEnabled() || !extension ||
      (request.audio_type !=
           blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE &&
       request.video_type !=
           blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE)) {
    return tab_capture_access_handler_.HandleRequest(
        web_contents, request, std::move(callback), extension);
  }

  // This Unretained is safe because the lifetime of this object is until
  // process exit (living inside a base::Singleton object).
  auto prompt_resolved_callback = base::AdaptCallbackForRepeating(
      base::BindOnce(&PublicSessionTabCaptureAccessHandler::ChainHandleRequest,
                     base::Unretained(this), web_contents, request,
                     std::move(callback), base::RetainedRef(extension)));

  extensions::permission_helper::HandlePermissionRequest(
      *extension, {extensions::APIPermission::kTabCapture}, web_contents,
      prompt_resolved_callback, extensions::permission_helper::PromptFactory());
}

void PublicSessionTabCaptureAccessHandler::ChainHandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const extensions::Extension* extension,
    const extensions::PermissionIDSet& allowed_permissions) {
  content::MediaStreamRequest request_copy(request);

  // If the user denied tab capture, here the request gets filtered out before
  // being passed on to the actual implementation.
  if (!allowed_permissions.ContainsID(extensions::APIPermission::kTabCapture)) {
    request_copy.audio_type = blink::mojom::MediaStreamType::NO_SERVICE;
    request_copy.video_type = blink::mojom::MediaStreamType::NO_SERVICE;
  }

  // Pass the request through to the original class.
  tab_capture_access_handler_.HandleRequest(web_contents, request_copy,
                                            std::move(callback), extension);
}
