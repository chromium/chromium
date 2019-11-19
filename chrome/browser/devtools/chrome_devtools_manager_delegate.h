// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/devtools/device/devtools_device_discovery.h"
#include "chrome/browser/devtools/protocol/forward.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "net/base/host_port_pair.h"

class ChromeDevToolsSession;
using RemoteLocations = std::set<net::HostPortPair>;

namespace extensions {
class Extension;
}

class ChromeDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  static const char kTypeApp[];
  static const char kTypeBackgroundPage[];

  ChromeDevToolsManagerDelegate();
  ~ChromeDevToolsManagerDelegate() override;

  static ChromeDevToolsManagerDelegate* GetInstance();
  void UpdateDeviceDiscovery();

  // |web_contents| may be null, in which case this function just checks
  // the settings for |profile|.
  static bool AllowInspection(Profile* profile,
                              content::WebContents* web_contents);

  // |extension| may be null, in which case this function just checks
  // the settings for |profile|.
  static bool AllowInspection(Profile* profile,
                              const extensions::Extension* extension);

  // Resets |device_manager_|.
  void ResetAndroidDeviceManagerForTesting();

  std::vector<content::BrowserContext*> GetBrowserContexts() override;
  content::BrowserContext* GetDefaultBrowserContext() override;

 private:
  friend class DevToolsManagerDelegateTest;

  // content::DevToolsManagerDelegate implementation.
  void Inspect(content::DevToolsAgentHost* agent_host) override;
  void HandleCommand(content::DevToolsAgentHost* agent_host,
                     content::DevToolsAgentHostClient* client,
                     const std::string& method,
                     const std::string& message,
                     NotHandledCallback callback) override;
  std::string GetTargetType(content::WebContents* web_contents) override;
  std::string GetTargetTitle(content::WebContents* web_contents) override;

  content::BrowserContext* CreateBrowserContext() override;
  void DisposeBrowserContext(content::BrowserContext*,
                             DisposeCallback callback) override;

  bool AllowInspectingRenderFrameHost(content::RenderFrameHost* rfh) override;
  void ClientAttached(content::DevToolsAgentHost* agent_host,
                      content::DevToolsAgentHostClient* client) override;
  void ClientDetached(content::DevToolsAgentHost* agent_host,
                      content::DevToolsAgentHostClient* client) override;
  scoped_refptr<content::DevToolsAgentHost> CreateNewTarget(
      const GURL& url) override;
  std::string GetDiscoveryPageHTML() override;
  bool HasBundledFrontendResources() override;

  void DevicesAvailable(
      const DevToolsDeviceDiscovery::CompleteDevices& devices);

  std::map<content::DevToolsAgentHostClient*,
           std::unique_ptr<ChromeDevToolsSession>>
      sessions_;

  std::unique_ptr<AndroidDeviceManager> device_manager_;
  std::unique_ptr<DevToolsDeviceDiscovery> device_discovery_;
  content::DevToolsAgentHost::List remote_agent_hosts_;
  RemoteLocations remote_locations_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDevToolsManagerDelegate);
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_
