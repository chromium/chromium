// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLIPBOARD_CHROME_CLIPBOARD_PERMISSION_CONTEXT_DELEGATE_H_
#define CHROME_BROWSER_CLIPBOARD_CHROME_CLIPBOARD_PERMISSION_CONTEXT_DELEGATE_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/contexts/clipboard_permission_context_delegate.h"

class GURL;

namespace content {
class RenderFrameHost;
}

namespace extensions {
class WebViewPermissionHelper;
}

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

  const Type type_;

  base::WeakPtrFactory<ChromeClipboardPermissionContextDelegate> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_CLIPBOARD_CHROME_CLIPBOARD_PERMISSION_CONTEXT_DELEGATE_H_
