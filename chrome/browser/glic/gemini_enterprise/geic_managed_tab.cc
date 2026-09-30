// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_managed_tab.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#include "chrome/browser/glic/gemini_enterprise/gemini_enterprise.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {

namespace {

// Origin of the default GE OAuth redirector page (`DEFAULT_REDIRECT_ORIGIN` in
// the GE web client's auth service, `auth_service.ts`).
// Cross-repo contract: All standard 3P connectors trampoline through this
// origin. Origin-specific connectors use the guest origin, which is passed in
// separately. If GE introduces a new redirector origin or path, Chrome must
// be updated first. See README.md.
constexpr std::string_view kDefaultConnectorOAuthRedirectorOrigin =
    "https://vertexaisearch.cloud.google.com";

}  // namespace

GeicManagedTab::GeicManagedTab(Profile* profile,
                               mojom::AuthTabPurpose purpose,
                               url::Origin gaia_origin,
                               url::Origin guest_origin)
    : profile_(profile),
      purpose_(purpose),
      gaia_origin_(std::move(gaia_origin)),
      guest_origin_(std::move(guest_origin)) {}

GeicManagedTab::~GeicManagedTab() = default;

bool GeicManagedTab::IsExpectedOrigin(const GURL& url) const {
  if (purpose_ == mojom::AuthTabPurpose::kUnknown) {
    return false;
  }
  const url::Origin origin = url::Origin::Create(url);
  if (origin == guest_origin_) {
    return true;
  }
  switch (purpose_) {
    case mojom::AuthTabPurpose::kSignIn:
      return origin == gaia_origin_;
    case mojom::AuthTabPurpose::kConnectorOauth:
      return origin.Serialize() == kDefaultConnectorOAuthRedirectorOrigin;
    case mojom::AuthTabPurpose::kUnknown:
      NOTREACHED();
  }
  NOTREACHED();
}

bool GeicManagedTab::IsStillOwned(tabs::TabInterface* tab) const {
  content::NavigationEntry* entry =
      tab->GetContents()->GetController().GetLastCommittedEntry();
  // The navigation Chrome started has not committed yet, so the user cannot
  // have navigated away.
  if (!entry || entry->IsInitialEntry()) {
    return true;
  }
  return IsExpectedOrigin(entry->GetURL());
}

BrowserWindowInterface* GeicManagedTab::GetLastActiveBrowserWindowForProfile()
    const {
  if (!profile_) {
    return nullptr;
  }
  BrowserWindowInterface* active_browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL &&
            browser->GetProfile() == profile_) {
          active_browser = browser;
          return false;
        }
        return true;
      });
  return active_browser;
}

bool GeicManagedTab::Open(const GURL& url, ReuseMode reuse_mode) {
  if (!profile_) {
    return false;
  }

  // If the managed tab is already open and still belongs to the flow, reuse it
  // instead of opening a duplicate. Otherwise (the user navigated it elsewhere,
  // or it is mid tab-drag and not attached to a window), fall through and open
  // a new tab; the old tab is left alone and no longer tracked. On reuse,
  // `tab_active_before_open_` is intentionally left as captured by the first
  // open, so `Close()` returns focus to the tab the user was on before the flow
  // started.
  if (tabs::TabInterface* existing_tab = tab_.Get();
      existing_tab && IsStillOwned(existing_tab)) {
    if (BrowserWindowInterface* browser =
            existing_tab->GetBrowserWindowInterface()) {
      if (auto* tab_list = TabListInterface::From(browser)) {
        if (reuse_mode == ReuseMode::kNavigateAndActivate) {
          content::NavigationController::LoadURLParams load_params(url);
          load_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
          existing_tab->GetContents()->GetController().LoadURLWithParams(
              load_params);
        }
        tab_list->ActivateTab(tab_);
        return true;
      }
    }
  }

  BrowserWindowInterface* browser_window =
      GetLastActiveBrowserWindowForProfile();
  if (!browser_window) {
    return false;
  }

  if (auto* tab_list = TabListInterface::From(browser_window)) {
    if (auto* active_tab = tab_list->GetActiveTab()) {
      tab_active_before_open_ = active_tab->GetHandle();
    }
  }

  auto params = std::make_unique<NavigateParams>(
      profile_, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params->browser = browser_window;
  params->disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params->user_gesture = false;

  base::WeakPtr<content::NavigationHandle> handle =
      glic::Navigate(std::move(params));
  if (handle && handle->GetWebContents()) {
    if (tabs::TabInterface* tab = tabs::TabInterface::MaybeGetFromContents(
            handle->GetWebContents())) {
      tab_ = tab->GetHandle();
      has_opened_tab_ = true;
      return true;
    }
  } else if (auto* tab_list = TabListInterface::From(browser_window)) {
    if (auto* active_tab = tab_list->GetActiveTab()) {
      // Only treat the active tab as the managed tab if a new tab was actually
      // opened (distinct from the tab that was active before opening).
      if (active_tab->GetHandle() != tab_active_before_open_) {
        tab_ = active_tab->GetHandle();
        has_opened_tab_ = true;
        return true;
      }
    }
  }
  return false;
}

GeicManagedTab::CloseOutcome GeicManagedTab::Close() {
  if (!has_opened_tab_) {
    return CloseOutcome::kNotOpened;
  }
  has_opened_tab_ = false;

  tabs::TabInterface* tab = tab_.Get();
  tabs::TabHandle restore_to = tab_active_before_open_;
  tab_ = tabs::TabHandle();
  tab_active_before_open_ = tabs::TabHandle();

  if (!tab) {
    return CloseOutcome::kAlreadyClosed;
  }
  if (!IsStillOwned(tab)) {
    // The user has taken the tab over; leave it (and focus) alone.
    return CloseOutcome::kNavigatedAway;
  }

  // Only move focus back if the user is still looking at the managed tab;
  // otherwise closing it should not pull them away from what they are doing.
  const bool was_active = tab->IsActivated();
  tab->Close();

  if (!was_active) {
    return CloseOutcome::kClosedInactive;
  }
  if (tabs::TabInterface* original = restore_to.Get()) {
    if (BrowserWindowInterface* browser =
            original->GetBrowserWindowInterface()) {
      if (auto* tab_list = TabListInterface::From(browser)) {
        tab_list->ActivateTab(restore_to);
      }
    }
  }
  return CloseOutcome::kClosedActive;
}

}  // namespace glic
