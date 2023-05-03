// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/prefs.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_util.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

const base::Value::List* GetPolicyUrlPatterns(PrefService* prefs) {
  if (!prefs->IsManagedPreference(kContextAwareAccessSignalsAllowlistPref))
    return nullptr;
  return &prefs->GetList(kContextAwareAccessSignalsAllowlistPref);
}

bool ConnectorPolicyHasValues(PrefService* profile_prefs) {
  const auto* list = GetPolicyUrlPatterns(profile_prefs);
  return list && !list->empty();
}

}  // namespace

DeviceTrustConnectorService::DeviceTrustConnectorService(
    PrefService* profile_prefs)
    : profile_prefs_(profile_prefs) {
  CHECK(profile_prefs_);

  if (!IsDeviceTrustConnectorFeatureEnabled()) {
    return;
  }

  if (!pref_observer_.IsEmpty()) {
    return;
  }

  pref_observer_.Init(profile_prefs_);
  pref_observer_.Add(
      kContextAwareAccessSignalsAllowlistPref,
      base::BindRepeating(&DeviceTrustConnectorService::OnPolicyUpdated,
                          weak_factory_.GetWeakPtr()));

  // Call once to initialize the watcher with the current pref's values.
  OnPolicyUpdated();
}

DeviceTrustConnectorService::~DeviceTrustConnectorService() = default;

bool DeviceTrustConnectorService::IsConnectorEnabled() const {
  if (!IsDeviceTrustConnectorFeatureEnabled() || !profile_prefs_) {
    return false;
  }
  return ConnectorPolicyHasValues(profile_prefs_);
}

const std::set<DTCPolicyLevel> DeviceTrustConnectorService::Watches(
    const GURL& url) const {
  std::set<DTCPolicyLevel> levels;

  if (matcher_ && !matcher_->MatchURL(url).empty()) {
    // TODO(b/279063343): This is temporary, later this service should insert
    // the correct policy levels based on the scope of the policy matchers(i.e
    // browser, user).
    levels.insert(DTCPolicyLevel::kBrowser);
    levels.insert(DTCPolicyLevel::kUser);
  }

  return levels;
}

void DeviceTrustConnectorService::AddObserver(
    std::unique_ptr<PolicyObserver> observer) {
  observers_.push_back(std::move(observer));

  // TODO(b/277902094): Ideally the matchers should not be reinitialized when
  // adding an observer, the current state (or prefs having values) should be
  // used to trigger enabled/disabled updates.
  OnPolicyUpdated();
}

void DeviceTrustConnectorService::OnPolicyUpdated() {
  DCHECK(IsDeviceTrustConnectorFeatureEnabled());

  const base::Value::List* url_patterns = GetPolicyUrlPatterns(profile_prefs_);

  if (!matcher_ || !matcher_->IsEmpty()) {
    // Reset the matcher.
    matcher_ = std::make_unique<url_matcher::URLMatcher>();
  }

  if (url_patterns && !url_patterns->empty()) {
    // Add the new endpoints to the conditions.
    url_matcher::util::AddAllowFilters(matcher_.get(), *url_patterns);
    OnInlinePolicyEnabled(DTCPolicyLevel::kBrowser);
    OnInlinePolicyEnabled(DTCPolicyLevel::kUser);
  } else {
    OnInlinePolicyDisabled(DTCPolicyLevel::kBrowser);
    OnInlinePolicyDisabled(DTCPolicyLevel::kUser);
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

}  // namespace enterprise_connectors
