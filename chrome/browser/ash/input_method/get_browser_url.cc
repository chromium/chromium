// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements the input method candidate window used on Chrome OS.

#include "chrome/browser/ash/input_method/get_browser_url.h"

#include "base/callback.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {
namespace input_method {
namespace {

absl::optional<GURL> GetAshChromeUrl() {
  Browser* browser = chrome::FindLastActive();
  // Ash chrome will return true for browser->window()->IsActive() if the
  // user is currently typing in an ash browser tab. IsActive() will return
  // false if the user is currently typing a lacros browser tab.
  if (browser && browser->window() && browser->window()->IsActive() &&
      browser->tab_strip_model() &&
      browser->tab_strip_model()->GetActiveWebContents()) {
    return browser->tab_strip_model()
        ->GetActiveWebContents()
        ->GetLastCommittedURL();
  }

  return absl::nullopt;
}

void GetLacrosChromeUrl(GetFocusedTabUrlCallback callback) {
  crosapi::BrowserManager* browser_manager = crosapi::BrowserManager::Get();
  // browser_manager will exist whenever there is a lacros browser running.
  // GetActiveTabUrlSupported() will only return true if the current lacros
  // browser is being used by the user.
  if (browser_manager && browser_manager->IsRunning() &&
      browser_manager->GetActiveTabUrlSupported()) {
    browser_manager->GetActiveTabUrl(std::move(callback));
    return;
  }

  std::move(callback).Run(absl::nullopt);
}

}  // namespace

void GetFocusedTabUrl(GetFocusedTabUrlCallback callback) {
  absl::optional<GURL> ash_url = GetAshChromeUrl();
  if (ash_url.has_value()) {
    std::move(callback).Run(ash_url);
    return;
  }

  GetLacrosChromeUrl(std::move(callback));
}

}  // namespace input_method
}  // namespace ash
