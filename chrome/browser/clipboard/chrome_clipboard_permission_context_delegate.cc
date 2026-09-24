// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/clipboard/chrome_clipboard_permission_context_delegate.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_types.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/api_permission_id.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

WebViewPermissionType GetWebViewPermissionType(
    ChromeClipboardPermissionContextDelegate::Type type) {
  switch (type) {
    case ChromeClipboardPermissionContextDelegate::Type::kReadWrite:
      return WEB_VIEW_PERMISSION_TYPE_CLIPBOARD_READ_WRITE;
    case ChromeClipboardPermissionContextDelegate::Type::kSanitizedWrite:
      return WEB_VIEW_PERMISSION_TYPE_CLIPBOARD_SANITIZED_WRITE;
  }
}

}  // namespace

ChromeClipboardPermissionContextDelegate::
    ChromeClipboardPermissionContextDelegate(Type type)
    : type_(type) {}

ChromeClipboardPermissionContextDelegate::
    ~ChromeClipboardPermissionContextDelegate() = default;

bool ChromeClipboardPermissionContextDelegate::DecidePermission(
    const permissions::PermissionRequestData& request_data,
    permissions::BrowserPermissionCallback callback) {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_data.id.global_render_frame_host_id());

  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHost(rfh);

  if (!web_view_permission_helper) {
    return false;
  }

  // Check embedder permission status to not allow to
  // make clipboard actions if user has revoked the permission.
  const url::Origin& requesting_origin = rfh->GetLastCommittedOrigin();
  if (web_view_permission_helper->HasClipboardPermission(
          GetWebViewPermissionType(type_), requesting_origin)) {
    if (IsEmbedderPermissionGranted(web_view_permission_helper)) {
      std::move(callback).Run(content::PermissionResult(
          blink::mojom::PermissionStatus::GRANTED,
          content::PermissionStatusSource::UNSPECIFIED));
    } else {
      std::move(callback).Run(content::PermissionResult(
          blink::mojom::PermissionStatus::DENIED,
          content::PermissionStatusSource::UNSPECIFIED));
    }

  } else {
    base::OnceCallback<void(bool)> final_callback = base::BindOnce(
        &ChromeClipboardPermissionContextDelegate::OnWebViewPermissionResult,
        weak_factory_.GetWeakPtr(), std::move(callback), request_data.id);

    switch (type_) {
      case Type::kReadWrite:
        web_view_permission_helper->RequestClipboardReadWritePermission(
            request_data.requesting_origin, request_data.user_gesture,
            std::move(final_callback));
        break;
      case Type::kSanitizedWrite:
        web_view_permission_helper->RequestClipboardSanitizedWritePermission(
            request_data.requesting_origin, std::move(final_callback));
        break;
    }
  }
  return true;
}

std::optional<ContentSetting>
ChromeClipboardPermissionContextDelegate::GetPermissionStatusForWebview(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin_url) const {
  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHost(
          render_frame_host);
  if (!web_view_permission_helper) {
    return std::nullopt;
  }

  const url::Origin& requesting_origin =
      render_frame_host->GetLastCommittedOrigin();
  if (web_view_permission_helper->HasClipboardPermission(
          GetWebViewPermissionType(type_), requesting_origin)) {
    if (IsEmbedderPermissionGranted(web_view_permission_helper)) {
      return ContentSetting::CONTENT_SETTING_ALLOW;
    } else {
      return ContentSetting::CONTENT_SETTING_BLOCK;
    }
  } else {
    return ContentSetting::CONTENT_SETTING_ASK;
  }
}

std::optional<ContentSetting>
ChromeClipboardPermissionContextDelegate::GetPermissionStatusForWorker(
    content::BrowserContext* browser_context,
    const GURL& requesting_origin) const {
  // Use url::Origin to be robust against path components (e.g. background.js)
  url::Origin origin = url::Origin::Create(requesting_origin);
  if (origin.scheme() != extensions::kExtensionScheme) {
    return std::nullopt;
  }

  const std::string extension_id = origin.host();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context);
  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension) {
    // Deferring here would fall through to the web default, which allows
    // sanitized write. An extension origin we cannot resolve gets nothing.
    return ContentSetting::CONTENT_SETTING_BLOCK;
  }

  if (type_ == Type::kReadWrite) {
    // Clipboard read (paste) is intentionally blocked for service workers
    // because they lack the user-visible context needed for safe paste
    // operations. Extensions should use popup/side panel views for reading.
    return ContentSetting::CONTENT_SETTING_BLOCK;
  }

  if (type_ == Type::kSanitizedWrite) {
    if (extension->permissions_data()->HasAPIPermission(
            extensions::mojom::APIPermissionID::kClipboardWrite)) {
      return ContentSetting::CONTENT_SETTING_ALLOW;
    }
  }

  return ContentSetting::CONTENT_SETTING_BLOCK;
}

bool ChromeClipboardPermissionContextDelegate::IsEmbedderPermissionGranted(
    extensions::WebViewPermissionHelper* web_view_permission_helper) const {
  blink::PermissionType permission_type;
  switch (type_) {
    case Type::kReadWrite:
      permission_type = blink::PermissionType::CLIPBOARD_READ_WRITE;
      break;
    case Type::kSanitizedWrite:
      permission_type = blink::PermissionType::CLIPBOARD_SANITIZED_WRITE;
      break;
  }
  content::RenderFrameHost* embedder_rfh =
      web_view_permission_helper->web_view_guest()->embedder_rfh();

  content::PermissionStatus permission_status =
      embedder_rfh->GetBrowserContext()
          ->GetPermissionController()
          ->GetPermissionStatusForCurrentDocument(
              content::PermissionDescriptorUtil::
                  CreatePermissionDescriptorForPermissionType(permission_type),
              embedder_rfh);

  return permission_status == content::PermissionStatus::GRANTED;
}

void ChromeClipboardPermissionContextDelegate::OnWebViewPermissionResult(
    permissions::BrowserPermissionCallback callback,
    permissions::PermissionRequestID request_id,
    bool allowed) {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      request_id.global_render_frame_host_id());
  if (!rfh) {
    // The frame has gone away. Don't try to use it.
    return;
  }

  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHost(rfh);

  if (web_view_permission_helper && allowed) {
    web_view_permission_helper->GrantClipboardPermission(
        GetWebViewPermissionType(type_), rfh->GetLastCommittedOrigin());
  }

  std::move(callback).Run(content::PermissionResult(
      allowed ? blink::mojom::PermissionStatus::GRANTED
              : blink::mojom::PermissionStatus::DENIED,
      content::PermissionStatusSource::UNSPECIFIED));
}
