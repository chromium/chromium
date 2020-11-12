// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nfc/nfc_permission_context_android.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "chrome/browser/android/tab_android.h"
#include "components/permissions/android/nfc/nfc_system_level_setting_impl.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/web_contents.h"

NfcPermissionContextAndroid::NfcPermissionContextAndroid(
    content::BrowserContext* browser_context)
    : NfcPermissionContext(browser_context),
      nfc_system_level_setting_(
          std::make_unique<permissions::NfcSystemLevelSettingImpl>()) {}

NfcPermissionContextAndroid::~NfcPermissionContextAndroid() = default;

void NfcPermissionContextAndroid::NotifyPermissionSet(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting) {
  if (content_setting != CONTENT_SETTING_ALLOW ||
      !nfc_system_level_setting_->IsNfcAccessPossible() ||
      nfc_system_level_setting_->IsNfcSystemLevelSettingEnabled()) {
    NfcPermissionContext::NotifyPermissionSet(
        id, requesting_origin, embedding_origin, std::move(callback), persist,
        content_setting);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(
          content::RenderFrameHost::FromID(id.render_process_id(),
                                           id.render_frame_id()));

  // Ignore when the associated RenderFrameHost has already been destroyed.
  if (!web_contents)
    return;

  // Only show the NFC system level setting prompt if the tab for |web_contents|
  // is user-interactable (i.e. is the current tab, and Chrome is active and not
  // in tab-switching mode).
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  if (tab && !tab->IsUserInteractable()) {
    permissions::PermissionContextBase::NotifyPermissionSet(
        id, requesting_origin, embedding_origin, std::move(callback),
        false /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  // TODO(crbug.com/1034607): Close prompt when there is a navigation in a tab
  nfc_system_level_setting_->PromptToEnableNfcSystemLevelSetting(
      web_contents,
      base::BindOnce(
          &NfcPermissionContextAndroid::OnNfcSystemLevelSettingPromptClosed,
          weak_factory_.GetWeakPtr(), id, requesting_origin, embedding_origin,
          std::move(callback), persist, content_setting));
}

void NfcPermissionContextAndroid::OnNfcSystemLevelSettingPromptClosed(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    ContentSetting content_setting) {
  NfcPermissionContext::NotifyPermissionSet(
      id, requesting_origin, embedding_origin, std::move(callback), persist,
      content_setting);
}
