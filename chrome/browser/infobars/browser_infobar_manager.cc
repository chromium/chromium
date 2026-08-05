// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/browser_infobar_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace infobars {

namespace {

// RegistryInfoBarDelegate acts as the universal adapter between the modern
// InfoBarSpec and the legacy ConfirmInfoBarDelegate.
class RegistryInfoBarDelegate final : public ConfirmInfoBarDelegate {
 public:
  explicit RegistryInfoBarDelegate(InfoBarSpec spec) : spec_(std::move(spec)) {}

  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override {
    return spec_.identifier();
  }

  std::u16string GetMessageText() const override {
    return spec_.message_text();
  }

  std::u16string GetLinkText() const override { return spec_.link_text(); }

  GURL GetLinkURL() const override { return spec_.link_navigation_url(); }

  int GetIconId() const override { return spec_.icon_id(); }

  const gfx::VectorIcon& GetVectorIcon() const override {
    return spec_.icon() ? *spec_.icon()
                        : ConfirmInfoBarDelegate::GetVectorIcon();
  }

  int GetButtons() const override {
    int buttons = BUTTON_NONE;
    if (!spec_.ok_button_label().empty() || spec_.ok_button_callback()) {
      buttons |= BUTTON_OK;
    }
    if (!spec_.cancel_button_label().empty() ||
        spec_.cancel_button_callback()) {
      buttons |= BUTTON_CANCEL;
    }
    return buttons;
  }

  std::u16string GetButtonLabel(InfoBarButton button) const override {
    if (button == BUTTON_OK && !spec_.ok_button_label().empty()) {
      return spec_.ok_button_label();
    }
    if (button == BUTTON_CANCEL && !spec_.cancel_button_label().empty()) {
      return spec_.cancel_button_label();
    }
    return ConfirmInfoBarDelegate::GetButtonLabel(button);
  }

  bool Accept() override {
    base::UmaHistogramSparse("InfoBar.Centralized.Accept", GetIdentifier());
    if (spec_.ok_button_callback()) {
      spec_.ok_button_callback().Run(GetWebContents());
    }
    return true;
  }

  bool Cancel() override {
    base::UmaHistogramSparse("InfoBar.Centralized.Cancel", GetIdentifier());
    if (spec_.cancel_button_callback()) {
      spec_.cancel_button_callback().Run(GetWebContents());
    }
    return true;
  }

  void InfoBarDismissed() override {
    base::UmaHistogramSparse("InfoBar.Centralized.Dismiss", GetIdentifier());
    if (spec_.dismiss_callback()) {
      spec_.dismiss_callback().Run(GetWebContents());
    }
  }

  bool LinkClicked(WindowOpenDisposition disposition) override {
    base::UmaHistogramSparse("InfoBar.Centralized.LinkClicked",
                             GetIdentifier());
    return ConfirmInfoBarDelegate::LinkClicked(disposition);
  }

  bool ShouldExpire(const NavigationDetails& details) const override {
    return spec_.expire_on_navigation() &&
           ConfirmInfoBarDelegate::ShouldExpire(details);
  }

  bool ShouldHideInFullscreen() const override {
    return spec_.should_hide_in_fullscreen();
  }

  bool ShouldAnimate() const override { return spec_.should_animate(); }

  bool IsCloseable() const override { return spec_.is_closeable(); }

  InfoBarDelegate::InfobarPriority GetPriority() const override {
    return spec_.priority();
  }

 private:
  content::WebContents* GetWebContents() {
    if (!infobar()) {
      return nullptr;
    }
    return ContentInfoBarManager::WebContentsFromInfoBar(infobar());
  }

  InfoBarSpec spec_;
};

content::WebContents* GetActiveWebContents() {
  // TODO(crbug.com/512825363): Derivation of browser will be changed to
  // accommodate profile.
  auto* browser = GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (!browser) {
    return nullptr;
  }

  auto* tab = browser->GetActiveTabInterface();
  if (!tab) {
    return nullptr;
  }

  return tab->GetContents();
}

ContentInfoBarManager* GetActiveTabInfoBarManager() {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents) {
    return nullptr;
  }

  return ContentInfoBarManager::FromWebContents(web_contents);
}

}  // namespace

DEFINE_USER_DATA(BrowserInfoBarManager);

BrowserInfoBarManager::BrowserInfoBarManager(BrowserProcess* browser_process)
    : scoped_unowned_user_data_(browser_process->GetUnownedUserDataHost(),
                                *this) {
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
  GlobalBrowserCollection::GetInstance()->ForEach(
      [this](BrowserWindowInterface* browser) {
        OnBrowserCreated(browser);
        return true;
      });
}

BrowserInfoBarManager::~BrowserInfoBarManager() = default;

// static
BrowserInfoBarManager* BrowserInfoBarManager::From(
    BrowserProcess* browser_process) {
  return Get(browser_process->GetUnownedUserDataHost());
}

void BrowserInfoBarManager::Register(InfoBarSpec spec) {
  CHECK(!registered_specs_.contains(spec.identifier()));
  registered_specs_[spec.identifier()] = std::move(spec);
}

void BrowserInfoBarManager::Show(
    content::WebContents* contents,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return;
  }
  CHECK(contents);
  CHECK(it->second.scope() == InfoBarScope::kTab);

  auto* manager = ContentInfoBarManager::FromWebContents(contents);
  if (!manager) {
    return;
  }
  if (manager->AddInfoBar(CreateConfirmInfoBar(
          std::make_unique<RegistryInfoBarDelegate>(it->second)))) {
    base::UmaHistogramSparse("InfoBar.Centralized.Show", identifier);
  }
}

void BrowserInfoBarManager::ShowGlobally(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return;
  }
  CHECK(it->second.scope() == InfoBarScope::kGlobal);

  const InfoBarSpec& spec = it->second;

  if (active_global_infobars_.contains(identifier)) {
    return;
  }
  active_global_infobars_[identifier] = GlobalInfoBarContext{.spec = spec};

  bool added_any_infobars = false;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [this, &spec, identifier,
       &added_any_infobars](BrowserWindowInterface* browser) {
        tabs::TabInterface* active_tab = browser->GetActiveTabInterface();
        content::WebContents* active_contents =
            active_tab ? active_tab->GetContents() : nullptr;
        if (active_contents) {
          auto* manager =
              ContentInfoBarManager::FromWebContents(active_contents);
          if (manager) {
            auto infobar = CreateConfirmInfoBar(
                std::make_unique<RegistryInfoBarDelegate>(spec));
            auto* added_infobar = manager->AddInfoBar(std::move(infobar));
            if (added_infobar) {
              active_global_infobars_[identifier].active_instances[manager] =
                  added_infobar;
              added_any_infobars = true;
              if (!infobar_manager_observations_.IsObservingSource(manager)) {
                infobar_manager_observations_.AddObservation(manager);
              }
            }
          }
        }
        return true;
      });

  if (added_any_infobars) {
    base::UmaHistogramSparse("InfoBar.Centralized.Show", identifier);
  }
}

void BrowserInfoBarManager::Hide(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return;
  }

  const InfoBarSpec& spec = it->second;

  if (spec.scope() == InfoBarScope::kTab) {
    auto* manager = GetActiveTabInfoBarManager();
    if (!manager) {
      return;
    }

    for (infobars::InfoBar* infobar : manager->infobars()) {
      if (infobar->delegate()->GetIdentifier() == identifier) {
        manager->RemoveInfoBar(infobar);
        break;
      }
    }
  } else if (spec.scope() == InfoBarScope::kGlobal) {
    auto active_it = active_global_infobars_.find(identifier);
    if (active_it != active_global_infobars_.end()) {
      auto& manager_map = active_it->second.active_instances;
      while (!manager_map.empty()) {
        auto map_it = manager_map.begin();
        infobars::InfoBarManager* manager = map_it->first;
        infobars::InfoBar* infobar = map_it->second;

        manager_map.erase(
            map_it);  // Erase first to signal programmatic removal.
        manager->RemoveInfoBar(infobar);
      }
      active_global_infobars_.erase(active_it);
    }
  }
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
  infobars::InfoBarDelegate::InfoBarIdentifier identifier =
      infobar->delegate()->GetIdentifier();

  infobars::InfoBarManager* found_manager = nullptr;
  auto it = active_global_infobars_.find(identifier);
  if (it != active_global_infobars_.end()) {
    auto& manager_map = it->second.active_instances;
    for (auto& [manager, ib] : manager_map) {
      if (ib == infobar) {
        found_manager = manager;
        break;
      }
    }
    if (found_manager) {
      manager_map.erase(found_manager);
    }
  }

  if (found_manager) {
    if (IsGlobal(identifier)) {
      Hide(identifier);
    }
  }
}

void BrowserInfoBarManager::OnManagerWillBeDestroyed(
    infobars::InfoBarManager* manager) {
  infobar_manager_observations_.RemoveObservation(manager);

  for (auto& [identifier, context] : active_global_infobars_) {
    context.active_instances.erase(manager);
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

InfoBarDelegate::InfobarPriority BrowserInfoBarManager::GetApprovedPriority(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  if (it != registered_specs_.end()) {
    return it->second.priority();
  }
  return InfoBarDelegate::InfobarPriority::kDefault;
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
      auto& manager_map = context.active_instances;
      auto it = manager_map.find(old_manager);
      if (it != manager_map.end()) {
        infobars::InfoBar* infobar = it->second;
        manager_map.erase(it);  // Erase first to signal programmatic removal.
        old_manager->RemoveInfoBar(infobar);
      }
    }
  }

  if (new_manager) {
    for (auto& [identifier, context] : active_global_infobars_) {
      auto infobar = CreateConfirmInfoBar(
          std::make_unique<RegistryInfoBarDelegate>(context.spec));
      auto* added_infobar = new_manager->AddInfoBar(std::move(infobar));
      if (added_infobar) {
        context.active_instances[new_manager] = added_infobar;
        if (!infobar_manager_observations_.IsObservingSource(new_manager)) {
          infobar_manager_observations_.AddObservation(new_manager);
        }
      }
    }
  }

  last_active_managers_[browser] = new_manager;
}

bool BrowserInfoBarManager::IsGlobal(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  return it != registered_specs_.end() &&
         it->second.scope() == InfoBarScope::kGlobal;
}

}  // namespace infobars
