// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_delegate.h"

#include "base/functional/bind.h"
#include "chrome/browser/geolocation/geolocation_permission_context_slim_web_view.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

GeolocationPermissionContextDelegate::GeolocationPermissionContextDelegate(
    content::BrowserContext* browser_context)
    : extensions_context_(Profile::FromBrowserContext(browser_context)) {}

GeolocationPermissionContextDelegate::~GeolocationPermissionContextDelegate() =
    default;

bool GeolocationPermissionContextDelegate::DecidePermission(
    const permissions::PermissionRequestData& request_data,
    permissions::BrowserPermissionCallback* callback,
    permissions::GeolocationPermissionContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (std::optional<GeolocationPermissionContextExtensions::Decision>
          extension_decision = extensions_context_.DecidePermission(
              request_data.id, request_data.requesting_origin,
              request_data.user_gesture, callback)) {
    CHECK_EQ(!!*callback, extension_decision->permission_set);
    if (extension_decision->permission_set) {
      context->NotifyPermissionSet(request_data, std::move(*callback),
                                   /*persist=*/false,
                                   extension_decision->decision);
    }
    return true;
  }
  // Handle permission requests if the render frame is for a SlimWebView.
  // Note that SlimWebView permissions are never persisted and don't have UI
  // indicators.
  if (GeolocationPermissionContextSlimWebView::DecidePermission(
          request_data.id, request_data.requesting_origin,
          request_data.user_gesture, callback)) {
    return true;
  }
  return false;
}
