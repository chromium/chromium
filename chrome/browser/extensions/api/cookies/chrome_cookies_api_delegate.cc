// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cookies/chrome_cookies_api_delegate.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {

ChromeCookiesApiDelegate::ChromeCookiesApiDelegate() = default;

ChromeCookiesApiDelegate::~ChromeCookiesApiDelegate() = default;

std::vector<CookiesApiDelegate::CookieStoreContext>
ChromeCookiesApiDelegate::GetCookieStoreContexts(
    content::BrowserContext& context,
    bool include_incognito) {
  Profile* original_profile =
      Profile::FromBrowserContext(&context)->GetOriginalProfile();
  DCHECK(original_profile);
  Profile* incognito_profile = nullptr;
  if (include_incognito && original_profile->HasPrimaryOTRProfile()) {
    incognito_profile =
        original_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }
  DCHECK(original_profile != incognito_profile);

  // Iterate through all browser instances, and for each browser, add its tab
  // IDs to either the regular or incognito tab ID list depending whether the
  // browser is regular or incognito.
  std::vector<int> original_tab_ids;
  std::vector<int> incognito_tab_ids;
  for (WindowController* window : *WindowControllerList::GetInstance()) {
    std::vector<int>* tab_ids = nullptr;
    if (window->profile() == original_profile) {
      tab_ids = &original_tab_ids;
    } else if (window->profile() == incognito_profile) {
      tab_ids = &incognito_tab_ids;
    } else {
      continue;
    }
    for (int i = 0; i < window->GetTabCount(); ++i) {
      tab_ids->push_back(
          ExtensionTabUtil::GetTabId(window->GetWebContentsAt(i)));
    }
  }

  // Return a cookie store for each profile with at least one open tab.
  std::vector<CookieStoreContext> cookie_stores;
  if (!original_tab_ids.empty()) {
    cookie_stores.push_back(
        {raw_ref<content::BrowserContext>(*original_profile),
         std::move(original_tab_ids)});
  }
  if (incognito_profile && !incognito_tab_ids.empty()) {
    cookie_stores.push_back(
        {raw_ref<content::BrowserContext>(*incognito_profile),
         std::move(incognito_tab_ids)});
  }
  return cookie_stores;
}

}  // namespace extensions
