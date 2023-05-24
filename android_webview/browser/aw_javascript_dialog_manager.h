// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_JAVASCRIPT_DIALOG_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_JAVASCRIPT_DIALOG_MANAGER_H_

#include "content/public/browser/javascript_dialog_manager.h"

namespace android_webview {

/**
 * Implements JavaScriptDialogManager for WebView.
 *
 * This class is a singleton, but it doesn't store any state, each method just
 * calls through to the AwContentsClientBridge tied to the Web Contents. If
 * you add state, please consider how this will work with Multi-profile.
 *
 * Lifetime: Singleton
 */
class AwJavaScriptDialogManager : public content::JavaScriptDialogManager {
 public:
  AwJavaScriptDialogManager();

  AwJavaScriptDialogManager(const AwJavaScriptDialogManager&) = delete;
  AwJavaScriptDialogManager& operator=(const AwJavaScriptDialogManager&) =
      delete;

  ~AwJavaScriptDialogManager() override;

  // Overridden from content::JavaScriptDialogManager:
  void RunJavaScriptDialog(content::WebContents* web_contents,
                           content::RenderFrameHost* render_frame_host,
                           content::JavaScriptDialogType dialog_type,
                           const std::u16string& message_text,
                           const std::u16string& default_prompt_text,
                           DialogClosedCallback callback,
                           bool* did_suppress_message) override;
  void RunBeforeUnloadDialog(content::WebContents* web_contents,
                             content::RenderFrameHost* render_frame_host,
                             bool is_reload,
                             DialogClosedCallback callback) override;
  void CancelDialogs(content::WebContents* web_contents,
                     bool reset_state) override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_JAVASCRIPT_DIALOG_MANAGER_H_
