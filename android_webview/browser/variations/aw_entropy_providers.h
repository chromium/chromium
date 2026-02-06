// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_VARIATIONS_AW_ENTROPY_PROVIDERS_H_
#define ANDROID_WEBVIEW_BROWSER_VARIATIONS_AW_ENTROPY_PROVIDERS_H_

#include <memory>
#include <set>
#include <string_view>

#include "base/metrics/field_trial.h"
#include "components/variations/entropy_provider.h"

namespace android_webview {

// A custom EntropyProviders that wraps an existing (standard) EntropyProviders.
// It uses a low entropy provider that uses a specific
// `nonembedded_low_entropy_source` for studies in an allowlist, and delegates
// to the standard provider for all other studies.
class AwEntropyProviders : public variations::EntropyProviders {
 public:
  // `standard_providers`: The providers created by MetricsStateManager.
  // `nonembedded_low_entropy_source`: The low entropy source (0-7999) for
  // allowlisted studies.
  // `nonembedded_low_entropy_source_allowlist`: The set of study names to use
  // the nonembedded low entropy source for.
  AwEntropyProviders(
      std::unique_ptr<const variations::EntropyProviders> standard_providers,
      uint32_t nonembedded_low_entropy_source,
      std::unique_ptr<const std::set<std::string_view>>
          nonembedded_low_entropy_source_allowlist);

  ~AwEntropyProviders() override;

  // variations::EntropyProviders:
  const base::FieldTrial::EntropyProvider& low_entropy() const override;
  const base::FieldTrial::EntropyProvider& default_entropy() const override;

 private:
  // An entropy provider that returns the nonembedded_low_entropy_source for
  // allowlisted studies and delegates to the standard provider for others.
  class DelegatingEntropyProvider : public base::FieldTrial::EntropyProvider {
   public:
    DelegatingEntropyProvider(
        std::unique_ptr<const variations::EntropyProviders> standard_providers,
        uint32_t nonembedded_low_entropy_source,
        std::unique_ptr<const std::set<std::string_view>>
            nonembedded_low_entropy_source_allowlist);
    ~DelegatingEntropyProvider() override;

    // base::FieldTrial::EntropyProvider:
    double GetEntropyForTrial(std::string_view trial_name,
                              uint32_t randomization_seed) const override;

   private:
    std::unique_ptr<const variations::EntropyProviders> standard_providers_;
    std::unique_ptr<const std::set<std::string_view>>
        nonembedded_low_entropy_source_allowlist_;
    variations::NormalizedMurmurHashEntropyProvider
        nonembedded_low_entropy_source_provider_;
  };

  DelegatingEntropyProvider delegating_provider_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_VARIATIONS_AW_ENTROPY_PROVIDERS_H_
