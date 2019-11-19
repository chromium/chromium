// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_

#include "base/macros.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/plugin.mojom.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace extensions {
class WebViewGuest;

class ChromeWebViewPermissionHelperDelegate
    : public WebViewPermissionHelperDelegate,
      public chrome::mojom::PluginAuthHost {
 public:
  explicit ChromeWebViewPermissionHelperDelegate(
      WebViewPermissionHelper* web_view_permission_helper);
  ~ChromeWebViewPermissionHelperDelegate() override;

  // WebViewPermissionHelperDelegate implementation.
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override;
  void RequestPointerLockPermission(
      bool user_gesture,
      bool last_unlocked_by_target,
      const base::Callback<void(bool)>& callback) override;
  void RequestGeolocationPermission(
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      base::OnceCallback<void(bool)> callback) override;
  void CancelGeolocationPermissionRequest(int bridge_id) override;
  void RequestFileSystemPermission(
      const GURL& url,
      bool allowed_by_default,
      base::OnceCallback<void(bool)> callback) override;
#if BUILDFLAG(ENABLE_PLUGINS)
  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
#endif  // BUILDFLAG(ENABLE_PLUGINS)

 private:
#if BUILDFLAG(ENABLE_PLUGINS)
  // chrome::mojom::PluginAuthHost methods.
  void BlockedUnauthorizedPlugin(const base::string16& name,
                                 const std::string& identifier) override;

  content::WebContentsFrameBindingSet<chrome::mojom::PluginAuthHost>
      plugin_auth_host_bindings_;

  void OnPermissionResponse(const std::string& identifier,
                            bool allow,
                            const std::string& user_input);
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  void OnOpenPDF(const GURL& url);

  void OnGeolocationPermissionResponse(
      int bridge_id,
      bool user_gesture,
      base::OnceCallback<void(ContentSetting)> callback,
      bool allow,
      const std::string& user_input);

  void OnFileSystemPermissionResponse(base::OnceCallback<void(bool)> callback,
                                      bool allow,
                                      const std::string& user_input);

  void OnDownloadPermissionResponse(base::OnceCallback<void(bool)> callback,
                                    bool allow,
                                    const std::string& user_input);

  void OnPointerLockPermissionResponse(
      const base::Callback<void(bool)>& callback,
      bool allow,
      const std::string& user_input);

  // Bridge IDs correspond to a geolocation request. This method will remove
  // the bookkeeping for a particular geolocation request associated with the
  // provided |bridge_id|. It returns the request ID of the geolocation request.
  int RemoveBridgeID(int bridge_id);

  void FileSystemAccessedAsyncResponse(int render_process_id,
                                       int render_frame_id,
                                       int request_id,
                                       const GURL& url,
                                       bool allowed);

  WebViewGuest* web_view_guest() {
    return web_view_permission_helper()->web_view_guest();
  }

  std::map<int, int> bridge_id_to_request_id_map_;

  base::WeakPtrFactory<ChromeWebViewPermissionHelperDelegate> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(ChromeWebViewPermissionHelperDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_GUEST_VIEW_WEB_VIEW_CHROME_WEB_VIEW_PERMISSION_HELPER_DELEGATE_H_
