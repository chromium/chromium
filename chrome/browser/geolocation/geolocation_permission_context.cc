// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context.h"

#include "base/bind.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/web_contents.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "url/origin.h"

GeolocationPermissionContext::GeolocationPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            ContentSettingsType::GEOLOCATION,
                            blink::mojom::FeaturePolicyFeature::kGeolocation),
      extensions_context_(profile) {}

GeolocationPermissionContext::~GeolocationPermissionContext() {
}

void GeolocationPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    BrowserPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool permission_set;
  bool new_permission;
  if (extensions_context_.DecidePermission(
          web_contents, id, id.request_id(), requesting_origin, user_gesture,
          &callback, &permission_set, &new_permission)) {
    DCHECK_EQ(!!callback, permission_set);
    if (permission_set) {
      ContentSetting content_setting =
          new_permission ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
      NotifyPermissionSet(id, requesting_origin,
                          web_contents->GetLastCommittedURL().GetOrigin(),
                          std::move(callback), false /* persist */,
                          content_setting);
    }
    return;
  }
  DCHECK(callback);

  PermissionContextBase::DecidePermission(web_contents, id, requesting_origin,
                                          embedding_origin, user_gesture,
                                          std::move(callback));
}

void GeolocationPermissionContext::UpdateTabContext(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::GetForFrame(id.render_process_id(),
                                              id.render_frame_id());

  // WebContents might not exist (extensions) or no longer exist. In which case,
  // TabSpecificContentSettings will be null.
  if (content_settings)
    content_settings->OnGeolocationPermissionSet(
        requesting_frame.GetOrigin(), allowed);

  if (allowed) {
    GetGeolocationControl()->UserDidOptIntoLocationServices();
  }
}

bool GeolocationPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}

device::mojom::GeolocationControl*
GeolocationPermissionContext::GetGeolocationControl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (geolocation_control_)
    return geolocation_control_.get();

  auto receiver = geolocation_control_.BindNewPipeAndPassReceiver();
  service_manager::Connector* connector = content::GetSystemConnector();
  if (connector)
    connector->Connect(device::mojom::kServiceName, std::move(receiver));
  return geolocation_control_.get();
}
