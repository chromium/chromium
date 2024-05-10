// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_DEVTOOLS_MANAGER_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_DEVTOOLS_MANAGER_DELEGATE_H_

#include <jni.h>

#include <memory>
#include <vector>

#include "content/public/browser/devtools_manager_delegate.h"

namespace android_webview {

// Delegate implementation for the devtools http handler for WebView. A new
// instance of this gets created each time web debugging is enabled.
class AwDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  AwDevToolsManagerDelegate();

  AwDevToolsManagerDelegate(const AwDevToolsManagerDelegate&) = delete;
  AwDevToolsManagerDelegate& operator=(const AwDevToolsManagerDelegate&) =
      delete;

  ~AwDevToolsManagerDelegate() override;

  // content::DevToolsManagerDelegate implementation.
  std::string GetTargetDescription(content::WebContents* web_contents) override;
  std::string GetDiscoveryPageHTML() override;
  bool IsBrowserTargetDiscoverable() override;
  // Returns all targets embedder would like to report as debuggable remotely.
  content::DevToolsAgentHost::List RemoteDebuggingTargets(
      TargetType target_type) override;
};

} //  namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DEVTOOLS_MANAGER_DELEGATE_H_
