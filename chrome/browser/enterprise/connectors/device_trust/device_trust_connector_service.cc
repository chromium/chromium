// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "components/policy/core/browser/url_util.h"
#include "components/prefs/pref_service.h"
#include "components/url_matcher/url_matcher.h"
#include "url/gurl.h"

namespace enterprise_connectors {

namespace {

bool IsFeatureEnabled() {
  return base::FeatureList::IsEnabled(kDeviceTrustConnectorEnabled);
}

const base::ListValue* GetPolicyUrlPatterns(PrefService* prefs) {
  return prefs->GetList(kContextAwareAccessSignalsAllowlistPref);
}

bool ConnectorPolicyHasValues(PrefService* profile_prefs) {
  const auto* list = GetPolicyUrlPatterns(profile_prefs);
  return list && !list->GetList().empty();
}

}  // namespace

// static
bool DeviceTrustConnectorService::IsConnectorEnabled(
    PrefService* profile_prefs) {
  if (!IsFeatureEnabled() || !profile_prefs) {
    return false;
  }

  return ConnectorPolicyHasValues(profile_prefs);
}

DeviceTrustConnectorService::DeviceTrustConnectorService(
    PrefService* profile_prefs)
    : profile_prefs_(profile_prefs),
      matcher_(std::make_unique<url_matcher::URLMatcher>()) {
  DCHECK(profile_prefs_);
  if (!IsFeatureEnabled()) {
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
  return DeviceTrustConnectorService::IsConnectorEnabled(profile_prefs_);
}

bool DeviceTrustConnectorService::Watches(const GURL& url) const {
  return !matcher_->MatchURL(url).empty();
}

void DeviceTrustConnectorService::OnConnectorEnabled() {
  // No-op by default.
}

void DeviceTrustConnectorService::OnPolicyUpdated() {
  DCHECK(IsFeatureEnabled());

  const base::ListValue* url_patterns = GetPolicyUrlPatterns(profile_prefs_);

  url_matcher::URLMatcherConditionSet::ID condition_set_id(0);
  if (!matcher_->IsEmpty()) {
    // Clear old conditions in case they exist.
    matcher_->RemoveConditionSets({condition_set_id});
  }

  if (url_patterns && !url_patterns->GetList().empty()) {
    // Add the new endpoints to the conditions.
    policy::url_util::AddFilters(matcher_.get(), /*allow=*/true,
                                 &condition_set_id, url_patterns);

    // Call the hook which signals that the connector has been enabled.
    OnConnectorEnabled();
  }
}

}  // namespace enterprise_connectors
