// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/mock_first_party_sets_handler.h"

#include <string>

#include "base/callback.h"
#include "base/types/optional_util.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace first_party_sets {

MockFirstPartySetsHandler::MockFirstPartySetsHandler() = default;
MockFirstPartySetsHandler::~MockFirstPartySetsHandler() = default;

bool MockFirstPartySetsHandler::IsEnabled() const {
  return true;
}

void MockFirstPartySetsHandler::SetPublicFirstPartySets(
    const base::Version& version,
    base::File sets_file) {}

void MockFirstPartySetsHandler::SetGlobalSetsForTesting(
    net::GlobalFirstPartySets global_sets) {
  global_sets_ = std::move(global_sets);
}

absl::optional<net::FirstPartySetEntry> MockFirstPartySetsHandler::FindEntry(
    const net::SchemefulSite& site,
    const net::FirstPartySetsContextConfig& config) const {
  if (!global_sets_.has_value()) {
    return absl::nullopt;
  }
  return global_sets_->FindEntry(site, config);
}

void MockFirstPartySetsHandler::GetContextConfigForPolicy(
    const base::Value::Dict* policy,
    base::OnceCallback<void(net::FirstPartySetsContextConfig)> callback) {
  DCHECK(config_.has_value()) << " Need to call SetContextConfig first.";
  std::move(callback).Run(config_->Clone());
}

void MockFirstPartySetsHandler::ClearSiteDataOnChangedSetsForContext(
    base::RepeatingCallback<content::BrowserContext*()> browser_context_getter,
    const std::string& browser_context_id,
    net::FirstPartySetsContextConfig context_config,
    base::OnceCallback<void(net::FirstPartySetsContextConfig,
                            net::FirstPartySetsCacheFilter)> callback) {
  DCHECK(config_.has_value()) << " Need to call SetContextConfig first.";
  DCHECK(cache_filter_.has_value()) << " Need to call SetCacheFilter first.";
  std::move(callback).Run(config_->Clone(), cache_filter_->Clone());
}

void MockFirstPartySetsHandler::ResetForTesting() {
  global_sets_ = absl::nullopt;
  config_ = absl::nullopt;
  cache_filter_ = absl::nullopt;
}

void MockFirstPartySetsHandler::SetContextConfig(
    net::FirstPartySetsContextConfig config) {
  config_ = std::move(config);
}
void MockFirstPartySetsHandler::SetCacheFilter(
    net::FirstPartySetsCacheFilter cache_filter) {
  cache_filter_ = std::move(cache_filter);
}

}  // namespace first_party_sets
