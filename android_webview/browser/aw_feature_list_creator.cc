// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_feature_list_creator.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_context_store.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_feature_entries.h"
#include "android_webview/browser/aw_metrics_service_client_delegate.h"
#include "android_webview/browser/metrics/android_metrics_provider.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/supervised_user/aw_supervised_user_url_classifier.h"
#include "android_webview/browser/tracing/aw_tracing_delegate.h"
#include "android_webview/browser/variations/variations_seed_loader.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/proto/aw_variations_seed.pb.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "components/embedder_support/origin_trials/origin_trial_prefs.h"
#include "components/embedder_support/origin_trials/pref_names.h"
#include "components/metrics/android_metrics_helper.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/persistent_histograms.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_name_set.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/segregated_pref_store.h"
#include "components/tracing/common/pref_names.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_switches.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "net/base/features.h"
#include "net/nqe/pref_names.h"

namespace android_webview {

namespace {

bool g_signature_verification_enabled = true;

// These prefs go in the JsonPrefStore, and will persist across runs. Other
// prefs go in the InMemoryPrefStore, and will be lost when the process ends.
const char* const kPersistentPrefsAllowlist[] = {
    // Restricted content blocking.
    android_webview::prefs::kShouldBlockRestrictedContent,

    // Origin Trial config overrides.
    embedder_support::prefs::kOriginTrialPublicKey,
    embedder_support::prefs::kOriginTrialDisabledFeatures,
    embedder_support::prefs::kOriginTrialDisabledTokens,
    // Randomly-generated GUID which pseudonymously identifies uploaded metrics.
    metrics::prefs::kMetricsClientID,
    // Random seed value for variation's entropy providers. Used to assign
    // experiment groups.
    metrics::prefs::kMetricsLowEntropySource,
    // File metrics metadata.
    metrics::prefs::kMetricsFileMetricsMetadata,
    // Logged directly in the ChromeUserMetricsExtension proto.
    metrics::prefs::kInstallDate,
    metrics::prefs::kMetricsReportingEnabledTimestamp,
    metrics::prefs::kMetricsSessionID,
    // Logged in system_profile.stability fields.
    metrics::prefs::kStabilityFileMetricsUnsentFilesCount,
    metrics::prefs::kStabilityFileMetricsUnsentSamplesCount,
    metrics::prefs::kStabilityLaunchCount,
    metrics::prefs::kStabilityPageLoadCount,
    metrics::prefs::kStabilityRendererLaunchCount,
    // Unsent logs.
    metrics::prefs::kMetricsInitialLogs,
    metrics::prefs::kMetricsOngoingLogs,
    // Unsent logs metadata.
    metrics::prefs::kMetricsInitialLogsMetadata,
    metrics::prefs::kMetricsOngoingLogsMetadata,
    net::nqe::kNetworkQualities,
    // Current and past country codes, to filter variations studies by country.
    variations::prefs::kVariationsCountry,
    variations::prefs::kVariationsPermanentConsistencyCountry,
    // Last variations seed fetch date/time, used for histograms and to
    // determine if the seed is expired.
    variations::prefs::kVariationsLastFetchTime,
    variations::prefs::kVariationsSeedDate,

    // The state of the previous background tracing session.
    tracing::kBackgroundTracingSessionState,

    // System-level info.
    metrics::prefs::kVersionCodePref,
    prefs::kPrimaryCpuAbiBitnessPref,

    // Records about profiles/contexts and their stored data
    prefs::kProfileListPref,
    prefs::kProfileCounterPref,
};

void HandleReadError(PersistentPrefStore::PrefReadError error) {}

base::FilePath GetPrefStorePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path);
  path = path.Append(FILE_PATH_LITERAL("pref_store"));
  return path;
}

// Adds WebView-specific switch-dependent feature overrides on top of the ones
// from the content layer.
std::vector<base::FeatureList::FeatureOverrideInfo>
GetSwitchDependentFeatureOverrides(const base::CommandLine& command_line) {
  std::vector<base::FeatureList::FeatureOverrideInfo> feature_overrides =
      content::GetSwitchDependentFeatureOverrides(command_line);

  return feature_overrides;
}

}  // namespace

AwFeatureListCreator::AwFeatureListCreator()
    : aw_field_trials_(std::make_unique<AwFieldTrials>()) {}

AwFeatureListCreator::~AwFeatureListCreator() {}

void AwFeatureListCreator::CreateFeatureListAndFieldTrials() {
  TRACE_EVENT0("startup",
               "AwFeatureListCreator::CreateFeatureListAndFieldTrials");
  CreateLocalState();
  AwMetricsServiceClient::SetInstance(std::make_unique<AwMetricsServiceClient>(
      std::make_unique<AwMetricsServiceClientDelegate>()));
  AwMetricsServiceClient::GetInstance()->Initialize(local_state_.get());
  SetUpFieldTrials();
}

void AwFeatureListCreator::CreateLocalState() {
  browser_policy_connector_ = std::make_unique<AwBrowserPolicyConnector>();
  local_state_ = CreatePrefService();
}

void AwFeatureListCreator::DisableSignatureVerificationForTesting() {
  g_signature_verification_enabled = false;
}

std::unique_ptr<PrefService> AwFeatureListCreator::CreatePrefService() {
  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();

  AwMetricsServiceClient::RegisterMetricsPrefs(pref_registry.get());
  variations::VariationsService::RegisterPrefs(pref_registry.get());

  embedder_support::OriginTrialPrefs::RegisterPrefs(pref_registry.get());
  AwBrowserProcess::RegisterNetworkContextLocalStatePrefs(pref_registry.get());
  AwBrowserProcess::RegisterEnterpriseAuthenticationAppLinkPolicyPref(
      pref_registry.get());
  AwTracingDelegate::RegisterPrefs(pref_registry.get());
  AwBrowserContextStore::RegisterPrefs(pref_registry.get());
  AwSupervisedUserUrlClassifier::RegisterPrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;

  PrefNameSet persistent_prefs;
  for (const char* const pref_name : kPersistentPrefsAllowlist)
    persistent_prefs.insert(pref_name);

  persistent_prefs.insert(std::string(metrics::prefs::kMetricsLastSeenPrefix) +
                          kBrowserMetricsName);
  persistent_prefs.insert(std::string(metrics::prefs::kMetricsLastSeenPrefix) +
                          metrics::kCrashpadHistogramAllocatorName);

  // SegregatedPrefStore may be validated with a MAC (message authentication
  // code). On Android, the store is protected by app sandboxing, so validation
  // is unnnecessary. Thus validation_delegate is null.
  pref_service_factory.set_user_prefs(base::MakeRefCounted<SegregatedPrefStore>(
      base::MakeRefCounted<InMemoryPrefStore>(),
      base::MakeRefCounted<JsonPrefStore>(GetPrefStorePath()),
      std::move(persistent_prefs)));

  pref_service_factory.set_managed_prefs(
      base::MakeRefCounted<policy::ConfigurationPolicyPrefStore>(
          browser_policy_connector_.get(),
          browser_policy_connector_->GetPolicyService(),
          browser_policy_connector_->GetHandlerList(),
          policy::POLICY_LEVEL_MANDATORY));

  pref_service_factory.set_read_error_callback(
      base::BindRepeating(&HandleReadError));

  return pref_service_factory.Create(pref_registry);
}

void AwFeatureListCreator::SetUpFieldTrials() {
  // The FieldTrialList should have been instantiated in
  // AndroidMetricsServiceClient::Initialize().
  DCHECK(base::FieldTrialList::GetInstance());

  // Convert the AwVariationsSeed proto to a SeedResponse object.
  std::unique_ptr<AwVariationsSeed> seed_proto = TakeSeed();
  std::unique_ptr<variations::SeedResponse> seed;
  base::Time seed_date;  // Initializes to null time.
  if (seed_proto) {
    // We set the seed fetch time to when the service downloaded the seed rather
    // than base::Time::Now() because we want to compute seed freshness based on
    // the initial download time, which happened in the service at some earlier
    // point.
    seed_date = base::Time::FromMillisecondsSinceUnixEpoch(seed_proto->date());

    seed = std::make_unique<variations::SeedResponse>();
    seed->data = seed_proto->seed_data();
    seed->signature = seed_proto->signature();
    seed->country = seed_proto->country();
    seed->date = seed_date;
    seed->is_gzip_compressed = seed_proto->is_gzip_compressed();
  }

  client_ = std::make_unique<AwVariationsServiceClient>();
  auto seed_store = std::make_unique<variations::VariationsSeedStore>(
      local_state_.get(), /*initial_seed=*/std::move(seed),
      /*signature_verification_enabled=*/g_signature_verification_enabled,
      std::make_unique<variations::VariationsSafeSeedStoreLocalState>(
          local_state_.get()),
      /*use_first_run_prefs=*/false);

  if (!seed_date.is_null())
    seed_store->RecordLastFetchTime(seed_date);

  variations::UIStringOverrider ui_string_overrider;
  variations_field_trial_creator_ =
      std::make_unique<variations::VariationsFieldTrialCreator>(
          client_.get(), std::move(seed_store), ui_string_overrider,
          // Limited entropy field trials are not supported on WebView.
          /*limited_entropy_synthetic_trial=*/nullptr);
  variations_field_trial_creator_->OverrideVariationsPlatform(
      variations::Study::PLATFORM_ANDROID_WEBVIEW);

  // Safe Mode is a feature which reverts to a previous variations seed if the
  // current one is suspected to be causing crashes, or preventing new seeds
  // from being downloaded. It's not implemented for WebView because 1) it's
  // difficult for WebView to implement Safe Mode's crash detection, and 2)
  // downloading and disseminating seeds is handled by the WebView service,
  // which itself doesn't support variations; therefore a bad seed shouldn't be
  // able to break seed downloads. See https://crbug.com/801771 for more info.
  variations::SafeSeedManager ignored_safe_seed_manager(local_state_.get());

  base::Time fetchTime =
      variations_field_trial_creator_->CalculateSeedFreshness();
  long seedFreshnessMinutes = (base::Time::Now() - fetchTime).InMinutes();
  CacheSeedFreshness(seedFreshnessMinutes);

  auto feature_list = std::make_unique<base::FeatureList>();
  std::vector<std::string> variation_ids =
      aw_feature_entries::RegisterEnabledFeatureEntries(feature_list.get());

  auto* metrics_client = AwMetricsServiceClient::GetInstance();
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // Populate FieldTrialList.
  // If you update this, consider whether "WebViewEnvironment" in
  // components/variations/variations_seed_processor_unittest.cc needs updates.
  // TODO(b/263797385): Re-evaluate if we can add entropy source id to
  // variations ids for WebView or not.
  variations_field_trial_creator_->SetUpFieldTrials(
      variation_ids,
      command_line->GetSwitchValueASCII(
          variations::switches::kForceVariationIds),
      GetSwitchDependentFeatureOverrides(*command_line),
      std::move(feature_list), metrics_client->metrics_state_manager(),
      metrics_client->GetSyntheticTrialRegistry(), aw_field_trials_.get(),
      &ignored_safe_seed_manager,
      /*add_entropy_source_to_variations_ids=*/false);
}

}  // namespace android_webview
