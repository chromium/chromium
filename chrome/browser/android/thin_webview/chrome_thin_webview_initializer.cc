// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/thin_webview/chrome_thin_webview_initializer.h"

#include "chrome/browser/android/web_contents_theme_client.h"
#include "chrome/browser/ui/android/context_menu_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "components/permissions/permission_request_manager.h"

namespace thin_webview::android {

// static
void ChromeThinWebViewInitializer::Initialize() {
  ThinWebViewInitializer::SetInstance(new ChromeThinWebViewInitializer);
}

void ChromeThinWebViewInitializer::SetUpTheming(
    content::WebContents* web_contents) {
  night_mode::WebContentsThemeClient::CreateForWebContents(web_contents);
}

void ChromeThinWebViewInitializer::AttachTabHelpers(
    content::WebContents* web_contents,
    bool enable_permission_requests) {
  TabHelpers::AttachTabHelpers(web_contents);
  // TODO(crbug.com/491498632): use enable_permission_requests requests.
  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_web_contents_supports_permission_requests(false);
}

void ChromeThinWebViewInitializer::SetContextMenuPopulatorFactory(
    content::WebContents* web_contents,
    const base::android::JavaRef<jobject>& jpopulator_factory) {
  auto* helper = ContextMenuHelper::FromWebContents(web_contents);
  if (helper) {
    helper->SetPopulatorFactory(jpopulator_factory);
  }
}

}  // namespace thin_webview::android
