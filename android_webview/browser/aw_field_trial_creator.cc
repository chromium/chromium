// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_field_trial_creator.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_metrics_service_client.h"
#include "android_webview/browser/aw_variations_seed_bridge.h"
#include "base/base_switches.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "cc/base/switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/service/safe_seed_manager.h"

namespace android_webview {

AwFieldTrialCreator::AwFieldTrialCreator()
    : aw_field_trials_(std::make_unique<AwFieldTrials>()) {}

AwFieldTrialCreator::~AwFieldTrialCreator() {}

void AwFieldTrialCreator::SetUpFieldTrials(PrefService* pref_service) {
  auto* metrics_client = AwMetricsServiceClient::GetInstance();

  // Chrome uses the default entropy provider here (rather than low entropy
  // provider). The default provider needs to know whether UMA is enabled, but
  // WebView determines UMA by querying GMS, which is very slow. So WebView
  // always uses the low entropy provider. Both providers guarantee permanent
  // consistency, which is the main requirement. The difference is that the low
  // entropy provider has fewer unique experiment combinations. This is better
  // for privacy (since experiment state doesn't identify users), but also means
  // fewer combinations tested in the wild.
  DCHECK(!field_trial_list_);
  field_trial_list_ = std::make_unique<base::FieldTrialList>(
      metrics_client->CreateLowEntropyProvider());

  variations::UIStringOverrider ui_string_overrider;
  client_ = std::make_unique<AwVariationsServiceClient>();
  auto seed_store = std::make_unique<variations::VariationsSeedStore>(
      pref_service, /*initial_seed=*/GetAndClearJavaSeed(),
      /*on_initial_seed_stored=*/base::DoNothing());
  variations_field_trial_creator_ =
      std::make_unique<variations::VariationsFieldTrialCreator>(
          pref_service, client_.get(), std::move(seed_store),
          ui_string_overrider);
  variations_field_trial_creator_->OverrideVariationsPlatform(
      variations::Study::PLATFORM_ANDROID_WEBVIEW);

  // Unused by WebView, but required by
  // VariationsFieldTrialCreator::SetupFieldTrials().
  // TODO(isherman): We might want a more genuine SafeSeedManager:
  // https://crbug.com/801771
  std::set<std::string> unforceable_field_trials;
  variations::SafeSeedManager ignored_safe_seed_manager(true, pref_service);

  // Populate FieldTrialList. Since low_entropy_provider is null, it will fall
  // back to the provider we previously gave to FieldTrialList, which is a low
  // entropy provider. We only want one low entropy provider, because multiple
  // CachingPermutedEntropyProvider objects would all try to cache their values
  // in the same pref store, overwriting each other's.
  variations_field_trial_creator_->SetupFieldTrials(
      cc::switches::kEnableGpuBenchmarking, switches::kEnableFeatures,
      switches::kDisableFeatures, unforceable_field_trials,
      std::vector<std::string>(), /*low_entropy_provider=*/nullptr,
      std::make_unique<base::FeatureList>(), aw_field_trials_.get(),
      &ignored_safe_seed_manager);
}

}  // namespace android_webview
