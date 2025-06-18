// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLIPBOARD_CHROME_CLIPBOARD_PERMISSION_CONTEXT_DELEGATE_H_
#define CHROME_BROWSER_CLIPBOARD_CHROME_CLIPBOARD_PERMISSION_CONTEXT_DELEGATE_H_

#include <map>
#include <optional>
#include <set>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/contexts/clipboard_permission_context_delegate.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"

// The class adds logic to check permission for a frame inside of a WebView.
class ChromeClipboardPermissionContextDelegate
    : public permissions::ClipboardPermissionContextDelegate {
 public:
  enum class Type {
    kReadWrite,
    kSanitizedWrite,
  };

  explicit ChromeClipboardPermissionContextDelegate(Type type);

  ChromeClipboardPermissionContextDelegate(
      const ChromeClipboardPermissionContextDelegate&) = delete;
  ChromeClipboardPermissionContextDelegate& operator=(
      const ChromeClipboardPermissionContextDelegate&) = delete;
  ~ChromeClipboardPermissionContextDelegate() override;

  // permissions::ClipboardPermissionContextDelegate:
  bool DecidePermission(
      const permissions::PermissionRequestData& request_data,
      permissions::BrowserPermissionCallback callback) override;

  // permissions::ClipboardPermissionContextDelegate:
  // This method is necessary, because when permission is granted by a webview
  // request, the result is not saved to ContentSettings and permission status
  // still would be prompt.
  std::optional<ContentSetting> GetPermissionStatus(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin) const override;

 private:
  bool IsEmbedderPermissionGranted(
      extensions::WebViewPermissionHelper* web_view_permission_helper) const;

  void OnWebViewPermissionResult(
      permissions::BrowserPermissionCallback callback,
      permissions::PermissionRequestID request_id,
      bool allowed);

  bool IsPermissionGrantedToWebView(
      content::RenderFrameHost* render_frame_host,
      extensions::WebViewPermissionHelper* web_view_permission_helper) const;

  // The first origin belongs to the top level frame.
  // The second origin is embedded frame origin.
  // This ensures that permissions are properly isolated.
  // Also the embedded origin is obtained from
  // web_view_guest()->embedder_rfh()->GetLastCommittedOrigin();
  // and not from permissions::PermissionRequest.embedding_origin,
  // because the latter uses GetMainFrame() which doesn't traverse GuestView.
  base::flat_map<url::Origin, std::set<url::Origin>> granted_permissions_;

  const Type type_;

  base::WeakPtrFactory<ChromeClipboardPermissionContextDelegate> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_CLIPBOARD_CHROME_CLIPBOARD_PERMISSION_CONTEXT_DELEGATE_H_
