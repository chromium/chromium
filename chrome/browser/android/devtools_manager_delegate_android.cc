// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/devtools_manager_delegate_android.h"

#include <map>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/android/tab_android.h"
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
  ~ClientProxy() override {}

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override {
    proxy_->DispatchOnClientHost(message);
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host) override {
    proxy_->ConnectionClosed();
  }

 private:
  content::DevToolsExternalAgentProxy* proxy_;
  DISALLOW_COPY_AND_ASSIGN(ClientProxy);
};

class TabProxyDelegate : public content::DevToolsExternalAgentProxyDelegate {
 public:
  explicit TabProxyDelegate(TabAndroid* tab)
      : tab_id_(tab->GetAndroidId()),
        title_(base::UTF16ToUTF8(tab->GetTitle())),
        url_(tab->GetURL()),
        agent_host_(tab->web_contents()
                        ? DevToolsAgentHost::GetOrCreateFor(tab->web_contents())
                        : nullptr) {}

  ~TabProxyDelegate() override {}

  void Attach(content::DevToolsExternalAgentProxy* proxy) override {
    proxies_[proxy].reset(new ClientProxy(proxy));
    MaterializeAgentHost();
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
    if (proxies_.empty())
      agent_host_ = nullptr;
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
                            const std::string& message) override {
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
    agent_host_ = DevToolsAgentHost::GetOrCreateFor(web_contents);
  }

  bool FindTab(TabModel** model_result, int* index_result) const {
    for (TabModelList::const_iterator iter = TabModelList::begin();
        iter != TabModelList::end(); ++iter) {
      TabModel* model = *iter;
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
  scoped_refptr<DevToolsAgentHost> agent_host_;
  std::map<content::DevToolsExternalAgentProxy*, std::unique_ptr<ClientProxy>>
      proxies_;
  DISALLOW_COPY_AND_ASSIGN(TabProxyDelegate);
};

scoped_refptr<DevToolsAgentHost> DevToolsAgentHostForTab(TabAndroid* tab) {
  scoped_refptr<DevToolsAgentHost> result = tab->GetDevToolsAgentHost();
  if (result)
    return result;

  result = DevToolsAgentHost::Forward(base::NumberToString(tab->GetAndroidId()),
                                      std::make_unique<TabProxyDelegate>(tab));
  tab->SetDevToolsAgentHost(result);
  return result;
}

} //  namespace

DevToolsManagerDelegateAndroid::DevToolsManagerDelegateAndroid() = default;

DevToolsManagerDelegateAndroid::~DevToolsManagerDelegateAndroid() = default;

std::string DevToolsManagerDelegateAndroid::GetTargetType(
    content::WebContents* web_contents) {
  TabAndroid* tab = web_contents ? TabAndroid::FromWebContents(web_contents)
      : nullptr;
  return tab ? DevToolsAgentHost::kTypePage :
      DevToolsAgentHost::kTypeOther;
}

DevToolsAgentHost::List
DevToolsManagerDelegateAndroid::RemoteDebuggingTargets() {
  // Enumerate existing tabs, including the ones with no WebContents.
  DevToolsAgentHost::List result;
  std::set<WebContents*> tab_web_contents;
  for (TabModelList::const_iterator iter = TabModelList::begin();
      iter != TabModelList::end(); ++iter) {
    TabModel* model = *iter;
    for (int i = 0; i < model->GetTabCount(); ++i) {
      TabAndroid* tab = model->GetTabAt(i);
      if (!tab)
        continue;

      if (tab->web_contents())
        tab_web_contents.insert(tab->web_contents());
      result.push_back(DevToolsAgentHostForTab(tab));
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
DevToolsManagerDelegateAndroid::CreateNewTarget(const GURL& url) {
  if (TabModelList::empty())
    return nullptr;

  TabModel* tab_model = TabModelList::get(0);
  if (!tab_model)
    return nullptr;

  WebContents* web_contents = tab_model->CreateNewTabForDevTools(url);
  if (!web_contents)
    return nullptr;

  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  return tab ? DevToolsAgentHostForTab(tab) : nullptr;
}

std::string DevToolsManagerDelegateAndroid::GetDiscoveryPageHTML() {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      IDR_DEVTOOLS_DISCOVERY_PAGE_HTML);
}

bool DevToolsManagerDelegateAndroid::IsBrowserTargetDiscoverable() {
  return true;
}
