// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_

#include "chrome/common/buildflags.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/common/plugin.mojom.h"
#endif

namespace url {
class Origin;
}  // namespace url

namespace extensions {
class WebViewGuest;

class ChromeWebViewPermissionHelperDelegate
    : public WebViewPermissionHelperDelegate
#if BUILDFLAG(ENABLE_PLUGINS)
    ,
      public chrome::mojom::PluginAuthHost
#endif
{
 public:
#if BUILDFLAG(ENABLE_PLUGINS)
  static void BindPluginAuthHost(
      mojo::PendingAssociatedReceiver<chrome::mojom::PluginAuthHost> receiver,
      content::RenderFrameHost* rfh);
#endif

  explicit ChromeWebViewPermissionHelperDelegate(
      WebViewPermissionHelper* web_view_permission_helper);

  ChromeWebViewPermissionHelperDelegate(
      const ChromeWebViewPermissionHelperDelegate&) = delete;
  ChromeWebViewPermissionHelperDelegate& operator=(
      const ChromeWebViewPermissionHelperDelegate&) = delete;

  ~ChromeWebViewPermissionHelperDelegate() override;

  // WebViewPermissionHelperDelegate implementation.
  void RequestMediaAccessPermissionForControlledFrame(
      content::WebContents* source,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermissionForControlledFrame(
      content::RenderFrameHost* render_frame_host,
      const url::Origin& security_origin,
      blink::mojom::MediaStreamType type) override;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override;
  void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      base::OnceCallback<void(bool)> callback) override;
  void RequestGeolocationPermission(
      const GURL& requesting_frame,
      bool user_gesture,
      base::OnceCallback<void(bool)> callback) override;
  void RequestHidPermission(const GURL& requesting_frame_url,
                            base::OnceCallback<void(bool)> callback) override;
  void RequestFileSystemPermission(
      const GURL& url,
      bool allowed_by_default,
      base::OnceCallback<void(bool)> callback) override;

  void RequestFullscreenPermission(
      const url::Origin& requesting_origin,
      WebViewPermissionHelper::PermissionResponseCallback callback) override;

 private:
#if BUILDFLAG(ENABLE_PLUGINS)
  // chrome::mojom::PluginAuthHost methods.
  void BlockedUnauthorizedPlugin(const std::u16string& name,
                                 const std::string& identifier) override;

  content::RenderFrameHostReceiverSet<chrome::mojom::PluginAuthHost>
      plugin_auth_host_receivers_;

  void OnPermissionResponse(const std::string& identifier,
                            bool allow,
                            const std::string& user_input);
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  void OnMediaPermissionResponseForControlledFrame(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback,
      bool allow,
      const std::string& user_input);

  void OnGeolocationPermissionResponse(
      bool user_gesture,
      base::OnceCallback<void(blink::mojom::PermissionStatus)> callback,
      bool allow,
      const std::string& user_input);

  void OnHidPermissionResponse(base::OnceCallback<void(bool)> callback,
                               bool allow,
                               const std::string& user_input);

  void OnFileSystemPermissionResponse(base::OnceCallback<void(bool)> callback,
                                      bool allow,
                                      const std::string& user_input);

  void OnDownloadPermissionResponse(base::OnceCallback<void(bool)> callback,
                                    bool allow,
                                    const std::string& user_input);

  void OnPointerLockPermissionResponse(base::OnceCallback<void(bool)> callback,
                                       bool allow,
                                       const std::string& user_input);

  void FileSystemAccessedAsyncResponse(int render_process_id,
                                       int render_frame_id,
                                       int request_id,
                                       const GURL& url,
                                       bool allowed);

  WebViewGuest* web_view_guest() {
    return web_view_permission_helper()->web_view_guest();
  }

  base::WeakPtrFactory<ChromeWebViewPermissionHelperDelegate> weak_factory_{
      this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
