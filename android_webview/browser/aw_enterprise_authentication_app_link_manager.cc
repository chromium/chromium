// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_enterprise_authentication_app_link_manager.h"

#include "components/url_matcher/url_util.h"

namespace android_webview {

namespace prefs {
extern const char kEnterpriseAuthAppLinkPolicy[];
}

EnterpriseAuthenticationAppLinkManager::EnterpriseAuthenticationAppLinkManager(
    PrefService* pref_service) {
  pref_service_ = pref_service;
  pref_observer_.Init(pref_service);
  pref_observer_.Add(
      prefs::kEnterpriseAuthAppLinkPolicy,
      base::BindRepeating(
          &EnterpriseAuthenticationAppLinkManager::OnPolicyUpdated,
          base::Unretained(this)));

  // Call once to initialize the watcher with the current pref's values.
  OnPolicyUpdated();
}

EnterpriseAuthenticationAppLinkManager::
    ~EnterpriseAuthenticationAppLinkManager() = default;

void EnterpriseAuthenticationAppLinkManager::OnPolicyUpdated() {
  const base::Value::List& authentication_urls_policy =
      pref_service_->GetList(prefs::kEnterpriseAuthAppLinkPolicy);

  url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::util::AddAllowFilters(url_matcher_.get(),
                                     authentication_urls_policy);
}

bool EnterpriseAuthenticationAppLinkManager::IsEnterpriseAuthenticationUrl(
    const GURL& url) {
  return url_matcher_ && !(url_matcher_->MatchURL(url).empty());
}
}  // namespace android_webview
