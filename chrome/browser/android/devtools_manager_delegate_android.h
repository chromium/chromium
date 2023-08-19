// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_

#include <memory>

#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/devtools_manager_delegate.h"

class DevToolsManagerDelegateAndroid : public content::DevToolsManagerDelegate {
 public:
  DevToolsManagerDelegateAndroid();

  DevToolsManagerDelegateAndroid(const DevToolsManagerDelegateAndroid&) =
      delete;
  DevToolsManagerDelegateAndroid& operator=(
      const DevToolsManagerDelegateAndroid&) = delete;

  ~DevToolsManagerDelegateAndroid() override;

 private:
  // content::DevToolsManagerDelegate implementation.
  content::BrowserContext* GetDefaultBrowserContext() override;
  std::string GetTargetType(content::WebContents* web_contents) override;
  content::DevToolsAgentHost::List RemoteDebuggingTargets(
      TargetType target_type) override;
  scoped_refptr<content::DevToolsAgentHost> CreateNewTarget(
      const GURL& url,
      TargetType target_type) override;
  bool IsBrowserTargetDiscoverable() override;
};

#endif  // CHROME_BROWSER_ANDROID_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_
