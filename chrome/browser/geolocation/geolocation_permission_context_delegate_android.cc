// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_delegate_android.h"

#include <utility>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/webapps/installable/installed_webapp_bridge.h"
#include "components/permissions/android/android_permission_util.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/permission_request_id.h"
#include "components/permissions/permission_util.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "url/gurl.h"
#include "url/origin.h"

GeolocationPermissionContextDelegateAndroid::
    GeolocationPermissionContextDelegateAndroid(Profile* profile)
    : GeolocationPermissionContextDelegate(profile) {}

GeolocationPermissionContextDelegateAndroid::
    ~GeolocationPermissionContextDelegateAndroid() = default;

bool GeolocationPermissionContextDelegateAndroid::DecidePermission(
    const permissions::PermissionRequestData& request_data,
    permissions::BrowserPermissionCallback* callback,
    permissions::GeolocationPermissionContext* context) {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data.id.global_render_frame_host_id());
  DCHECK(rfh);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  DCHECK(web_contents);

  if (web_contents->GetDelegate() &&
      web_contents->GetDelegate()->GetInstalledWebappGeolocationContext()) {
    auto data = permissions::PermissionRequestData(
        context, request_data.id,
        content::PermissionRequestDescription(
            content::PermissionDescriptorUtil::
                CreatePermissionDescriptorForPermissionType(
                    blink::PermissionType::GEOLOCATION)),
        request_data.requesting_origin,
        permissions::PermissionUtil::GetLastCommittedOriginAsURL(
            rfh->GetMainFrame()));
    data.embedded_permission_request_descriptor =
        request_data.embedded_permission_request_descriptor.Clone();
    InstalledWebappBridge::PermissionCallback permission_callback =
        base::BindOnce(
            &permissions::GeolocationPermissionContext::NotifyPermissionSet,
            context->GetWeakPtr(), std::move(data), std::move(*callback),
            false /* persist */);
    InstalledWebappBridge::DecidePermission(
        ContentSettingsType::GEOLOCATION, request_data.requesting_origin,
        web_contents->GetLastCommittedURL(), std::move(permission_callback));
    return true;
  }
  return GeolocationPermissionContextDelegate::DecidePermission(
      request_data, callback, context);
}

bool GeolocationPermissionContextDelegateAndroid::IsInteractable(
    content::WebContents* web_contents) {
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  return tab && tab->IsUserInteractable();
}

PrefService* GeolocationPermissionContextDelegateAndroid::GetPrefs(
    content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)->GetPrefs();
}

bool GeolocationPermissionContextDelegateAndroid::IsRequestingOriginDSE(
    content::BrowserContext* browser_context,
    const GURL& requesting_origin) {
  GURL dse_url;

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  if (template_url_service) {
    url::Origin dse_origin =
        template_url_service->GetDefaultSearchProviderOrigin();
    return dse_origin.IsSameOriginWith(requesting_origin);
  }

  return false;
}
