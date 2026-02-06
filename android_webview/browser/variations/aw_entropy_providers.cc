// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/variations/aw_entropy_providers.h"

#include <set>

#include "base/no_destructor.h"
#include "components/metrics/entropy_state.h"

namespace android_webview {

AwEntropyProviders::AwEntropyProviders(
    std::unique_ptr<const variations::EntropyProviders> standard_providers,
    uint32_t nonembedded_low_entropy_source,
    std::unique_ptr<const std::set<std::string_view>>
        nonembedded_low_entropy_source_allowlist)
    : variations::EntropyProviders(
          /*high_entropy_source=*/"",
          /*low_entropy_source=*/
          {static_cast<uint32_t>(standard_providers->low_entropy_source()),
           static_cast<uint32_t>(standard_providers->low_entropy_domain())},
          /*limited_entropy_source=*/"",
          standard_providers->benchmarking_enabled()),
      delegating_provider_(
          std::move(standard_providers),
          nonembedded_low_entropy_source,
          std::move(nonembedded_low_entropy_source_allowlist)) {}

AwEntropyProviders::~AwEntropyProviders() = default;

const base::FieldTrial::EntropyProvider& AwEntropyProviders::low_entropy()
    const {
  return delegating_provider_;
}

const base::FieldTrial::EntropyProvider& AwEntropyProviders::default_entropy()
    const {
  return delegating_provider_;
}

AwEntropyProviders::DelegatingEntropyProvider::DelegatingEntropyProvider(
    std::unique_ptr<const variations::EntropyProviders> standard_providers,
    uint32_t nonembedded_low_entropy_source,
    std::unique_ptr<const std::set<std::string_view>>
        nonembedded_low_entropy_source_allowlist)
    : standard_providers_(std::move(standard_providers)),
      nonembedded_low_entropy_source_allowlist_(
          std::move(nonembedded_low_entropy_source_allowlist)),
      nonembedded_low_entropy_source_provider_(
          {nonembedded_low_entropy_source,
           metrics::EntropyState::kMaxLowEntropySize}) {}

AwEntropyProviders::DelegatingEntropyProvider::~DelegatingEntropyProvider() =
    default;

double AwEntropyProviders::DelegatingEntropyProvider::GetEntropyForTrial(
    std::string_view trial_name,
    uint32_t randomization_seed) const {
  if (nonembedded_low_entropy_source_allowlist_->contains(trial_name)) {
    return nonembedded_low_entropy_source_provider_.GetEntropyForTrial(
        trial_name, randomization_seed);
  }
  return standard_providers_->low_entropy().GetEntropyForTrial(
      trial_name, randomization_seed);
}

}  // namespace android_webview
