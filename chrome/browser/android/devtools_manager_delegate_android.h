// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/devtools_manager_delegate.h"

class ChromeDevToolsSessionAndroid;

class DevToolsManagerDelegateAndroid : public content::DevToolsManagerDelegate {
 public:
  DevToolsManagerDelegateAndroid();

  DevToolsManagerDelegateAndroid(const DevToolsManagerDelegateAndroid&) =
      delete;
  DevToolsManagerDelegateAndroid& operator=(
      const DevToolsManagerDelegateAndroid&) = delete;

  ~DevToolsManagerDelegateAndroid() override;

  static void MarkCreatedByDevTools(content::WebContents& web_contents);
  static bool IsCreatedByDevTools(const content::WebContents& web_contents);

 private:
  // content::DevToolsManagerDelegate implementation.
  std::vector<base::WeakPtr<content::BrowserContext>> GetBrowserContexts()
      override;
  content::BrowserContext* GetDefaultBrowserContext() override;
  content::BrowserContext* GetBrowserContext(
      const std::string& context_id) override;
  content::BrowserContext* CreateBrowserContext() override;
  void DisposeBrowserContext(content::BrowserContext* context,
                             DisposeCallback callback) override;
  std::string GetTargetType(content::WebContents* web_contents) override;
  content::DevToolsAgentHost::List RemoteDebuggingTargets(
      TargetType target_type) override;
  bool IsBrowserTargetDiscoverable() override;
  bool AllowInspectingTarget(content::DevToolsAgentHost* agent_host) override;

  void HandleCommand(content::DevToolsAgentHostClientChannel* channel,
                     base::span<const uint8_t> message,
                     NotHandledCallback callback) override;
  void ClientAttached(
      content::DevToolsAgentHostClientChannel* channel) override;
  void ClientDetached(
      content::DevToolsAgentHostClientChannel* channel) override;

  base::flat_map<content::DevToolsAgentHostClientChannel*,
                 std::unique_ptr<ChromeDevToolsSessionAndroid>>
      sessions_;
};

#endif  // CHROME_BROWSER_ANDROID_DEVTOOLS_MANAGER_DELEGATE_ANDROID_H_
