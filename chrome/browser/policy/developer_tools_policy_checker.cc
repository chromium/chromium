// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/developer_tools_policy_checker.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace policy {

DeveloperToolsPolicyChecker::DeveloperToolsPolicyChecker(
    PrefService* pref_service)
    : pref_service_(pref_service),
      url_blocklist_manager_(pref_service,
                             prefs::kDeveloperToolsAvailabilityBlocklist,
                             prefs::kDeveloperToolsAvailabilityAllowlist) {}
DeveloperToolsPolicyChecker::~DeveloperToolsPolicyChecker() = default;
bool DeveloperToolsPolicyChecker::IsUrlAllowedByPolicy(const GURL& url) const {
  return url_blocklist_manager_.GetURLBlocklistState(url) ==
         URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST;
}
bool DeveloperToolsPolicyChecker::IsUrlBlockedByPolicy(const GURL& url) const {
  return url_blocklist_manager_.GetURLBlocklistState(url) ==
         URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

base::CallbackListSubscription DeveloperToolsPolicyChecker::AddObserver(
    base::RepeatingClosure callback) {
  return url_blocklist_manager_.AddObserver(std::move(callback));
}

DeveloperToolsPolicyChecker::DevToolsAvailability
DeveloperToolsPolicyChecker::GetDevToolsAvailabilityForUrl(
    const GURL& url) const {
  URLBlocklist::URLBlocklistState url_state =
      url_blocklist_manager_.GetURLBlocklistState(url);
  if (url_state == URLBlocklist::URLBlocklistState::URL_IN_ALLOWLIST) {
    return DevToolsAvailability::kAllowed;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (!pref_service_->GetList(prefs::kDeveloperToolsAvailabilityAllowlist)
           .empty() &&
      pref_service_->GetList(prefs::kDeveloperToolsAvailabilityBlocklist)
          .empty()) {
    return DevToolsAvailability::kDisallowed;
  }
#endif

  if (url_state == URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST) {
    return DevToolsAvailability::kDisallowed;
  }
  return DevToolsAvailability::kNotSet;
}

}  // namespace policy
