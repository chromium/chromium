// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h"

#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "chrome/common/buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#endif

namespace extensions {

namespace {

void CallbackWrapper(base::OnceCallback<void(bool)> callback,
                     blink::mojom::PermissionStatus status) {
  std::move(callback).Run(status == blink::mojom::PermissionStatus::GRANTED);
}

}  // anonymous namespace

#if BUILDFLAG(ENABLE_PLUGINS)
// static
void ChromeWebViewPermissionHelperDelegate::BindPluginAuthHost(
    mojo::PendingAssociatedReceiver<chrome::mojom::PluginAuthHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHost(rfh);
  if (!permission_helper)
    return;
  WebViewPermissionHelperDelegate* delegate = permission_helper->delegate();
  if (!delegate)
    return;
  auto* chrome_delegate =
      static_cast<ChromeWebViewPermissionHelperDelegate*>(delegate);
  chrome_delegate->plugin_auth_host_receivers_.Bind(rfh, std::move(receiver));
}
#endif

ChromeWebViewPermissionHelperDelegate::ChromeWebViewPermissionHelperDelegate(
    WebViewPermissionHelper* web_view_permission_helper)
    : WebViewPermissionHelperDelegate(web_view_permission_helper)
#if BUILDFLAG(ENABLE_PLUGINS)
      ,
      plugin_auth_host_receivers_(
          web_view_permission_helper->web_view_guest()->web_contents(),
          this)
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

  base::Value::Dict info;
  info.Set(kPluginName, name);
  info.Set(kPluginIdentifier, identifier);
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_LOAD_PLUGIN, std::move(info),
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
        web_view_permission_helper()->web_view_guest()->web_contents(), true,
        identifier);
  }
}

#endif  // BUILDFLAG(ENABLE_PLUGINS)

void ChromeWebViewPermissionHelperDelegate::CanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  base::Value::Dict request_info;
  request_info.Set(guest_view::kUrl, url.spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_DOWNLOAD, std::move(request_info),
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
  base::Value::Dict request_info;
  request_info.Set(guest_view::kUserGesture, user_gesture);
  request_info.Set(webview::kLastUnlockedBySelf, last_unlocked_by_target);
  request_info.Set(guest_view::kUrl, web_view_permission_helper()
                                         ->web_view_guest()
                                         ->web_contents()
                                         ->GetLastCommittedURL()
                                         .spec());

  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_POINTER_LOCK, std::move(request_info),
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
  base::Value::Dict request_info;
  request_info.Set(guest_view::kUrl, requesting_frame.spec());
  request_info.Set(guest_view::kUserGesture, user_gesture);

  // It is safe to hold an unretained pointer to
  // ChromeWebViewPermissionHelperDelegate because this callback is called from
  // ChromeWebViewPermissionHelperDelegate::SetPermission.
  WebViewPermissionHelper::PermissionResponseCallback permission_callback =
      base::BindOnce(&ChromeWebViewPermissionHelperDelegate::
                         OnGeolocationPermissionResponse,
                     weak_factory_.GetWeakPtr(), user_gesture,
                     base::BindOnce(&CallbackWrapper, std::move(callback)));
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_GEOLOCATION, std::move(request_info),
      std::move(permission_callback), false /* allowed_by_default */);
}

void ChromeWebViewPermissionHelperDelegate::OnGeolocationPermissionResponse(
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback,
    bool allow,
    const std::string& user_input) {
  // The <webview> embedder has allowed the permission. We now need to make sure
  // that the embedder has geolocation permission.
  if (!allow || !web_view_guest()->attached()) {
    std::move(callback).Run(blink::mojom::PermissionStatus::DENIED);
    return;
  }

  web_view_guest()
      ->browser_context()
      ->GetPermissionController()
      ->RequestPermissionFromCurrentDocument(
          blink::PermissionType::GEOLOCATION,
          web_view_guest()->embedder_web_contents()->GetPrimaryMainFrame(),
          user_gesture, std::move(callback));
}

void ChromeWebViewPermissionHelperDelegate::RequestFileSystemPermission(
    const GURL& url,
    bool allowed_by_default,
    base::OnceCallback<void(bool)> callback) {
  base::Value::Dict request_info;
  request_info.Set(guest_view::kUrl, url.spec());
  web_view_permission_helper()->RequestPermission(
      WEB_VIEW_PERMISSION_TYPE_FILESYSTEM, std::move(request_info),
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
