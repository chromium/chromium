// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/web_time_limit_enforcer.h"

#include <memory>

#include "base/feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limits_allowlist_policy_wrapper.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"

namespace ash {
namespace app_time {

// static
bool WebTimeLimitEnforcer::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kWebTimeLimits);
}

WebTimeLimitEnforcer::WebTimeLimitEnforcer(
    AppTimeController* app_time_controller)
    : app_time_controller_(app_time_controller) {}

WebTimeLimitEnforcer::~WebTimeLimitEnforcer() = default;

void WebTimeLimitEnforcer::OnWebTimeLimitReached(base::TimeDelta time_limit) {
  if (chrome_blocked_)
    return;
  time_limit_ = time_limit;

  chrome_blocked_ = true;
  ReloadAllWebContents();
}

void WebTimeLimitEnforcer::OnWebTimeLimitEnded() {
  if (!chrome_blocked_)
    return;

  time_limit_ = base::TimeDelta();
  chrome_blocked_ = false;
  ReloadAllWebContents();
}

void WebTimeLimitEnforcer::OnTimeLimitAllowlistChanged(
    const AppTimeLimitsAllowlistPolicyWrapper& wrapper) {
  std::vector<std::string> allowlisted_urls = wrapper.GetAllowlistURLList();

  // clean up |url_matcher_|;
  url_matcher_ = std::make_unique<url_matcher::URLMatcher>();

  url_matcher::URLMatcherConditionSet::Vector condition_set_vector;
  auto* condition_factory = url_matcher_->condition_factory();
  int id = 0;
  for (const auto& url : allowlisted_urls) {
    url_matcher::URLMatcherCondition condition =
        condition_factory->CreateURLMatchesCondition(url);

    url_matcher::URLMatcherConditionSet::Conditions conditions;
    conditions.insert(condition);
    condition_set_vector.push_back(
        base::MakeRefCounted<url_matcher::URLMatcherConditionSet>(id++,
                                                                  conditions));
  }

  url_matcher_->AddConditionSets(condition_set_vector);

  // Filters have been updated. Now reload all WebContents.
  ReloadAllWebContents();
}

bool WebTimeLimitEnforcer::IsURLAllowlisted(const GURL& url) const {
  // Block everything if |scheme_filter_| and |domain_matcher_| are not
  // instantiated yet.
  if (!url_matcher_)
    return false;

  GURL effective_url = url_matcher::util::Normalize(url);
  if (!effective_url.is_valid())
    effective_url = url;

  if (IsValidExtensionUrl(effective_url))
    return app_time_controller_->IsExtensionAllowlisted(effective_url.host());

  auto matching_set_size = url_matcher_->MatchURL(effective_url).size();
  return matching_set_size > 0;
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

}  // namespace app_time
}  // namespace ash
