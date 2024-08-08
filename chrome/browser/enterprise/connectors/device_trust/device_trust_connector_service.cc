// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"

#include "base/check.h"
#include "components/enterprise/device_trust/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

namespace enterprise_connectors {

DeviceTrustConnectorService::DeviceTrustConnectorService(
    PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  CHECK(profile_prefs_);

  pref_observer_.Init(profile_prefs_);
  policy_details_map_.emplace(
      DTCPolicyLevel::kUser,
      DTCPolicyDetails(kUserContextAwareAccessSignalsAllowlistPref));
  policy_details_map_.emplace(
      DTCPolicyLevel::kBrowser,
      DTCPolicyDetails(kBrowserContextAwareAccessSignalsAllowlistPref));

  for (auto const& policy_details : policy_details_map_) {
    pref_observer_.Add(
        policy_details.second.pref,
        base::BindRepeating(&DeviceTrustConnectorService::OnPolicyUpdated,
                            weak_factory_.GetWeakPtr(),
                            /*level=*/policy_details.first,
                            /*pref=*/policy_details.second.pref));

    // Call once to initialize the watcher with the current pref's values.
    OnPolicyUpdated(/*level=*/policy_details.first,
                    /*pref=*/policy_details.second.pref);
  }
}
DeviceTrustConnectorService::~DeviceTrustConnectorService() = default;

bool DeviceTrustConnectorService::IsConnectorEnabled() const {
  return !GetEnabledInlinePolicyLevels().empty();
}

const std::set<DTCPolicyLevel> DeviceTrustConnectorService::Watches(
    const GURL& url) const {
  std::set<DTCPolicyLevel> levels;
  for (auto const& policy_details : policy_details_map_) {
    if (policy_details.second.matcher &&
        !policy_details.second.matcher->MatchURL(url).empty()) {
      levels.insert(policy_details.first);
    }
  }

  return levels;
}

void DeviceTrustConnectorService::AddObserver(
    std::unique_ptr<PolicyObserver> observer) {
  observers_.push_back(std::move(observer));
  for (auto const& policy_details : policy_details_map_) {
    if (policy_details.second.enabled) {
      OnInlinePolicyEnabled(policy_details.first);
    } else {
      OnInlinePolicyDisabled(policy_details.first);
    }
  }
}

const std::set<DTCPolicyLevel>
DeviceTrustConnectorService::GetEnabledInlinePolicyLevels() const {
  std::set<DTCPolicyLevel> levels;
  for (auto const& policy_details : policy_details_map_) {
    if (policy_details.second.enabled) {
      levels.insert(policy_details.first);
    }
  }

  return levels;
}

DeviceTrustConnectorService::DTCPolicyDetails::DTCPolicyDetails(
    const std::string& pref)
    : enabled(false),
      pref(pref),
      matcher(std::make_unique<url_matcher::URLMatcher>()) {}

DeviceTrustConnectorService::DTCPolicyDetails::DTCPolicyDetails(
    DTCPolicyDetails&& other) = default;

DeviceTrustConnectorService::DTCPolicyDetails&

DeviceTrustConnectorService::DTCPolicyDetails::operator=(
    DeviceTrustConnectorService::DTCPolicyDetails&& other) = default;

DeviceTrustConnectorService::DTCPolicyDetails::~DTCPolicyDetails() = default;

void DeviceTrustConnectorService::OnPolicyUpdated(const DTCPolicyLevel& level,
                                                  const std::string& pref) {
  const base::Value::List* url_patterns = GetPolicyUrlPatterns(pref);
  auto& policy_details = policy_details_map_.at(level);
  // Reset the matcher and update the policy details.
  policy_details.matcher = std::make_unique<url_matcher::URLMatcher>();
  policy_details.enabled = url_patterns && !url_patterns->empty();

  if (policy_details.enabled) {
    // Add the new endpoints to the conditions.
    url_matcher::util::AddAllowFilters(policy_details.matcher.get(),
                                       *url_patterns);
    OnInlinePolicyEnabled(level);
  } else {
    OnInlinePolicyDisabled(level);
  }
}

void DeviceTrustConnectorService::OnInlinePolicyEnabled(DTCPolicyLevel level) {
  for (const auto& observer : observers_) {
    observer->OnInlinePolicyEnabled(level);
  }
}

void DeviceTrustConnectorService::OnInlinePolicyDisabled(DTCPolicyLevel level) {
  for (const auto& observer : observers_) {
    observer->OnInlinePolicyDisabled(level);
  }
}

const base::Value::List* DeviceTrustConnectorService::GetPolicyUrlPatterns(
    const std::string& pref) const {
  if (!profile_prefs_->IsManagedPreference(pref)) {
    return nullptr;
  }
  return &profile_prefs_->GetList(pref);
}

}  // namespace enterprise_connectors
