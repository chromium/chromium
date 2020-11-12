// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_delegate.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_request_id.h"
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
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback* callback,
    permissions::GeolocationPermissionContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool permission_set;
  bool new_permission;
  if (extensions_context_.DecidePermission(
          web_contents, id, id.request_id(), requesting_origin, user_gesture,
          callback, &permission_set, &new_permission)) {
    DCHECK_EQ(!!*callback, permission_set);
    if (permission_set) {
      ContentSetting content_setting =
          new_permission ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
      context->NotifyPermissionSet(
          id, requesting_origin,
          web_contents->GetLastCommittedURL().GetOrigin(), std::move(*callback),
          false /* persist */, content_setting);
    }
    return true;
  }
  return false;
}
