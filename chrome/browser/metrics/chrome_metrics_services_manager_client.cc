// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"
#include "chrome/browser/metrics/variations/ui_string_overrider_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif  // OS_ANDROID

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/install_static/install_util.h"
#include "components/crash/content/app/crash_export_thunks.h"
#include "components/crash/content/app/crashpad.h"
#endif  // OS_WIN

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"
#endif  // defined(OS_CHROMEOS)

namespace metrics {

namespace internal {
// Metrics reporting feature. This feature, along with user consent, controls if
// recording and reporting are enabled. If the feature is enabled, but no
// consent is given, then there will be no recording or reporting.
const base::Feature kMetricsReportingFeature{"MetricsReporting",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// A feature controlling whether all clients in the OutOfReportingSample group
// should discard their uploads, regardless of which user consent flow they
// went through. When disabled, only opt-out users will discard uploads.
const base::Feature kMetricsDownsampleConsistentlyFeature{
    "MetricsDownsampleConsistently", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace internal
}  // namespace metrics

namespace {

// Name of the variations param that defines the sampling rate.
const char kRateParamName[] = "sampling_rate_per_mille";

// Posts |GoogleUpdateSettings::StoreMetricsClientInfo| on blocking pool thread
// because it needs access to IO and cannot work from UI thread.
void PostStoreMetricsClientInfo(const metrics::ClientInfo& client_info) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GoogleUpdateSettings::StoreMetricsClientInfo,
                     client_info));
}

// Appends a group to the sampling controlling |trial|. The group will be
// associated with a variation param for reporting sampling |rate| in per mille.
void AppendSamplingTrialGroup(const std::string& group_name,
                              int rate,
                              base::FieldTrial* trial) {
  std::map<std::string, std::string> params = {
      {kRateParamName, base::NumberToString(rate)}};
  variations::AssociateVariationParams(trial->trial_name(), group_name, params);
  trial->AppendGroup(group_name, rate);
}

// Unless the DownsampleConsistently feature is enabled, only clients that were
// given an opt-out metrics-reporting consent flow are eligible for sampling.
bool IsClientEligibleForSampling(PrefService* local_state) {
  return base::FeatureList::IsEnabled(
             metrics::internal::kMetricsDownsampleConsistentlyFeature) ||
         metrics::GetMetricsReportingDefaultState(local_state) ==
             metrics::EnableMetricsDefault::OPT_OUT;
}

// Implementation of IsClientInSample() that takes a PrefService param.
bool IsClientInSampleImpl(PrefService* local_state) {
  // Test the MetricsReporting feature for all users to ensure that the trial
  // is reported.
  bool is_in_sample_group =
      base::FeatureList::IsEnabled(metrics::internal::kMetricsReportingFeature);
  // Until the DownsampleConsistently feature is rolled out, only some clients
  // are eligible for downsampling. Clients that aren't eligible should always
  // send reports when they have opted to do so, but should still report their
  // group assignment to the trial controlling downsampling.
  return is_in_sample_group || !IsClientEligibleForSampling(local_state);
}

#if defined(OS_CHROMEOS)
// Callback to update the metrics reporting state when the Chrome OS metrics
// reporting setting changes.
void OnCrosMetricsReportingSettingChange() {
  bool enable_metrics = chromeos::StatsReportingController::Get()->IsEnabled();
  ChangeMetricsReportingState(enable_metrics);
}
#endif

// Returns the name of a key under HKEY_CURRENT_USER that can be used to store
// backups of metrics data. Unused except on Windows.
base::string16 GetRegistryBackupKey() {
#if defined(OS_WIN)
  return install_static::GetRegistryPath().append(L"\\StabilityMetrics");
#else
  return base::string16();
#endif
}

}  // namespace


class ChromeMetricsServicesManagerClient::ChromeEnabledStateProvider
    : public metrics::EnabledStateProvider {
 public:
  explicit ChromeEnabledStateProvider(PrefService* local_state)
      : local_state_(local_state) {}
  ~ChromeEnabledStateProvider() override {}

  bool IsConsentGiven() const override {
    return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled(
        local_state_);
  }

  bool IsReportingEnabled() const override {
    return metrics::EnabledStateProvider::IsReportingEnabled() &&
           IsClientInSampleImpl(local_state_);
  }

 private:
  PrefService* const local_state_;

  DISALLOW_COPY_AND_ASSIGN(ChromeEnabledStateProvider);
};

ChromeMetricsServicesManagerClient::ChromeMetricsServicesManagerClient(
    PrefService* local_state)
    : enabled_state_provider_(
          std::make_unique<ChromeEnabledStateProvider>(local_state)),
      local_state_(local_state) {
  DCHECK(local_state);
}

ChromeMetricsServicesManagerClient::~ChromeMetricsServicesManagerClient() {}

// static
void ChromeMetricsServicesManagerClient::CreateFallbackSamplingTrial(
    version_info::Channel channel,
    base::FeatureList* feature_list) {
  // The trial name must be kept in sync with the server config controlling
  // sampling. If they don't match, then clients will be shuffled into different
  // groups when the server config takes over from the fallback trial.
  static const char kTrialName[] = "MetricsAndCrashSampling";
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kTrialName, 1000, "Default", base::FieldTrial::ONE_TIME_RANDOMIZED,
          nullptr));

  // On all channels except stable, we sample out at a minimal rate to ensure
  // the code paths are exercised in the wild before hitting stable.
  int sampled_in_rate = 990;
  int sampled_out_rate = 10;
  if (channel == version_info::Channel::STABLE) {
    sampled_in_rate = 100;
    sampled_out_rate = 900;
  }

  // Like the trial name, the order that these two groups are added to the trial
  // must be kept in sync with the order that they appear in the server config.
  // For future sanity purposes, the desired order is:
  // OutOfReportingSample, InReportingSample

  static const char kSampledOutGroup[] = "OutOfReportingSample";
  AppendSamplingTrialGroup(kSampledOutGroup, sampled_out_rate, trial.get());

  static const char kInSampleGroup[] = "InReportingSample";
  AppendSamplingTrialGroup(kInSampleGroup, sampled_in_rate, trial.get());

  // Setup the feature. This must be done after all groups are added since
  // GetGroupNameWithoutActivation() will finalize the group choice.
  const std::string& group_name = trial->GetGroupNameWithoutActivation();
  feature_list->RegisterFieldTrialOverride(
      metrics::internal::kMetricsReportingFeature.name,
      group_name == kSampledOutGroup
          ? base::FeatureList::OVERRIDE_DISABLE_FEATURE
          : base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial.get());
}

// static
bool ChromeMetricsServicesManagerClient::IsClientInSample() {
  return IsClientInSampleImpl(g_browser_process->local_state());
}

// static
bool ChromeMetricsServicesManagerClient::GetSamplingRatePerMille(int* rate) {
  // The population that is NOT eligible for sampling in considered "in sample",
  // but does not have a defined sample rate.
  if (!IsClientEligibleForSampling(g_browser_process->local_state()))
    return false;

  std::string rate_str = variations::GetVariationParamValueByFeature(
      metrics::internal::kMetricsReportingFeature, kRateParamName);
  if (rate_str.empty())
    return false;

  if (!base::StringToInt(rate_str, rate) || *rate > 1000)
    return false;

  return true;
}

#if defined(OS_CHROMEOS)
void ChromeMetricsServicesManagerClient::OnCrosSettingsCreated() {
  reporting_setting_observer_ =
      chromeos::StatsReportingController::Get()->AddObserver(
          base::Bind(&OnCrosMetricsReportingSettingChange));
  // Invoke the callback once initially to set the metrics reporting state.
  OnCrosMetricsReportingSettingChange();
}
#endif

const metrics::EnabledStateProvider&
ChromeMetricsServicesManagerClient::GetEnabledStateProviderForTesting() {
  return *enabled_state_provider_;
}

std::unique_ptr<rappor::RapporServiceImpl>
ChromeMetricsServicesManagerClient::CreateRapporServiceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return std::make_unique<rappor::RapporServiceImpl>(
      local_state_, base::Bind(&chrome::IsIncognitoSessionActive));
}

std::unique_ptr<variations::VariationsService>
ChromeMetricsServicesManagerClient::CreateVariationsService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return variations::VariationsService::Create(
      std::make_unique<ChromeVariationsServiceClient>(), local_state_,
      GetMetricsStateManager(), switches::kDisableBackgroundNetworking,
      chrome_variations::CreateUIStringOverrider(),
      base::BindOnce(&content::GetNetworkConnectionTracker));
}

std::unique_ptr<metrics::MetricsServiceClient>
ChromeMetricsServicesManagerClient::CreateMetricsServiceClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return ChromeMetricsServiceClient::Create(GetMetricsStateManager());
}

metrics::MetricsStateManager*
ChromeMetricsServicesManagerClient::GetMetricsStateManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!metrics_state_manager_) {
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_, enabled_state_provider_.get(), GetRegistryBackupKey(),
        base::Bind(&PostStoreMetricsClientInfo),
        base::Bind(&GoogleUpdateSettings::LoadMetricsClientInfo));
  }
  return metrics_state_manager_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeMetricsServicesManagerClient::GetURLLoaderFactory() {
  return g_browser_process->system_network_context_manager()
      ->GetSharedURLLoaderFactory();
}

bool ChromeMetricsServicesManagerClient::IsMetricsReportingEnabled() {
  return enabled_state_provider_->IsReportingEnabled();
}

bool ChromeMetricsServicesManagerClient::IsMetricsConsentGiven() {
  return enabled_state_provider_->IsConsentGiven();
}

bool ChromeMetricsServicesManagerClient::IsIncognitoSessionActive() {
#if defined(OS_ANDROID)
  // This differs from TabModelList::IsOffTheRecordSessionActive in that it
  // does not ignore TabModels that have no open tabs, because it may be checked
  // before tabs get added to the TabModel. This means it may be more
  // conservative in case unused TabModels are not cleaned up, but it seems to
  // work correctly.
  // TODO(crbug/741888): Check if TabModelList's version can be updated safely.
  for (TabModelList::const_iterator i = TabModelList::begin();
       i != TabModelList::end(); i++) {
    if ((*i)->IsOffTheRecord())
      return true;
  }

  return false;
#else
  // Depending directly on BrowserList, since that is the implementation
  // that we get correct notifications for.
  return BrowserList::IsIncognitoSessionActive();
#endif
}

#if defined(OS_WIN)
void ChromeMetricsServicesManagerClient::UpdateRunningServices(
    bool may_record,
    bool may_upload) {
  // First, set the registry value so that Crashpad will have the sampling state
  // now and for subsequent runs.
  install_static::SetCollectStatsInSample(IsClientInSample());

  // Next, get Crashpad to pick up the sampling state for this session.
  // Crashpad will use the kRegUsageStatsInSample registry value to apply
  // sampling correctly, but may_record already reflects the sampling state.
  // This isn't a problem though, since they will be consistent.
  SetUploadConsent_ExportThunk(may_record && may_upload);
}
#endif  // defined(OS_WIN)
