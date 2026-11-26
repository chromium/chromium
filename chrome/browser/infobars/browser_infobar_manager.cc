// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/browser_infobar_manager.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/confirm_infobar_creation_allowlist.h"
#include "chrome/browser/infobars/infobar_outcome_tracker.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/infobars/registry_infobar_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace infobars {

namespace {

// Returns the browser whose `WebContents` shows `infobar`, if any.
BrowserWindowInterface* BrowserForInfoBar(infobars::InfoBar* infobar) {
  content::WebContents* web_contents =
      static_cast<RegistryInfoBarDelegate*>(infobar->delegate())
          ->web_contents();
  if (!web_contents) {
    return nullptr;
  }
  if (tabs::TabInterface* tab =
          tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    return tab->GetBrowserWindowInterface();
  }
  return webui::GetBrowserWindowInterface(web_contents);
}

}  // namespace

// Tracks live `kTab` instances so `Hide()` can distinguish them from global
// and legacy/untracked instances, unobserving each `InfoBarManager` once its
// last instance leaves.
class BrowserInfoBarManager::TabInfoBarTracker final
    : public InfoBarManager::Observer {
 public:
  TabInfoBarTracker() = default;
  TabInfoBarTracker(const TabInfoBarTracker&) = delete;
  TabInfoBarTracker& operator=(const TabInfoBarTracker&) = delete;
  ~TabInfoBarTracker() override = default;

  void Track(InfoBar* infobar) {
    CHECK(infobar);
    InfoBarManager* manager = infobar->owner();
    CHECK(manager);
    instances_.emplace(infobar, manager);
    if (!observations_.IsObservingSource(manager)) {
      observations_.AddObservation(manager);
    }
  }

  bool Contains(const InfoBar* infobar) const {
    return instances_.contains(infobar);
  }

  std::vector<InfoBar*> Snapshot() const {
    std::vector<InfoBar*> snapshot;
    snapshot.reserve(instances_.size());
    for (const auto& [infobar, manager] : instances_) {
      snapshot.push_back(infobar);
    }
    return snapshot;
  }

  // InfoBarManager::Observer:
  void OnInfoBarRemoved(InfoBar* infobar, bool animate) override {
    Untrack(infobar);
  }

  void OnInfoBarReplaced(InfoBar* old_infobar, InfoBar* new_infobar) override {
    Untrack(old_infobar);
  }

  void OnManagerWillBeDestroyed(InfoBarManager* manager) override {
    observations_.RemoveObservation(manager);
    std::erase_if(instances_, [manager](const auto& entry) {
      return entry.second == manager;
    });
  }

 private:
  void Untrack(InfoBar* infobar) {
    auto it = instances_.find(infobar);
    if (it == instances_.end()) {
      return;
    }
    InfoBarManager* manager = it->second;
    instances_.erase(it);
    if (observations_.IsObservingSource(manager) &&
        std::ranges::none_of(instances_, [manager](const auto& entry) {
          return entry.second == manager;
        })) {
      observations_.RemoveObservation(manager);
    }
  }

  std::map<raw_ptr<InfoBar>, raw_ptr<InfoBarManager>, std::less<>> instances_;
  base::ScopedMultiSourceObservation<InfoBarManager, InfoBarManager::Observer>
      observations_{this};
};

struct BrowserInfoBarManager::GlobalInfoBarContext {
  scoped_refptr<const SharedInfoBarSpec> spec;
  InfoBarShowParams params;
  scoped_refptr<InfoBarOutcomeTracker> outcome;
  base::RepeatingClosure on_global_close;
  std::map<infobars::InfoBarManager*, infobars::InfoBar*> active_instances;
};

DEFINE_USER_DATA(BrowserInfoBarManager);

BrowserInfoBarManager::BrowserInfoBarManager(BrowserProcess* browser_process)
    : local_state_(browser_process->local_state()),
      scoped_unowned_user_data_(browser_process->GetUnownedUserDataHost(),
                                *this),
      tab_infobars_(std::make_unique<TabInfoBarTracker>()) {}

BrowserInfoBarManager::~BrowserInfoBarManager() = default;

// static
BrowserInfoBarManager* BrowserInfoBarManager::From(
    BrowserProcess* browser_process) {
  return browser_process ? Get(browser_process->GetUnownedUserDataHost())
                         : nullptr;
}

void BrowserInfoBarManager::Register(InfoBarSpec spec) {
  const auto identifier = spec.identifier();
  CHECK(!registered_specs_.contains(identifier));
  registered_specs_.emplace(
      identifier, base::MakeRefCounted<SharedInfoBarSpec>(std::move(spec)));
}

bool BrowserInfoBarManager::IsRegistered(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) const {
  return registered_specs_.contains(identifier);
}

infobars::InfoBar* BrowserInfoBarManager::Show(
    tabs::TabInterface* tab,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  return Show(tab, identifier, InfoBarShowParams());
}

infobars::InfoBar* BrowserInfoBarManager::Show(
    tabs::TabInterface* tab,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    InfoBarShowParams params) {
  if (!registered_specs_.contains(identifier)) {
    return nullptr;
  }
  CHECK(tab);
  return ShowInternal(tab->GetContents(), identifier, std::move(params));
}

infobars::InfoBar* BrowserInfoBarManager::Show(
    content::WebContents* web_contents,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  return Show(web_contents, identifier, InfoBarShowParams());
}

infobars::InfoBar* BrowserInfoBarManager::Show(
    content::WebContents* web_contents,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    InfoBarShowParams params) {
  if (!registered_specs_.contains(identifier)) {
    return nullptr;
  }
  CHECK(web_contents);
  return ShowInternal(web_contents, identifier, std::move(params));
}

infobars::InfoBar* BrowserInfoBarManager::ShowInternal(
    content::WebContents* web_contents,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    InfoBarShowParams params) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return nullptr;
  }
  const scoped_refptr<const SharedInfoBarSpec>& shared_spec = it->second;
  const InfoBarSpec& spec = shared_spec->data;
  CHECK(params.scope.value_or(spec.scope()) == InfoBarScope::kTab);
  if (IsBlockedByPolicy(spec)) {
    base::UmaHistogramSparse("InfoBar.Centralized.SuppressedByPolicy",
                             identifier);
    return nullptr;
  }

  if (!web_contents) {
    return nullptr;
  }

  auto* manager = ContentInfoBarManager::FromWebContents(web_contents);
  if (!manager) {
    return nullptr;
  }
  auto outcome = base::MakeRefCounted<InfoBarOutcomeTracker>(
      identifier,
      params.result_callback ? params.result_callback : spec.result_callback());
  auto* added_infobar = manager->AddInfoBar(CreateRegistryInfoBar(
      ConfirmInfoBarCreationAllowlist::GetPassKey(), shared_spec, web_contents,
      std::move(params), outcome));
  if (added_infobar &&
      std::ranges::contains(manager->infobars(), added_infobar)) {
    outcome->Arm();
    tab_infobars_->Track(added_infobar);
    base::UmaHistogramSparse("InfoBar.Centralized.Show", identifier);
    return added_infobar;
  }
  return nullptr;
}

infobars::InfoBar* BrowserInfoBarManager::Replace(
    infobars::InfoBar* old_infobar,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    InfoBarShowParams params) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return nullptr;
  }
  CHECK(old_infobar);
  const scoped_refptr<const SharedInfoBarSpec>& shared_spec = it->second;
  const InfoBarSpec& spec = shared_spec->data;
  CHECK(params.scope.value_or(spec.scope()) == InfoBarScope::kTab);
  if (IsBlockedByPolicy(spec)) {
    base::UmaHistogramSparse("InfoBar.Centralized.SuppressedByPolicy",
                             identifier);
    return nullptr;
  }

  infobars::InfoBarManager* manager = old_infobar->owner();
  if (!manager) {
    return nullptr;
  }

  auto* old_delegate =
      static_cast<RegistryInfoBarDelegate*>(old_infobar->delegate());
  content::WebContents* contents = old_delegate->web_contents();
  if (!contents) {
    return nullptr;
  }

  old_delegate->outcome()->Suppress();
  auto outcome = base::MakeRefCounted<InfoBarOutcomeTracker>(
      identifier,
      params.result_callback ? params.result_callback : spec.result_callback());
  auto* replaced_infobar = manager->ReplaceInfoBar(
      old_infobar,
      CreateRegistryInfoBar(ConfirmInfoBarCreationAllowlist::GetPassKey(),
                            shared_spec, contents, std::move(params), outcome));
  if (replaced_infobar &&
      std::ranges::contains(manager->infobars(), replaced_infobar)) {
    outcome->Arm();
    tab_infobars_->Track(replaced_infobar);
    return replaced_infobar;
  }
  return nullptr;
}

bool BrowserInfoBarManager::ShowGlobally(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  return ShowGlobally(identifier, InfoBarShowParams());
}

bool BrowserInfoBarManager::ShowGlobally(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    InfoBarShowParams params) {
  CHECK(!params.scope.has_value());
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return false;
  }
  const scoped_refptr<const SharedInfoBarSpec>& shared_spec = it->second;
  const InfoBarSpec& spec = shared_spec->data;
  CHECK(spec.scope() == InfoBarScope::kGlobal);
  if (IsBlockedByPolicy(spec)) {
    base::UmaHistogramSparse("InfoBar.Centralized.SuppressedByPolicy",
                             identifier);
    return false;
  }

  if (active_global_infobars_.contains(identifier)) {
    return false;
  }

  StartObservingBrowsersIfNeeded();

  auto outcome = base::MakeRefCounted<InfoBarOutcomeTracker>(
      identifier,
      params.result_callback ? params.result_callback : spec.result_callback());
  outcome->Arm();
  auto on_global_close = base::BindRepeating(
      [](base::WeakPtr<BrowserInfoBarManager> self,
         InfoBarDelegate::InfoBarIdentifier id) {
        if (self) {
          self->Hide(id);
        }
      },
      weak_factory_.GetWeakPtr(), identifier);

  auto context_ptr = std::make_unique<GlobalInfoBarContext>(
      GlobalInfoBarContext{.spec = shared_spec,
                           .params = std::move(params),
                           .outcome = std::move(outcome),
                           .on_global_close = std::move(on_global_close)});
  active_global_infobars_[identifier] = std::move(context_ptr);
  active_global_order_.push_back(identifier);

  bool added_any_infobars = false;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [this, identifier, &added_any_infobars](BrowserWindowInterface* browser) {
        auto ctx_it = active_global_infobars_.find(identifier);
        if (ctx_it == active_global_infobars_.end()) {
          return false;
        }
        if (const auto browser_filter =
                ctx_it->second->spec->data.browser_filter();
            browser_filter && !browser_filter.Run(browser)) {
          return true;
        }
        // Re-check after `browser_filter` in case it re-entrantly hid the
        // infobar.
        ctx_it = active_global_infobars_.find(identifier);
        if (ctx_it == active_global_infobars_.end()) {
          return false;
        }
        GlobalInfoBarContext* context = ctx_it->second.get();

        tabs::TabInterface* active_tab = browser->GetActiveTabInterface();
        content::WebContents* active_contents =
            active_tab ? active_tab->GetContents() : nullptr;
        if (!active_contents) {
          return true;
        }
        auto* manager = ContentInfoBarManager::FromWebContents(active_contents);
        if (!manager) {
          return true;
        }
        auto infobar = CreateRegistryInfoBar(
            ConfirmInfoBarCreationAllowlist::GetPassKey(), context->spec,
            active_contents, context->params, context->outcome,
            context->on_global_close);
        if (!active_global_infobars_.contains(identifier)) {
          return false;
        }
        auto* added_infobar = manager->AddInfoBar(std::move(infobar));
        if (!added_infobar ||
            !std::ranges::contains(manager->infobars(), added_infobar)) {
          return active_global_infobars_.contains(identifier);
        }
        // Re-check after `AddInfoBar()` in case an observer re-entrantly hid
        // the infobar.
        ctx_it = active_global_infobars_.find(identifier);
        if (ctx_it == active_global_infobars_.end()) {
          RemoveRegistryInfoBar(manager, added_infobar,
                                /*suppress_outcome=*/true);
          return false;
        }
        ctx_it->second->active_instances[manager] = added_infobar;
        added_any_infobars = true;
        if (!infobar_manager_observations_.IsObservingSource(manager)) {
          infobar_manager_observations_.AddObservation(manager);
        }
        return true;
      });

  if (added_any_infobars) {
    base::UmaHistogramSparse("InfoBar.Centralized.Show", identifier);
  } else {
    // A false return must mean nothing is showing and nothing will appear
    // later, so drop the armed context without reporting an outcome.
    if (auto ctx_it = active_global_infobars_.find(identifier);
        ctx_it != active_global_infobars_.end()) {
      ctx_it->second->outcome->Suppress();
      active_global_infobars_.erase(ctx_it);
    }
    std::erase(active_global_order_, identifier);
    StopObservingBrowsersIfIdle();
  }
  return added_any_infobars;
}

void BrowserInfoBarManager::Hide(
    content::WebContents* web_contents,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return;
  }
  CHECK(web_contents);

  auto* manager = ContentInfoBarManager::FromWebContents(web_contents);
  if (!manager) {
    return;
  }

  for (infobars::InfoBar* infobar : manager->infobars()) {
    if (infobar->delegate()->GetIdentifier() == identifier &&
        tab_infobars_->Contains(infobar)) {
      RemoveRegistryInfoBar(manager, infobar, /*suppress_outcome=*/true);
      break;
    }
  }
}

void BrowserInfoBarManager::Hide(infobars::InfoBar* infobar) {
  CHECK(infobar);
  infobars::InfoBarManager* owner = infobar->owner();
  if (!owner) {
    return;
  }
  RemoveRegistryInfoBar(owner, infobar, /*suppress_outcome=*/true);
}

void BrowserInfoBarManager::Hide(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    const InfoBarSpec::BrowserFilter& browser_filter) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return;
  }

  if (it->second->data.scope() == InfoBarScope::kTab) {
    for (infobars::InfoBar* infobar : tab_infobars_->Snapshot()) {
      // A removal can take other instances with it, so check membership before
      // touching one.
      if (!tab_infobars_->Contains(infobar) ||
          infobar->delegate()->GetIdentifier() != identifier) {
        continue;
      }

      if (browser_filter) {
        BrowserWindowInterface* browser = BrowserForInfoBar(infobar);
        if (!browser || !browser_filter.Run(browser)) {
          continue;
        }
      }

      Hide(infobar);
    }
    return;
  }

  // A global infobar is hidden everywhere.
  CHECK(!browser_filter);

  HideGlobalInternal(identifier, /*suppress_outcome=*/true);
}

void BrowserInfoBarManager::HideGlobalInternal(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    bool suppress_outcome) {
  auto active_it = active_global_infobars_.find(identifier);
  if (active_it == active_global_infobars_.end()) {
    return;
  }
  if (suppress_outcome) {
    active_it->second->outcome->Suppress();
  }
  while (true) {
    active_it = active_global_infobars_.find(identifier);
    if (active_it == active_global_infobars_.end()) {
      return;
    }
    auto& manager_map = active_it->second->active_instances;
    if (manager_map.empty()) {
      break;
    }
    auto map_it = manager_map.begin();
    infobars::InfoBarManager* manager = map_it->first;
    infobars::InfoBar* infobar = map_it->second;

    manager_map.erase(map_it);  // Erase first to signal programmatic removal.
    RemoveRegistryInfoBar(manager, infobar, /*suppress_outcome=*/false);
  }
  active_global_infobars_.erase(active_it);
  std::erase(active_global_order_, identifier);
  StopObservingBrowsersIfIdle();
}

void BrowserInfoBarManager::PrepareForActiveTabChange(
    BrowserWindowInterface* browser) {
  PopulateGlobalInfoBarsForActiveTab(browser);
}

void BrowserInfoBarManager::OnBrowserCreated(BrowserWindowInterface* browser) {
  active_tab_subscriptions_[browser] =
      browser->RegisterActiveTabDidChange(base::BindRepeating(
          &BrowserInfoBarManager::OnActiveTabChanged, base::Unretained(this)));
  OnActiveTabChanged(browser);
}

void BrowserInfoBarManager::OnBrowserClosed(BrowserWindowInterface* browser) {
  active_tab_subscriptions_.erase(browser);
  last_active_managers_.erase(browser);
}

void BrowserInfoBarManager::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                             bool animate) {
  // Only tracked instances matter here. Everything in
  // `active_global_infobars_` was put there by ShowGlobally(), which only
  // accepts global specs, so membership is the scope check.
  const infobars::InfoBarDelegate::InfoBarIdentifier identifier =
      infobar->delegate()->GetIdentifier();
  auto it = active_global_infobars_.find(identifier);
  if (it == active_global_infobars_.end()) {
    return;
  }

  auto& instances = it->second->active_instances;
  const auto instance = std::ranges::find_if(
      instances,
      [infobar](const auto& entry) { return entry.second == infobar; });
  if (instance == instances.end()) {
    return;
  }
  instances.erase(instance);

  // Use the delegate, not the owner: when called from ~InfoBarManager(), the
  // owner's WebContentsObserver base is already destroyed.
  content::WebContents* web_contents =
      static_cast<RegistryInfoBarDelegate*>(infobar->delegate())
          ->web_contents();
  tabs::TabInterface* tab =
      web_contents ? tabs::TabInterface::MaybeGetFromContents(web_contents)
                   : nullptr;
  BrowserWindowInterface* browser =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  if (!browser) {
    return;
  }
  // An individual tab can go away while its window stays open, and during fast
  // shutdown its renderer may already be gone, so neither `closing_all()` nor
  // `IsDeleteScheduled()` is sufficient on its own to detect teardown.
  const bool is_tearing_down =
      browser->GetTabStripModel()->closing_all() ||
      browser->IsDeleteScheduled() || web_contents->IsBeingDestroyed() ||
      !web_contents->GetPrimaryMainFrame()->IsRenderFrameLive();
  if (!is_tearing_down) {
    scoped_refptr<InfoBarOutcomeTracker> outcome = it->second->outcome;
    base::WeakPtr<content::WebContents> weak_contents =
        web_contents->GetWeakPtr();
    HideGlobalInternal(identifier, /*suppress_outcome=*/false);
    if (outcome) {
      outcome->ReportPending(weak_contents.get());
    }
    return;
  }
  // The window or tab's renderer is closing or gone, not the infobar, which
  // stays up in the other browsers. Report the pending outcome once when the
  // last instance leaves; the resolved tracker stays attached for future
  // windows without retaining any callback state.
  if (instances.empty()) {
    scoped_refptr<InfoBarOutcomeTracker> outcome = it->second->outcome;
    if (outcome) {
      outcome->ReportPending(web_contents);
    }
  }
}

void BrowserInfoBarManager::OnManagerWillBeDestroyed(
    infobars::InfoBarManager* manager) {
  infobar_manager_observations_.RemoveObservation(manager);

  for (auto& [identifier, context] : active_global_infobars_) {
    context->active_instances.erase(manager);
  }

  for (auto it = last_active_managers_.begin();
       it != last_active_managers_.end();) {
    if (it->second == manager) {
      it = last_active_managers_.erase(it);
    } else {
      ++it;
    }
  }
}

void BrowserInfoBarManager::PopulateGlobalInfoBarsForActiveTab(
    BrowserWindowInterface* browser) {
  if (active_global_order_.empty()) {
    return;
  }

  tabs::TabInterface* active_tab = browser->GetActiveTabInterface();
  content::WebContents* active_contents =
      active_tab ? active_tab->GetContents() : nullptr;
  infobars::InfoBarManager* new_manager =
      active_contents ? ContentInfoBarManager::FromWebContents(active_contents)
                      : nullptr;
  if (!new_manager) {
    return;
  }

  // Snapshot order. Callbacks below may mutate `active_global_infobars_`.
  const auto order = active_global_order_;
  for (const auto identifier : order) {
    auto ctx_it = active_global_infobars_.find(identifier);
    if (ctx_it == active_global_infobars_.end()) {
      continue;
    }

    if (const auto browser_filter = ctx_it->second->spec->data.browser_filter();
        browser_filter && !browser_filter.Run(browser)) {
      ctx_it = active_global_infobars_.find(identifier);
      if (ctx_it != active_global_infobars_.end()) {
        auto inst_it = ctx_it->second->active_instances.find(new_manager);
        if (inst_it != ctx_it->second->active_instances.end()) {
          infobars::InfoBar* infobar = inst_it->second;
          ctx_it->second->active_instances.erase(inst_it);
          RemoveRegistryInfoBar(new_manager, infobar,
                                /*suppress_outcome=*/false);
        }
      }
      continue;
    }
    ctx_it = active_global_infobars_.find(identifier);
    if (ctx_it == active_global_infobars_.end()) {
      continue;
    }

    if (ctx_it->second->active_instances.contains(new_manager)) {
      continue;
    }

    // An interaction callback switched away and back; keep its instance.
    auto existing = std::ranges::find_if(
        new_manager->infobars(), [identifier](infobars::InfoBar* ib) {
          return ib->delegate()->GetIdentifier() == identifier &&
                 static_cast<RegistryInfoBarDelegate*>(ib->delegate())
                     ->in_interaction();
        });
    if (existing != new_manager->infobars().end()) {
      static_cast<RegistryInfoBarDelegate*>((*existing)->delegate())
          ->set_close_after_interaction(false);
      ctx_it->second->active_instances[new_manager] = *existing;
      continue;
    }

    auto infobar = CreateRegistryInfoBar(
        ConfirmInfoBarCreationAllowlist::GetPassKey(), ctx_it->second->spec,
        active_contents, ctx_it->second->params, ctx_it->second->outcome,
        ctx_it->second->on_global_close);
    if (!active_global_infobars_.contains(identifier)) {
      continue;
    }

    auto* added_infobar = new_manager->AddInfoBar(std::move(infobar));
    if (added_infobar &&
        std::ranges::contains(new_manager->infobars(), added_infobar)) {
      ctx_it = active_global_infobars_.find(identifier);
      if (ctx_it == active_global_infobars_.end()) {
        RemoveRegistryInfoBar(new_manager, added_infobar,
                              /*suppress_outcome=*/true);
        continue;
      }
      ctx_it->second->active_instances[new_manager] = added_infobar;
      if (!infobar_manager_observations_.IsObservingSource(new_manager)) {
        infobar_manager_observations_.AddObservation(new_manager);
      }
    }
  }
}

void BrowserInfoBarManager::OnActiveTabChanged(
    BrowserWindowInterface* browser) {
  tabs::TabInterface* active_tab = browser->GetActiveTabInterface();
  content::WebContents* active_contents =
      active_tab ? active_tab->GetContents() : nullptr;
  infobars::InfoBarManager* new_manager =
      active_contents ? ContentInfoBarManager::FromWebContents(active_contents)
                      : nullptr;

  infobars::InfoBarManager* old_manager = last_active_managers_[browser];

  if (old_manager == new_manager) {
    return;
  }

  if (old_manager) {
    for (auto& [identifier, context] : active_global_infobars_) {
      auto& manager_map = context->active_instances;
      auto it = manager_map.find(old_manager);
      if (it != manager_map.end()) {
        infobars::InfoBar* infobar = it->second;
        manager_map.erase(it);  // Erase first to signal programmatic removal.
        RemoveRegistryInfoBar(old_manager, infobar, /*suppress_outcome=*/false);
      }
    }
  }

  PopulateGlobalInfoBarsForActiveTab(browser);

  last_active_managers_[browser] = new_manager;
}

void BrowserInfoBarManager::StartObservingBrowsersIfNeeded() {
  if (browser_collection_observation_.IsObserving()) {
    return;
  }
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
  GlobalBrowserCollection::GetInstance()->ForEach(
      [this](BrowserWindowInterface* browser) {
        OnBrowserCreated(browser);
        return true;
      });
}

void BrowserInfoBarManager::StopObservingBrowsersIfIdle() {
  if (!active_global_infobars_.empty()) {
    return;
  }
  browser_collection_observation_.Reset();
  active_tab_subscriptions_.clear();
  last_active_managers_.clear();
}

bool BrowserInfoBarManager::IsBlockedByPolicy(const InfoBarSpec& spec) const {
  return spec.is_promotional() && local_state_ &&
         !local_state_->GetBoolean(prefs::kPromotionsEnabled);
}

}  // namespace infobars
