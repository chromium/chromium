// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_manager.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#endif

namespace extensions {

namespace {

void CallbackWrapper(base::OnceCallback<void(bool)> callback,
                     ContentSetting status) {
  std::move(callback).Run(status == CONTENT_SETTING_ALLOW);
}

}  // anonymous namespace

ChromeWebViewPermissionHelperDelegate::ChromeWebViewPermissionHelperDelegate(
    WebViewPermissionHelper* web_view_permission_helper)
    : WebViewPermissionHelperDelegate(web_view_permission_helper)
#if BUILDFLAG(ENABLE_PLUGINS)
      ,
      plugin_auth_host_receivers_(web_contents(), this)
#endif
{
}

ChromeWebViewPermissionHelperDelegate::~ChromeWebViewPermissionHelperDelegate()
{}

#if BUILDFLAG(ENABLE_PLUGINS)

void ChromeWebViewPermissionHelperDelegate::BlockedUnauthorizedPlugin(
    const std::u16string& name,
    const std::string& identifier) {
  const char kPluginName[] = "name";
  const char kPluginIdentifier[] = "identifier";

  base::DictionaryValue info;
  info.SetString(std::string(kPluginName), name);
  info.SetString(std::string(kPluginIdentifier), identifier);
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN, info,
      base::BindOnce(
          &ChromeWebViewPermissionHelperDelegate::OnPermissionResponse,
          weak_factory_.GetWeakPtr(), identifier),
      true /* allowed_by_default */);
  base::RecordAction(
      base::UserMetricsAction("WebView.Guest.PluginLoadRequest"));
}

void ChromeWebViewPermissionHelperDelegate::OnPermissionResponse(
    const std::string& identifier,
    bool allow,
    const std::string& input) {
  if (allow) {
    ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
        web_contents(), true, identifier);
  }
}

#endif  // BUILDFLAG(ENABLE_PLUGINS)

void ChromeWebViewPermissionHelperDelegate::CanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guest_view::kUrl, url.spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_DOWNLOAD, request_info,
      base::BindOnce(
          &ChromeWebViewPermissionHelperDelegate::OnDownloadPermissionResponse,
          weak_factory_.GetWeakPtr(), std::move(callback)),
      false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnDownloadPermissionResponse(
    base::OnceCallback<void(bool)> callback,
    bool allow,
    const std::string& user_input) {
  std::move(callback).Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::RequestPointerLockPermission(
    bool user_gesture,
    bool last_unlocked_by_target,
    base::OnceCallback<void(bool)> callback) {
  base::DictionaryValue request_info;
  request_info.SetBoolean(guest_view::kUserGesture, user_gesture);
  request_info.SetBoolean(webview::kLastUnlockedBySelf,
                          last_unlocked_by_target);
  request_info.SetString(guest_view::kUrl,
                         web_contents()->GetLastCommittedURL().spec());

  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK, request_info,
      base::BindOnce(&ChromeWebViewPermissionHelperDelegate::
                         OnPointerLockPermissionResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnPointerLockPermissionResponse(
    base::OnceCallback<void(bool)> callback,
    bool allow,
    const std::string& user_input) {
  std::move(callback).Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::RequestGeolocationPermission(
    const GURL& requesting_frame,
    bool user_gesture,
    base::OnceCallback<void(bool)> callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guest_view::kUrl, requesting_frame.spec());
  request_info.SetBoolean(guest_view::kUserGesture, user_gesture);

  // It is safe to hold an unretained pointer to
  // ChromeWebViewPermissionHelperDelegate because this callback is called from
  // ChromeWebViewPermissionHelperDelegate::SetPermission.
  WebViewPermissionHelper::PermissionResponseCallback permission_callback =
      base::BindOnce(&ChromeWebViewPermissionHelperDelegate::
                         OnGeolocationPermissionResponse,
                     weak_factory_.GetWeakPtr(), user_gesture,
                     base::BindOnce(&CallbackWrapper, std::move(callback)));
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_GEOLOCATION, request_info,
      std::move(permission_callback), false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnGeolocationPermissionResponse(
    bool user_gesture,
    base::OnceCallback<void(ContentSetting)> callback,
    bool allow,
    const std::string& user_input) {
  // The <webview> embedder has allowed the permission. We now need to make sure
  // that the embedder has geolocation permission.
  if (!allow || !web_view_guest()->attached()) {
    std::move(callback).Run(CONTENT_SETTING_BLOCK);
    return;
  }

  Profile* profile = Profile::FromBrowserContext(
      web_view_guest()->browser_context());
  PermissionManagerFactory::GetForProfile(profile)->RequestPermission(
      ContentSettingsType::GEOLOCATION,
      web_view_guest()->embedder_web_contents()->GetMainFrame(),
      web_view_guest()
          ->embedder_web_contents()
          ->GetLastCommittedURL()
          .GetOrigin(),
      user_gesture, std::move(callback));
}

void ChromeWebViewPermissionHelperDelegate::RequestFileSystemPermission(
    const GURL& url,
    bool allowed_by_default,
    base::OnceCallback<void(bool)> callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guest_view::kUrl, url.spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_FILESYSTEM, request_info,
      base::BindOnce(&ChromeWebViewPermissionHelperDelegate::
                         OnFileSystemPermissionResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      allowed_by_default);
}

void ChromeWebViewPermissionHelperDelegate::OnFileSystemPermissionResponse(
    base::OnceCallback<void(bool)> callback,
    bool allow,
    const std::string& user_input) {
  std::move(callback).Run(allow && web_view_guest()->attached());
}

}  // namespace extensions
