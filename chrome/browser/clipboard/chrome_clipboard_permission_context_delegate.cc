// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/clipboard/chrome_clipboard_permission_context_delegate.h"

#include <utility>

#include "base/containers/map_util.h"
#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "components/permissions/permission_decision.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"

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
  if (IsPermissionGrantedToWebView(rfh, web_view_permission_helper)) {
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
ChromeClipboardPermissionContextDelegate::GetPermissionStatus(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin_url) const {
  extensions::WebViewPermissionHelper* web_view_permission_helper =
      extensions::WebViewPermissionHelper::FromRenderFrameHost(
          render_frame_host);
  if (!web_view_permission_helper) {
    return std::nullopt;
  }

  if (IsPermissionGrantedToWebView(render_frame_host,
                                   web_view_permission_helper)) {
    if (IsEmbedderPermissionGranted(web_view_permission_helper)) {
      return ContentSetting::CONTENT_SETTING_ALLOW;
    } else {
      return ContentSetting::CONTENT_SETTING_BLOCK;
    }
  } else {
    return ContentSetting::CONTENT_SETTING_ASK;
  }
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

  const url::Origin& embedder_origin =
      web_view_permission_helper->web_view_guest()
          ->embedder_rfh()
          ->GetLastCommittedOrigin();
  const url::Origin& requesting_origin = rfh->GetLastCommittedOrigin();

  if (allowed) {
    granted_permissions_[embedder_origin].insert(requesting_origin);
  }

  std::move(callback).Run(content::PermissionResult(
      allowed ? blink::mojom::PermissionStatus::GRANTED
              : blink::mojom::PermissionStatus::DENIED,
      content::PermissionStatusSource::UNSPECIFIED));
}

bool ChromeClipboardPermissionContextDelegate::IsPermissionGrantedToWebView(
    content::RenderFrameHost* render_frame_host,
    extensions::WebViewPermissionHelper* web_view_permission_helper) const {
  content::RenderFrameHost* embedder_rfh =
      web_view_permission_helper->web_view_guest()->embedder_rfh();
  const url::Origin& embedder_origin = embedder_rfh->GetLastCommittedOrigin();
  const url::Origin& requesting_origin =
      render_frame_host->GetLastCommittedOrigin();

  if (auto* permissions =
          base::FindOrNull(granted_permissions_, embedder_origin)) {
    return permissions->contains(requesting_origin);
  }
  return false;
}
