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
#include "chrome/browser/devtools/global_confirm_info_bar.h"
#include "chrome/browser/devtools/protocol/protocol.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/devtools_manager_delegate.h"
#include "net/base/host_port_pair.h"

class ChromeDevToolsSession;
class ScopedKeepAlive;
using RemoteLocations = std::set<net::HostPortPair>;

class ChromeDevToolsManagerDelegate : public content::DevToolsManagerDelegate,
                                      public ConfirmInfoBarDelegate::Observer {
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
  scoped_refptr<content::DevToolsAgentHost> GetDevToolsAgentHost(
      content::DevToolsAgentHost* agent_host) override;
  scoped_refptr<content::DevToolsAgentHost> OpenDevTools(
      content::DevToolsAgentHost* agent_host,
      const content::DevToolsManagerDelegate::DevToolsOptions& devtools_options)
      override;
  void Activate(content::DevToolsAgentHost* agent_host) override;
  void HandleCommand(content::DevToolsAgentHostClientChannel* channel,
                     base::span<const uint8_t> message,
                     NotHandledCallback callback) override;
  std::string GetTargetType(content::WebContents* web_contents) override;
  std::string GetTargetTitle(content::WebContents* web_contents) override;
  std::optional<bool> ShouldReportAsTabTarget(
      content::WebContents* web_contents) override;

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
      TargetType target_type,
      bool new_window) override;
  bool HasBundledFrontendResources() override;
  void AcceptDebugging(AcceptCallback) override;
  void SetActiveWebSocketConnections(size_t count) override;

  void DevicesAvailable(
      const DevToolsDeviceDiscovery::CompleteDevices& devices);

  // ConfirmInfoBarDelegate::Observer
  void OnAccept() override;
  void OnDismiss() override;

  raw_ptr<GlobalConfirmInfoBar> infobar_ = nullptr;
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
