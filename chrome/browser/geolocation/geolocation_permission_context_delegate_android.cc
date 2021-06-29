// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context_delegate_android.h"

#include <utility>

#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/installable/installed_webapp_bridge.h"
#include "chrome/browser/permissions/permission_update_infobar_delegate_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/permissions/android/android_permission_util.h"
#include "components/permissions/permission_request_id.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "url/gurl.h"

GeolocationPermissionContextDelegateAndroid::
    GeolocationPermissionContextDelegateAndroid(Profile* profile)
    : GeolocationPermissionContextDelegate(profile) {}

GeolocationPermissionContextDelegateAndroid::
    ~GeolocationPermissionContextDelegateAndroid() = default;

bool GeolocationPermissionContextDelegateAndroid::DecidePermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback* callback,
    permissions::GeolocationPermissionContext* context) {
  if (web_contents->GetDelegate() &&
      web_contents->GetDelegate()->GetInstalledWebappGeolocationContext()) {
    InstalledWebappBridge::PermissionResponseCallback permission_callback =
        base::BindOnce(
            &permissions::GeolocationPermissionContext::NotifyPermissionSet,
            context->GetWeakPtr(), id, requesting_origin,
            web_contents->GetLastCommittedURL().GetOrigin(),
            std::move(*callback), false /* persist */);
    InstalledWebappBridge::DecidePermission(requesting_origin,
                                            std::move(permission_callback));
    return true;
  }
  return GeolocationPermissionContextDelegate::DecidePermission(
      web_contents, id, requesting_origin, user_gesture, callback, context);
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
    const TemplateURL* template_url =
        template_url_service->GetDefaultSearchProvider();
    if (template_url) {
      dse_url = template_url->GenerateSearchURL(
          template_url_service->search_terms_data());
    }
  }

  return url::IsSameOriginWith(requesting_origin, dse_url);
}

void GeolocationPermissionContextDelegateAndroid::FinishNotifyPermissionSet(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  if (requesting_origin != embedding_origin)
    return;

  // If this is the default search origin, and the DSE Geolocation setting is
  // being used, potentially show the disclosure.
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(id.render_process_id(),
                                           id.render_frame_id()));
  if (!web_contents)
    return;

  SearchGeolocationDisclosureTabHelper* disclosure_helper =
      SearchGeolocationDisclosureTabHelper::FromWebContents(web_contents);

  // The tab helper can be null in tests.
  if (disclosure_helper)
    disclosure_helper->MaybeShowDisclosureForAPIAccess(requesting_origin);
}
