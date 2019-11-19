// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_feature_list_creator.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_pref_names.h"
#include "android_webview/browser/aw_variations_seed_bridge.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "cc/base/switches.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_service.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "services/preferences/tracked/segregated_pref_store.h"

namespace android_webview {

namespace {

// These prefs go in the JsonPrefStore, and will persist across runs. Other
// prefs go in the InMemoryPrefStore, and will be lost when the process ends.
const char* const kPersistentPrefsWhitelist[] = {
    // Randomly-generated GUID which pseudonymously identifies uploaded metrics.
    metrics::prefs::kMetricsClientID,
    // Random seed value for variation's entropy providers. Used to assign
    // experiment groups.
    metrics::prefs::kMetricsLowEntropySource,
    // Logged directly in the ChromeUserMetricsExtension proto.
    metrics::prefs::kInstallDate,
    metrics::prefs::kMetricsSessionID,
    // Current and past country codes, to filter variations studies by country.
    variations::prefs::kVariationsCountry,
    variations::prefs::kVariationsPermanentConsistencyCountry,
    // Last variations seed fetch date/time, used for histograms and to
    // determine if the seed is expired.
    variations::prefs::kVariationsLastFetchTime,
    variations::prefs::kVariationsSeedDate,
    // Number of consecutive WebView browser process initializations with a
    // stale variations seed.
    prefs::kRestartsWithStaleSeed,
};

// Shows notifications which correspond to PersistentPrefStore's reading errors.
void HandleReadError(PersistentPrefStore::PrefReadError error) {}

base::FilePath GetPrefStorePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path);
  path = path.Append(FILE_PATH_LITERAL("pref_store"));
  return path;
}

std::unique_ptr<PrefService> CreatePrefService() {
  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();

  metrics::MetricsService::RegisterPrefs(pref_registry.get());
  variations::VariationsService::RegisterPrefs(pref_registry.get());
  pref_registry->RegisterIntegerPref(prefs::kRestartsWithStaleSeed, 0);

  AwBrowserProcess::RegisterNetworkContextLocalStatePrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;

  std::set<std::string> persistent_prefs;
  for (const char* const pref_name : kPersistentPrefsWhitelist)
    persistent_prefs.insert(pref_name);

  // SegregatedPrefStore may be validated with a MAC (message authentication
  // code). On Android, the store is protected by app sandboxing, so validation
  // is unnnecessary. Thus validation_delegate is null.
  pref_service_factory.set_user_prefs(base::MakeRefCounted<SegregatedPrefStore>(
      base::MakeRefCounted<InMemoryPrefStore>(),
      base::MakeRefCounted<JsonPrefStore>(GetPrefStorePath()), persistent_prefs,
      mojo::Remote<::prefs::mojom::TrackedPreferenceValidationDelegate>()));

  pref_service_factory.set_read_error_callback(
      base::BindRepeating(&HandleReadError));

  return pref_service_factory.Create(pref_registry);
}

void CountOrRecordRestartsWithStaleSeed(PrefService* local_state,
                                        bool is_loaded_seed_fresh) {
  int restarts = local_state->GetInteger(prefs::kRestartsWithStaleSeed);
  if (!is_loaded_seed_fresh) {
    // If the seed isn't fresh, increase the restart count pref.
    local_state->SetInteger(prefs::kRestartsWithStaleSeed, restarts + 1);
  } else if (restarts > 0) {
    // If the seed is fresh and the last restart had a stale seed, record and
    // reset the restart count.
    local_state->SetInteger(prefs::kRestartsWithStaleSeed, 0);
    UMA_HISTOGRAM_COUNTS_100("Variations.RestartsWithStaleSeed", restarts);
  }
}

}  // namespace

AwFeatureListCreator::AwFeatureListCreator()
    : aw_field_trials_(std::make_unique<AwFieldTrials>()) {}

AwFeatureListCreator::~AwFeatureListCreator() {}

void AwFeatureListCreator::SetUpFieldTrials() {
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

  std::unique_ptr<variations::SeedResponse> seed = GetAndClearJavaSeed();
  base::Time null_time;
  base::Time seed_date =
      seed ? base::Time::FromJavaTime(seed->date) : null_time;
  variations::UIStringOverrider ui_string_overrider;
  client_ = std::make_unique<AwVariationsServiceClient>();
  auto seed_store = std::make_unique<variations::VariationsSeedStore>(
      local_state_.get(), /*initial_seed=*/std::move(seed),
      /*on_initial_seed_stored=*/base::DoNothing());

  // We set the seed fetch time to when the service downloaded the seed rather
  // than base::Time::Now() because we want to compute seed freshness based on
  // the initial download time, which happened in the service at some earlier
  // point.
  if (!seed_date.is_null())
    seed_store->RecordLastFetchTime(seed_date);

  variations_field_trial_creator_ =
      std::make_unique<variations::VariationsFieldTrialCreator>(
          local_state_.get(), client_.get(), std::move(seed_store),
          ui_string_overrider);
  variations_field_trial_creator_->OverrideVariationsPlatform(
      variations::Study::PLATFORM_ANDROID_WEBVIEW);

  // Safe Mode is a feature which reverts to a previous variations seed if the
  // current one is suspected to be causing crashes, or preventing new seeds
  // from being downloaded. It's not implemented for WebView because 1) it's
  // difficult for WebView to implement Safe Mode's crash detection, and 2)
  // downloading and disseminating seeds is handled by the WebView service,
  // which itself doesn't support variations; therefore a bad seed shouldn't be
  // able to break seed downloads. See https://crbug.com/801771 for more info.
  std::set<std::string> unforceable_field_trials;
  variations::SafeSeedManager ignored_safe_seed_manager(true,
                                                        local_state_.get());

  // Populate FieldTrialList. Since low_entropy_provider is null, it will fall
  // back to the provider we previously gave to FieldTrialList, which is a low
  // entropy provider.
  variations_field_trial_creator_->SetupFieldTrials(
      cc::switches::kEnableGpuBenchmarking, switches::kEnableFeatures,
      switches::kDisableFeatures, unforceable_field_trials,
      std::vector<std::string>(),
      content::GetSwitchDependentFeatureOverrides(
          *base::CommandLine::ForCurrentProcess()),
      /*low_entropy_provider=*/nullptr, std::make_unique<base::FeatureList>(),
      aw_field_trials_.get(), &ignored_safe_seed_manager);

  CountOrRecordRestartsWithStaleSeed(local_state_.get(), IsSeedFresh());
}

void AwFeatureListCreator::CreateLocalState() {
  browser_policy_connector_ = std::make_unique<AwBrowserPolicyConnector>();
  local_state_ = CreatePrefService();
}

void AwFeatureListCreator::CreateFeatureListAndFieldTrials() {
  CreateLocalState();
  AwMetricsServiceClient::GetInstance()->Initialize(local_state_.get());
  SetUpFieldTrials();
}

}  // namespace android_webview
