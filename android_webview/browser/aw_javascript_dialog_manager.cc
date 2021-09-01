// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_javascript_dialog_manager.h"

#include <utility>

#include "android_webview/browser/aw_contents_client_bridge.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {

AwJavaScriptDialogManager::AwJavaScriptDialogManager() {}

AwJavaScriptDialogManager::~AwJavaScriptDialogManager() {}

void AwJavaScriptDialogManager::RunJavaScriptDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    DialogClosedCallback callback,
    bool* did_suppress_message) {
  AwContentsClientBridge* bridge =
      AwContentsClientBridge::FromWebContents(web_contents);
  if (!bridge) {
    std::move(callback).Run(false, std::u16string());
    return;
  }

  bridge->RunJavaScriptDialog(
      dialog_type, render_frame_host->GetLastCommittedURL(), message_text,
      default_prompt_text, std::move(callback));
}

void AwJavaScriptDialogManager::RunBeforeUnloadDialog(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    bool is_reload,
    DialogClosedCallback callback) {
  AwContentsClientBridge* bridge =
      AwContentsClientBridge::FromWebContents(web_contents);
  if (!bridge) {
    std::move(callback).Run(false, std::u16string());
    return;
  }

  bridge->RunBeforeUnloadDialog(web_contents->GetURL(), std::move(callback));
}

void AwJavaScriptDialogManager::CancelDialogs(
    content::WebContents* web_contents,
    bool reset_state) {}

}  // namespace android_webview
