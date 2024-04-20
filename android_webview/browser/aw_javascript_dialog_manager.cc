// Copyright 2012 The Chromium Authors
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

  // Non-WebView versions of Chrome use the frame's last committed origin for
  // dialog attribution (crbug.com/1241497). However, WebView exposes JS dialog
  // calls to users, which means that a change to using the origin (exposed as
  // a URL) would be app-visible and a potential compatibility issue.
  //
  // In addition, WebView is special in that the "last committed origin" and the
  // origin of the "last committed URL" can be different due to a legacy app-
  // exposed setting, so such a change might be even more breaking.
  //
  // TODO(crbug.com/40194877): Figure out if some kind of migration can be done
  // here, as this is one of several instances in which moving from URL to
  // origin would be desirable.
  //
  // References:
  // https://chromium-review.googlesource.com/c/chromium/src/+/2944834/27..46/android_webview/browser/aw_permission_manager.cc#b599
  // https://chromium-review.googlesource.com/c/chromium/src/+/3107569/5/android_webview/browser/aw_javascript_dialog_manager.cc#41
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
