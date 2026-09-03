// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_pwc_manager.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/geic/geic_browser_host_impl.h"
#include "chrome/browser/geic/geic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_api_binder.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

namespace geic {

namespace {
const char kGeicPwcManagerUserDataKey[] = "geic_pwc_manager";
}  // namespace

// static
GURL GeicPwcManager::GetConfiguredGuestURL(Profile* profile) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kGeicGuestURLSwitch)) {
    GURL cmd_url(command_line->GetSwitchValueASCII(kGeicGuestURLSwitch));
    if (IsValidGuestUrl(cmd_url)) {
      return CanonicalizeGuestUrl(cmd_url);
    }
  }
  std::string param_url_str = features::kGeicGuestURL.Get();
  if (!param_url_str.empty()) {
    GURL param_url(param_url_str);
    if (IsValidGuestUrl(param_url)) {
      return CanonicalizeGuestUrl(param_url);
    }
  }
  return profile ? GetPolicyGuestUrl(profile) : GURL();
}

// static
GeicPwcManager* GeicPwcManager::GetOrCreateForProfile(Profile* profile,
                                                      GURL dev_url) {
  if (!profile) {
    return nullptr;
  }
  auto* manager = static_cast<GeicPwcManager*>(
      profile->GetUserData(kGeicPwcManagerUserDataKey));
  if (!manager) {
    if (dev_url.is_empty()) {
      dev_url = GetConfiguredGuestURL(profile);
    }
    auto new_manager =
        std::make_unique<GeicPwcManager>(profile, std::move(dev_url));
    manager = new_manager.get();
    profile->SetUserData(kGeicPwcManagerUserDataKey, std::move(new_manager));
  }
  return manager;
}

GeicPwcManager::GeicPwcManager(Profile* profile, GURL dev_url)
    : profile_(profile),
      dev_url_(dev_url.is_empty() ? GetConfiguredGuestURL(profile)
                                  : std::move(dev_url)) {
  if (profile_) {
    profile_observation_.Observe(profile_);
  }
}

GeicPwcManager::~GeicPwcManager() = default;

void GeicPwcManager::OnProfileWillBeDestroyed(Profile* profile) {
  entries_.clear();
  profile_observation_.Reset();
}

content::WebContents* GeicPwcManager::GetOrCreateWebContentsForTab(
    tabs::TabInterface* tab) {
  if (!tab || !profile_) {
    return nullptr;
  }
  auto it = entries_.find(tab->GetHandle());
  if (it != entries_.end()) {
    return it->second->web_contents();
  }

  if (dev_url_.is_empty() || !dev_url_.is_valid()) {
    DVLOG(1)
        << "GEiC guest URL is not configured; skipping PWC initialization.";
    return nullptr;
  }

  url::Origin origin = url::Origin::Create(dev_url_);
  auto policy_delegate = std::make_unique<pwc::FixedPwcPolicyDelegate>(
      /*navigation_allowlist=*/std::vector<url::Origin>{origin},
      /*capability_allowlist=*/std::vector<url::Origin>{origin});

  auto pwc = pwc::PrivilegedWebContents::Create(
      pwc::PrivilegedComponent::kGeic, profile_, std::move(policy_delegate));
  if (!pwc || !pwc->web_contents()) {
    return nullptr;
  }

  auto browser_host = std::make_unique<GeicBrowserHostImpl>(tab);
  auto entry = std::make_unique<TabEntry>(this, tab, std::move(pwc),
                                          std::move(browser_host));

  content::WebContents* wc = entry->web_contents();
  wc->GetController().LoadURL(dev_url_, content::Referrer(),
                              ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  entries_[tab->GetHandle()] = std::move(entry);
  return wc;
}

pwc::PrivilegedWebContents* GeicPwcManager::GetPwcForTab(
    tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  auto it = entries_.find(tab->GetHandle());
  if (it != entries_.end()) {
    return it->second->pwc();
  }
  return nullptr;
}

GeicBrowserHostImpl* GeicPwcManager::GetBrowserHostForPwc(
    pwc::PrivilegedWebContents* privileged) {
  if (!privileged) {
    return nullptr;
  }
  for (const auto& [handle, entry] : entries_) {
    if (entry->pwc() == privileged) {
      return entry->browser_host();
    }
  }
  return nullptr;
}

void GeicPwcManager::RemoveTab(tabs::TabHandle tab_handle) {
  entries_.erase(tab_handle);
}

// TabEntry implementation:
GeicPwcManager::TabEntry::TabEntry(
    GeicPwcManager* manager,
    tabs::TabInterface* tab,
    std::unique_ptr<pwc::PrivilegedWebContents> pwc,
    std::unique_ptr<GeicBrowserHostImpl> browser_host)
    : content::WebContentsObserver(pwc ? pwc->web_contents() : nullptr),
      manager_(manager),
      tab_handle_(tab ? tab->GetHandle() : tabs::TabHandle::Null()),
      pwc_(std::move(pwc)),
      browser_host_(std::move(browser_host)) {
  if (tab) {
    will_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
        &GeicPwcManager::TabEntry::OnTabWillDetach, base::Unretained(this)));
    will_discard_subscription_ = tab->RegisterWillDiscardContents(
        base::BindRepeating(&GeicPwcManager::TabEntry::OnTabWillDiscard,
                            base::Unretained(this)));
  }
}

GeicPwcManager::TabEntry::~TabEntry() {
  Observe(nullptr);
}

void GeicPwcManager::TabEntry::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  // TODO(crbug.com/539909218): Prototype shortcut. This exposes the entire
  // Mojo surface rather than only GeicApi, and the committed origin is not
  // checked against the capability allowlist here.
  if (render_frame_host->IsInPrimaryMainFrame()) {
    render_frame_host->EnableMojoJsBindings(/*features=*/nullptr);
  }
}

void GeicPwcManager::TabEntry::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(crbug.com/539909218): Prototype shortcut. This exposes the entire
  // Mojo surface rather than only GeicApi, and the committed origin is not
  // checked against the capability allowlist here.
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->GetRenderFrameHost()) {
    navigation_handle->GetRenderFrameHost()->EnableMojoJsBindings(
        /*features=*/nullptr);
  }
}

void GeicPwcManager::TabEntry::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&GeicPwcManager::RemoveTab,
                       manager_->weak_factory_.GetWeakPtr(), tab_handle_));
  }
}

void GeicPwcManager::TabEntry::OnTabWillDiscard(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&GeicPwcManager::RemoveTab,
                     manager_->weak_factory_.GetWeakPtr(), tab_handle_));
}

void BindGeicBrowserHost(
    content::RenderFrameHost* rfh,
    mojo::PendingReceiver<mojom::GeicBrowserHost> receiver) {
  pwc::PrivilegedWebContents* privileged = pwc::EnforceCapabilityGate(rfh);
  if (!privileged) {
    return;
  }

  if (privileged->component() != pwc::PrivilegedComponent::kGeic) {
    bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                    bad_message::PWC_BRIDGE_UNQUALIFIED_FRAME);
    return;
  }

  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
  auto* manager = GeicPwcManager::GetOrCreateForProfile(profile);
  if (manager) {
    GeicBrowserHostImpl* host = manager->GetBrowserHostForPwc(privileged);
    if (host) {
      host->BindBrowserHost(std::move(receiver));
    }
  }
}

}  // namespace geic
