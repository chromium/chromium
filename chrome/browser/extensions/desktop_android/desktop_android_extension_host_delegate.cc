// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/desktop_android_extension_host_delegate.h"

#include "base/notimplemented.h"
#include "content/public/browser/web_contents_delegate.h"

namespace extensions {

DesktopAndroidExtensionHostDelegate::DesktopAndroidExtensionHostDelegate() =
    default;

DesktopAndroidExtensionHostDelegate::~DesktopAndroidExtensionHostDelegate() =
    default;

void DesktopAndroidExtensionHostDelegate::OnExtensionHostCreated(
    content::WebContents* web_contents) {}

void DesktopAndroidExtensionHostDelegate::OnMainFrameCreatedForBackgroundPage(
    ExtensionHost* host) {}

content::JavaScriptDialogManager*
DesktopAndroidExtensionHostDelegate::GetJavaScriptDialogManager() {
  // TODO(crbug.com/373434594): Support dialogs.
  NOTIMPLEMENTED();
  return nullptr;
}

void DesktopAndroidExtensionHostDelegate::CreateTab(
    std::unique_ptr<content::WebContents> web_contents,
    const ExtensionId& extension_id,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture) {
  // TODO(crbug.com/373434594): Support opening tabs.
  NOTIMPLEMENTED();
}

void DesktopAndroidExtensionHostDelegate::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const Extension* extension) {
  // TODO(crbug.com/373434594): Support media access.
  NOTIMPLEMENTED();
}

bool DesktopAndroidExtensionHostDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type,
    const Extension* extension) {
  // TODO(crbug.com/373434594): Support media access.
  NOTIMPLEMENTED();
  return true;
}

content::PictureInPictureResult
DesktopAndroidExtensionHostDelegate::EnterPictureInPicture(
    content::WebContents* web_contents) {
  // TODO(crbug.com/373434594): Support picture-in-picture.
  NOTIMPLEMENTED();
  return content::PictureInPictureResult::kNotSupported;
}

void DesktopAndroidExtensionHostDelegate::ExitPictureInPicture() {
  // TODO(crbug.com/373434594): Support picture-in-picture.
  NOTIMPLEMENTED();
}

}  // namespace extensions
