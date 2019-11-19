// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"

#include <memory>

#include "base/feature_list.h"
#include "base/stl_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"

namespace {

// URL schemes not on this list:  (e.g., file:// and chrome://,
// chrome-extension:// ...) will always be allowed.
const char* const kFilteredSchemes[] = {"http", "https", "ftp", "ws", "wss"};

bool IsSchemeFiltered(const GURL& url) {
  for (const auto* i : kFilteredSchemes) {
    if (i == url.scheme())
      return true;
  }
  return false;
}

}  // namespace

namespace chromeos {

// static
bool WebTimeLimitEnforcer::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kWebTimeLimits);
}

WebTimeLimitEnforcer::WebTimeLimitEnforcer() = default;
WebTimeLimitEnforcer::~WebTimeLimitEnforcer() = default;

void WebTimeLimitEnforcer::OnWebTimeLimitReached() {
  if (chrome_blocked_)
    return;

  chrome_blocked_ = true;
  ReloadAllWebContents();
}

void WebTimeLimitEnforcer::OnWebTimeLimitEnded() {
  if (!chrome_blocked_)
    return;

  chrome_blocked_ = false;
  ReloadAllWebContents();
}

void WebTimeLimitEnforcer::OnWhitelistAdded(const GURL& url) {
  auto result = whitelisted_urls_.insert(url);
  bool inserted = result.second;
  if (inserted && blocked())
    ReloadAllWebContents();
}

void WebTimeLimitEnforcer::OnWhitelistRemoved(const GURL& url) {
  if (!base::Contains(whitelisted_urls_, url))
    return;

  whitelisted_urls_.erase(url);
  if (blocked())
    ReloadAllWebContents();
}

bool WebTimeLimitEnforcer::IsURLWhitelisted(const GURL& url) const {
  if (!IsSchemeFiltered(url))
    return true;

  return base::Contains(whitelisted_urls_, url);
}

void WebTimeLimitEnforcer::ReloadAllWebContents() {
  auto* browser_list = BrowserList::GetInstance();
  for (auto* browser : *browser_list) {
    auto* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); i++) {
      auto* web_content = tab_strip_model->GetWebContentsAt(i);
      web_content->GetController().Reload(content::ReloadType::NORMAL,
                                          /* check_for_repost */ false);
    }
  }
}

}  // namespace chromeos
