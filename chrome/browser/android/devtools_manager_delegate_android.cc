// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/devtools_manager_delegate_android.h"

#include <map>
#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/resource/resource_bundle.h"

using content::DevToolsAgentHost;
using content::WebContents;

namespace {

class ClientProxy : public content::DevToolsAgentHostClient {
 public:
  explicit ClientProxy(content::DevToolsExternalAgentProxy* proxy)
      : proxy_(proxy) {}

  ClientProxy(const ClientProxy&) = delete;
  ClientProxy& operator=(const ClientProxy&) = delete;

  ~ClientProxy() override {}

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    proxy_->DispatchOnClientHost(message);
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host) override {
    proxy_->ConnectionClosed();
  }

 private:
  raw_ptr<content::DevToolsExternalAgentProxy> proxy_;
};

class TabProxyDelegate : public content::DevToolsExternalAgentProxyDelegate {
 public:
  TabProxyDelegate(TabAndroid* tab, bool use_tab_target)
      : tab_id_(tab->GetAndroidId()),
        title_(base::UTF16ToUTF8(tab->GetTitle())),
        url_(tab->GetURL()),
        use_tab_target_(use_tab_target) {
    if (tab->web_contents()) {
      agent_host_ =
          use_tab_target
              ? DevToolsAgentHost::GetOrCreateForTab(tab->web_contents())
              : DevToolsAgentHost::GetOrCreateFor(tab->web_contents());
    }
  }
  TabProxyDelegate(const TabProxyDelegate&) = delete;
  TabProxyDelegate& operator=(const TabProxyDelegate&) = delete;

  ~TabProxyDelegate() override {}

  void Attach(content::DevToolsExternalAgentProxy* proxy) override {
    proxies_[proxy] = std::make_unique<ClientProxy>(proxy);
    if (agent_host_)
      agent_host_->AttachClient(proxies_[proxy].get());
  }

  void Detach(content::DevToolsExternalAgentProxy* proxy) override {
    auto it = proxies_.find(proxy);
    if (it == proxies_.end())
      return;
    if (agent_host_)
      agent_host_->DetachClient(it->second.get());
    proxies_.erase(it);
    if (proxies_.empty()) {
      agent_host_ = nullptr;
      MaterializeAgentHost();
    }
  }

  std::string GetType() override {
    return agent_host_ ? agent_host_->GetType() : DevToolsAgentHost::kTypePage;
  }

  std::string GetTitle() override {
    return agent_host_ ? agent_host_->GetTitle() : title_;
  }

  std::string GetDescription() override {
    return agent_host_ ? agent_host_->GetDescription() : "";
  }

  GURL GetURL() override {
    return agent_host_ ? agent_host_->GetURL() : url_;
  }

  GURL GetFaviconURL() override {
    return agent_host_ ? agent_host_->GetFaviconURL() : GURL();
  }

  std::string GetFrontendURL() override {
    return std::string();
  }

  bool Activate() override {
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return false;
    model->SetActiveIndex(index);
    return true;
  }

  void Reload() override {
    MaterializeAgentHost();
    if (agent_host_)
      agent_host_->Reload();
  }

  bool Close() override {
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return false;
    model->CloseTabAt(index);
    return true;
  }

  base::TimeTicks GetLastActivityTime() override {
    return agent_host_ ? agent_host_->GetLastActivityTime() : base::TimeTicks();
  }

  void SendMessageToBackend(content::DevToolsExternalAgentProxy* proxy,
                            base::span<const uint8_t> message) override {
    auto it = proxies_.find(proxy);
    if (it == proxies_.end())
      return;
    if (agent_host_)
      agent_host_->DispatchProtocolMessage(it->second.get(), message);
  }

 private:
  void MaterializeAgentHost() {
    if (agent_host_)
      return;
    TabModel* model;
    int index;
    if (!FindTab(&model, &index))
      return;
    WebContents* web_contents = model->GetWebContentsAt(index);
    if (!web_contents)
      return;
    agent_host_ = use_tab_target_
                      ? DevToolsAgentHost::GetOrCreateForTab(web_contents)
                      : DevToolsAgentHost::GetOrCreateFor(web_contents);
  }

  bool FindTab(TabModel** model_result, int* index_result) const {
    for (TabModel* model : TabModelList::models()) {
      for (int i = 0; i < model->GetTabCount(); ++i) {
        TabAndroid* tab = model->GetTabAt(i);
        if (tab && tab->GetAndroidId() == tab_id_) {
          *model_result = model;
          *index_result = i;
          return true;
        }
      }
    }
    return false;
  }

  const int tab_id_;
  const std::string title_;
  const GURL url_;
  const bool use_tab_target_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  std::map<content::DevToolsExternalAgentProxy*, std::unique_ptr<ClientProxy>>
      proxies_;
};

scoped_refptr<DevToolsAgentHost> DevToolsAgentHostForTab(TabAndroid* tab,
                                                         bool use_tab_target) {
  scoped_refptr<DevToolsAgentHost> result = tab->GetDevToolsAgentHost();
  if (result)
    return result;

  result = DevToolsAgentHost::Forward(
      base::NumberToString(tab->GetAndroidId()),
      std::make_unique<TabProxyDelegate>(tab, use_tab_target));
  tab->SetDevToolsAgentHost(result);
  return result;
}

static const void* const kCreatedByDevTools = &kCreatedByDevTools;

bool IsCreatedByDevTools(const WebContents& web_contents) {
  return !!web_contents.GetUserData(kCreatedByDevTools);
}

void MarkCreatedByDevTools(WebContents& web_contents) {
  DCHECK(!IsCreatedByDevTools(web_contents));
  web_contents.SetUserData(kCreatedByDevTools,
                           std::make_unique<base::SupportsUserData::Data>());
}

} //  namespace

DevToolsManagerDelegateAndroid::DevToolsManagerDelegateAndroid() = default;

DevToolsManagerDelegateAndroid::~DevToolsManagerDelegateAndroid() = default;

content::BrowserContext*
DevToolsManagerDelegateAndroid::GetDefaultBrowserContext() {
  return ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
}

std::string DevToolsManagerDelegateAndroid::GetTargetType(
    content::WebContents* web_contents) {
  TabAndroid* tab = web_contents ? TabAndroid::FromWebContents(web_contents)
      : nullptr;
  return tab ? DevToolsAgentHost::kTypePage :
      DevToolsAgentHost::kTypeOther;
}

DevToolsAgentHost::List DevToolsManagerDelegateAndroid::RemoteDebuggingTargets(
    DevToolsManagerDelegate::TargetType target_type) {
  // Enumerate existing tabs, including the ones with no WebContents.
  DevToolsAgentHost::List result;
  std::set<WebContents*> tab_web_contents;
  for (const TabModel* model : TabModelList::models()) {
    for (int i = 0; i < model->GetTabCount(); ++i) {
      TabAndroid* tab = model->GetTabAt(i);
      if (!tab)
        continue;

      WebContents* wc = tab->web_contents();
      // For web contents created programmatically by CDP clients, do not create
      // tab proxies to avoid clients being confused by the fact they get more
      // targets than they create and match the behavior of desktop chrome.
      if (!wc || !IsCreatedByDevTools(*wc)) {
        result.push_back(DevToolsAgentHostForTab(
            tab, target_type == DevToolsManagerDelegate::kTab));
        if (wc) {
          tab_web_contents.insert(wc);
        }
      }
    }
  }

  // Add descriptors for targets not associated with any tabs.
  DevToolsAgentHost::List agents = DevToolsAgentHost::GetOrCreateAll();
  for (DevToolsAgentHost::List::iterator it = agents.begin();
       it != agents.end(); ++it) {
    if (WebContents* web_contents = (*it)->GetWebContents()) {
      if (tab_web_contents.find(web_contents) != tab_web_contents.end())
        continue;
    }
    result.push_back(*it);
  }

  return result;
}

scoped_refptr<DevToolsAgentHost>
DevToolsManagerDelegateAndroid::CreateNewTarget(
    const GURL& url,
    DevToolsManagerDelegate::TargetType target_type) {
  if (TabModelList::models().empty())
    return nullptr;

  TabModel* tab_model = TabModelList::models()[0];
  if (!tab_model)
    return nullptr;

  WebContents* web_contents = tab_model->CreateNewTabForDevTools(url);
  if (!web_contents)
    return nullptr;

  MarkCreatedByDevTools(*web_contents);
  return target_type == DevToolsManagerDelegate::kTab
             ? DevToolsAgentHost::GetOrCreateForTab(web_contents)
             : DevToolsAgentHost::GetOrCreateFor(web_contents);
}

bool DevToolsManagerDelegateAndroid::IsBrowserTargetDiscoverable() {
  return true;
}
