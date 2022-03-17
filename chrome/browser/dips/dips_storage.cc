// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_storage.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace dips {

DIPSStorage::DIPSStorage() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DIPSStorage::~DIPSStorage() = default;

DIPSState DIPSStorage::Read(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string site = GetSite(url);
  auto iter = map_.find(site);
  if (iter == map_.end()) {
    return DIPSState(this, std::move(site), /*was_loaded=*/false);
  }
  const StateValue& value = iter->second;
  DIPSState state(this, std::move(site), /*was_loaded=*/true);
  state.set_site_storage_time(value.site_storage_time);
  state.set_user_interaction_time(value.user_interaction_time);
  return state;
}

void DIPSStorage::Write(const DIPSState& state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StateValue& value = map_[state.site()];
  value.site_storage_time = state.site_storage_time();
  value.user_interaction_time = state.user_interaction_time();
}

/* static */
std::string DIPSStorage::GetSite(const GURL& url) {
  // TODO(crbug.com/1306935): use privacy boundary instead of eTLD+1
  const auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  return domain.empty() ? url.host() : domain;
}

}  // namespace dips
