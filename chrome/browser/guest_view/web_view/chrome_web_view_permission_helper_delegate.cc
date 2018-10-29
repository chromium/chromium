// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h"

#include <map>
#include <utility>

#include "base/metrics/user_metrics.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_manager.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/render_messages.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "ppapi/buildflags/buildflags.h"

namespace extensions {

namespace {

void CallbackWrapper(const base::Callback<void(bool)>& callback,
                     ContentSetting status) {
  callback.Run(status == CONTENT_SETTING_ALLOW);
}

}  // anonymous namespace

ChromeWebViewPermissionHelperDelegate::ChromeWebViewPermissionHelperDelegate(
    WebViewPermissionHelper* web_view_permission_helper)
    : WebViewPermissionHelperDelegate(web_view_permission_helper),
      plugin_auth_host_bindings_(web_contents(), this),
      weak_factory_(this) {}

ChromeWebViewPermissionHelperDelegate::~ChromeWebViewPermissionHelperDelegate()
{}

#if BUILDFLAG(ENABLE_PLUGINS)
bool ChromeWebViewPermissionHelperDelegate::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  IPC_BEGIN_MESSAGE_MAP(ChromeWebViewPermissionHelperDelegate, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_OpenPDF, OnOpenPDF)
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()

  return true;
}

void ChromeWebViewPermissionHelperDelegate::BlockedUnauthorizedPlugin(
    const base::string16& name,
    const std::string& identifier) {
  const char kPluginName[] = "name";
  const char kPluginIdentifier[] = "identifier";

  base::DictionaryValue info;
  info.SetString(std::string(kPluginName), name);
  info.SetString(std::string(kPluginIdentifier), identifier);
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN,
      info,
      base::Bind(&ChromeWebViewPermissionHelperDelegate::OnPermissionResponse,
                 weak_factory_.GetWeakPtr(),
                 identifier),
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

void ChromeWebViewPermissionHelperDelegate::OnOpenPDF(const GURL& url) {
  // Intentionally blank since guest views should never trigger PDF downloads.
}

void ChromeWebViewPermissionHelperDelegate::CanDownload(
    const GURL& url,
    const std::string& request_method,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guest_view::kUrl, url.spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_DOWNLOAD,
      request_info,
      base::Bind(
          &ChromeWebViewPermissionHelperDelegate::OnDownloadPermissionResponse,
          weak_factory_.GetWeakPtr(),
          callback),
      false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnDownloadPermissionResponse(
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::RequestPointerLockPermission(
    bool user_gesture,
    bool last_unlocked_by_target,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetBoolean(guest_view::kUserGesture, user_gesture);
  request_info.SetBoolean(webview::kLastUnlockedBySelf,
                          last_unlocked_by_target);
  request_info.SetString(guest_view::kUrl,
                         web_contents()->GetLastCommittedURL().spec());

  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK,
      request_info,
      base::Bind(&ChromeWebViewPermissionHelperDelegate::
                     OnPointerLockPermissionResponse,
                 weak_factory_.GetWeakPtr(),
                 callback),
      false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnPointerLockPermissionResponse(
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::RequestGeolocationPermission(
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guest_view::kUrl, requesting_frame.spec());
  request_info.SetBoolean(guest_view::kUserGesture, user_gesture);

  // It is safe to hold an unretained pointer to
  // ChromeWebViewPermissionHelperDelegate because this callback is called from
  // ChromeWebViewPermissionHelperDelegate::SetPermission.
  WebViewPermissionHelper::PermissionResponseCallback permission_callback =
      base::BindOnce(&ChromeWebViewPermissionHelperDelegate::
                         OnGeolocationPermissionResponse,
                     weak_factory_.GetWeakPtr(), bridge_id, user_gesture,
                     base::Bind(&CallbackWrapper, callback));
  int request_id = web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_GEOLOCATION, request_info,
      std::move(permission_callback), false /* allowed_by_default */);
  bridge_id_to_request_id_map_[bridge_id] = request_id;
}

void ChromeWebViewPermissionHelperDelegate::OnGeolocationPermissionResponse(
    int bridge_id,
    bool user_gesture,
    const base::Callback<void(ContentSetting)>& callback,
    bool allow,
    const std::string& user_input) {
  // The <webview> embedder has allowed the permission. We now need to make sure
  // that the embedder has geolocation permission.
  RemoveBridgeID(bridge_id);

  if (!allow || !web_view_guest()->attached()) {
    callback.Run(CONTENT_SETTING_BLOCK);
    return;
  }

  content::WebContents* web_contents =
      web_view_guest()->embedder_web_contents();
  int render_process_id = web_contents->GetMainFrame()->GetProcess()->GetID();
  int render_frame_id = web_contents->GetMainFrame()->GetRoutingID();

  const PermissionRequestID request_id(
      render_process_id,
      render_frame_id,
      // The geolocation permission request here is not initiated
      // through WebGeolocationPermissionRequest. We are only interested
      // in the fact whether the embedder/app has geolocation
      // permission. Therefore we use an invalid |bridge_id|.
      -1);

  Profile* profile = Profile::FromBrowserContext(
      web_view_guest()->browser_context());
  PermissionManager::Get(profile)->RequestPermission(
      CONTENT_SETTINGS_TYPE_GEOLOCATION, web_contents->GetMainFrame(),
      web_view_guest()
          ->embedder_web_contents()
          ->GetLastCommittedURL()
          .GetOrigin(),
      user_gesture,
      callback);
}

void ChromeWebViewPermissionHelperDelegate::CancelGeolocationPermissionRequest(
    int bridge_id) {
  int request_id = RemoveBridgeID(bridge_id);
  web_view_permission_helper()->CancelPendingPermissionRequest(request_id);
}

int ChromeWebViewPermissionHelperDelegate::RemoveBridgeID(int bridge_id) {
  auto bridge_itr = bridge_id_to_request_id_map_.find(bridge_id);
  if (bridge_itr == bridge_id_to_request_id_map_.end())
    return webview::kInvalidPermissionRequestID;

  int request_id = bridge_itr->second;
  bridge_id_to_request_id_map_.erase(bridge_itr);
  return request_id;
}

void ChromeWebViewPermissionHelperDelegate::RequestFileSystemPermission(
    const GURL& url,
    bool allowed_by_default,
    const base::Callback<void(bool)>& callback) {
  base::DictionaryValue request_info;
  request_info.SetString(guest_view::kUrl, url.spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_FILESYSTEM,
      request_info,
      base::Bind(&ChromeWebViewPermissionHelperDelegate::
                     OnFileSystemPermissionResponse,
                 weak_factory_.GetWeakPtr(),
                 callback),
      allowed_by_default);
}

void ChromeWebViewPermissionHelperDelegate::OnFileSystemPermissionResponse(
    const base::Callback<void(bool)>& callback,
    bool allow,
    const std::string& user_input) {
  callback.Run(allow && web_view_guest()->attached());
}

void ChromeWebViewPermissionHelperDelegate::FileSystemAccessedAsync(
    int render_process_id,
    int render_frame_id,
    int request_id,
    const GURL& url,
    bool blocked_by_policy) {
  RequestFileSystemPermission(
      url,
      !blocked_by_policy,
      base::Bind(&ChromeWebViewPermissionHelperDelegate::
                     FileSystemAccessedAsyncResponse,
                 weak_factory_.GetWeakPtr(),
                 render_process_id,
                 render_frame_id,
                 request_id,
                 url));
}

void ChromeWebViewPermissionHelperDelegate::FileSystemAccessedAsyncResponse(
    int render_process_id,
    int render_frame_id,
    int request_id,
    const GURL& url,
    bool allowed) {
  TabSpecificContentSettings::FileSystemAccessed(
      render_process_id, render_frame_id, url, !allowed);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (rfh) {
    rfh->Send(new ChromeViewMsg_RequestFileSystemAccessAsyncResponse(
        render_frame_id, request_id, allowed));
  }
}

}  // namespace extensions
