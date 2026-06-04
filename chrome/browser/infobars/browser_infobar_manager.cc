// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/browser_infobar_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
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

content::WebContents* GetActiveWebContents() {
  // TODO(crbug.com/512825363): Derivation of browser will be changed to accommodate profile.
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

  bool Accept() override {
    if (spec_.ok_button_callback()) {
      spec_.ok_button_callback().Run(GetActiveWebContents());
    }
    return true;
  }

  bool Cancel() override {
    if (spec_.cancel_button_callback()) {
      spec_.cancel_button_callback().Run(GetActiveWebContents());
    }
    return true;
  }

  void InfoBarDismissed() override {
    if (spec_.dismiss_callback()) {
      spec_.dismiss_callback().Run(GetActiveWebContents());
    }
  }

  bool ShouldExpire(const NavigationDetails& details) const override {
    return spec_.expire_on_navigation() &&
           ConfirmInfoBarDelegate::ShouldExpire(details);
  }

 private:
  InfoBarSpec spec_;
};

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
                                *this) {}

BrowserInfoBarManager::~BrowserInfoBarManager() = default;

// static
BrowserInfoBarManager* BrowserInfoBarManager::From(
    BrowserProcess* browser_process) {
  return Get(browser_process->GetUnownedUserDataHost());
}

void BrowserInfoBarManager::Register(InfoBarSpec spec) {
  registered_specs_[spec.identifier()] = std::move(spec);
}

void BrowserInfoBarManager::Show(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return;
  }

  const InfoBarSpec& spec = it->second;

  // For now, only handle current tab scope.
  if (spec.scope() == InfoBarScope::kCurrentTab) {
    auto* manager = GetActiveTabInfoBarManager();
    if (!manager) {
      return;
    }

    manager->AddInfoBar(
        CreateConfirmInfoBar(std::make_unique<RegistryInfoBarDelegate>(spec)));
  }
}

void BrowserInfoBarManager::Hide(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  if (it == registered_specs_.end()) {
    return;
  }

  const InfoBarSpec& spec = it->second;

  if (spec.scope() == InfoBarScope::kCurrentTab) {
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
  }
}

InfoBarPriority BrowserInfoBarManager::GetApprovedPriority(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  auto it = registered_specs_.find(identifier);
  if (it != registered_specs_.end()) {
    return it->second.priority();
  }
  return InfoBarPriority::kDefault;
}

}  // namespace infobars
