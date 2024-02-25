// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "chrome/browser/devtools/device/devtools_device_discovery.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "net/base/host_port_pair.h"

class ChromeDevToolsSession;
class Profile;
class ScopedKeepAlive;
using RemoteLocations = std::set<net::HostPortPair>;

namespace extensions {
class Extension;
}

namespace web_app {
class WebApp;
}

class ChromeDevToolsManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  static const char kTypeApp[];
  static const char kTypeBackgroundPage[];
  static const char kTypePage[];

  ChromeDevToolsManagerDelegate();

  ChromeDevToolsManagerDelegate(const ChromeDevToolsManagerDelegate&) = delete;
  ChromeDevToolsManagerDelegate& operator=(
      const ChromeDevToolsManagerDelegate&) = delete;

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

  // |web_app| may be null, in which case this function just checks
  // the settings for |profile|.
  static bool AllowInspection(Profile* profile, const web_app::WebApp* web_app);

  // Resets |device_manager_|.
  void ResetAndroidDeviceManagerForTesting();

  std::vector<content::BrowserContext*> GetBrowserContexts() override;
  content::BrowserContext* GetDefaultBrowserContext() override;

  // Closes browser soon, not in the current task.
  static void CloseBrowserSoon();

  // Release browser keep alive allowing browser to close.
  static void AllowBrowserToClose();

 private:
  friend class DevToolsManagerDelegateTest;

  // content::DevToolsManagerDelegate implementation.
  void Inspect(content::DevToolsAgentHost* agent_host) override;
  void Activate(content::DevToolsAgentHost* agent_host) override;
  void HandleCommand(content::DevToolsAgentHostClientChannel* channel,
                     base::span<const uint8_t> message,
                     NotHandledCallback callback) override;
  std::string GetTargetType(content::WebContents* web_contents) override;
  std::string GetTargetTitle(content::WebContents* web_contents) override;

  content::BrowserContext* CreateBrowserContext() override;
  void DisposeBrowserContext(content::BrowserContext*,
                             DisposeCallback callback) override;

  bool AllowInspectingRenderFrameHost(content::RenderFrameHost* rfh) override;
  void ClientAttached(
      content::DevToolsAgentHostClientChannel* channel) override;
  void ClientDetached(
      content::DevToolsAgentHostClientChannel* channel) override;
  scoped_refptr<content::DevToolsAgentHost> CreateNewTarget(
      const GURL& url,
      TargetType target_type) override;
  bool HasBundledFrontendResources() override;

  void DevicesAvailable(
      const DevToolsDeviceDiscovery::CompleteDevices& devices);

  std::map<content::DevToolsAgentHostClientChannel*,
           std::unique_ptr<ChromeDevToolsSession>>
      sessions_;

  std::unique_ptr<AndroidDeviceManager> device_manager_;
  std::unique_ptr<DevToolsDeviceDiscovery> device_discovery_;
  content::DevToolsAgentHost::List remote_agent_hosts_;
  RemoteLocations remote_locations_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_CHROME_DEVTOOLS_MANAGER_DELEGATE_H_
