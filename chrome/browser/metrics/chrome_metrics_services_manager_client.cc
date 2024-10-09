// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"

#include <map>
#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"
#include "chrome/browser/metrics/variations/ui_string_overrider_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/limited_entropy_synthetic_trial.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/android/metrics/uma_session_stats.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser_list.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
#include "base/win/registry.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/install_static/install_util.h"
#include "components/crash/core/app/crash_export_thunks.h"
#include "components/crash/core/app/crashpad.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "components/metrics/structured/recorder.h"               // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace metrics {
namespace internal {

// Metrics reporting feature. This feature, along with user consent, controls if
// recording and reporting are enabled. If the feature is enabled, but no
// consent is given, then there will be no recording or reporting.
BASE_FEATURE(kMetricsReportingFeature,
             "MetricsReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Same as |kMetricsReportingFeature|, but this feature is associated with a
// different trial, which has different sampling rates. This is due to a bug
// in which the old sampling rate was not being applied correctly. In order for
// the fix to not affect the overall sampling rate, this new feature was
// created. See crbug/1306481.
BASE_FEATURE(kPostFREFixMetricsReportingFeature,
             "PostFREFixMetricsReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Name of the variations param that defines the sampling rate.
const char kRateParamName[] = "sampling_rate_per_mille";

}  // namespace internal
}  // namespace metrics

namespace {

// Posts |GoogleUpdateSettings::StoreMetricsClientInfo| on blocking pool thread
// because it needs access to IO and cannot work from UI thread.
void PostStoreMetricsClientInfo(const metrics::ClientInfo& client_info) {
  // This must happen on the same sequence as the tasks to enable/disable
  // metrics reporting. Otherwise, this may run while disabling metrics
  // reporting if the user quickly enables and disables metrics reporting.
  GoogleUpdateSettings::CollectStatsConsentTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&GoogleUpdateSettings::StoreMetricsClientInfo,
                                client_info));
}

#if BUILDFLAG(IS_ANDROID)
// Returns true if we should use the new sampling trial and feature to determine
// sampling. See the comment on |kUsePostFREFixSamplingTrial| for more details.
bool ShouldUsePostFREFixSamplingTrial(PrefService* local_state) {
  return local_state->GetBoolean(metrics::prefs::kUsePostFREFixSamplingTrial);
}

bool ShouldUsePostFREFixSamplingTrial() {
  // We check for g_browser_process and local_state() because some unit tests
  // may reach this point without creating a test browser process and/or local
  // state.
  // TODO(crbug.com/40837610): Fix the unit tests so that we do not need to
  // check for g_browser_process and local_state().
  return g_browser_process && g_browser_process->local_state() &&
         ShouldUsePostFREFixSamplingTrial(g_browser_process->local_state());
}
#endif  // BUILDFLAG(IS_ANDROID)

// Implementation of IsClientInSample() that takes a PrefService param.
bool IsClientInSampleImpl(PrefService* local_state) {
  // Test the MetricsReporting or PostFREFixMetricsReporting feature (depending
  // on the |kUsePostFREFixSamplingTrial| pref and platform) for all users to
  // ensure that the trial is reported. See the comment on
  // |kUsePostFREFixSamplingTrial| for more details on why there are two
  // different features.
#if BUILDFLAG(IS_ANDROID)
  if (ShouldUsePostFREFixSamplingTrial(local_state)) {
    return base::FeatureList::IsEnabled(
        metrics::internal::kPostFREFixMetricsReportingFeature);
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(
      metrics::internal::kMetricsReportingFeature);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Callback to update the metrics reporting state when the Chrome OS metrics
// reporting setting changes.
void OnCrosMetricsReportingSettingChange(
    ChangeMetricsReportingStateCalledFrom called_from) {
  bool enable_metrics = ash::StatsReportingController::Get()->IsEnabled();
  ChangeMetricsReportingState(enable_metrics, called_from);
}
#endif

// Returns the name of a key under HKEY_CURRENT_USER that can be used to store
// backups of metrics data. Unused except on Windows.
std::wstring GetRegistryBackupKey() {
#if BUILDFLAG(IS_WIN)
  return install_static::GetRegistryPath().append(L"\\StabilityMetrics");
#else
  return std::wstring();
#endif
}

}  // namespace

class ChromeMetricsServicesManagerClient::ChromeEnabledStateProvider
    : public metrics::EnabledStateProvider {
 public:
  explicit ChromeEnabledStateProvider(PrefService* local_state)
      : local_state_(local_state) {}

  ChromeEnabledStateProvider(const ChromeEnabledStateProvider&) = delete;
  ChromeEnabledStateProvider& operator=(const ChromeEnabledStateProvider&) =
      delete;

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
  const raw_ptr<PrefService> local_state_;
};

ChromeMetricsServicesManagerClient::ChromeMetricsServicesManagerClient(
    PrefService* local_state)
    : enabled_state_provider_(
          std::make_unique<ChromeEnabledStateProvider>(local_state)),
      local_state_(local_state) {
  DCHECK(local_state);
}

ChromeMetricsServicesManagerClient::~ChromeMetricsServicesManagerClient() {}

metrics::MetricsStateManager*
ChromeMetricsServicesManagerClient::GetMetricsStateManagerForTesting() {
  return GetMetricsStateManager();
}

// static
bool ChromeMetricsServicesManagerClient::IsClientInSampleForMetrics() {
  return IsClientInSampleImpl(g_browser_process->local_state());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// static
bool ChromeMetricsServicesManagerClient::IsClientInSampleForCrashes() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, there are two field trials that, together, drive metrics and
  // crash reporting. The determination of which trial to use is based on
  // whether the client went through the FRE before or after the fix to
  // crbug.com/1306481 was deployed.
  //
  // The PostFREFixSamplingTrial controls crash and metrics sampling for clients
  // which went through the FRE after the FRE fix was deployed. These clients
  // use the PostFREFixMetricsReortingFeature and its "disable_crashes" feature
  // parameter to control whether the client is in-sample for crash reporting.
  if (ShouldUsePostFREFixSamplingTrial(g_browser_process->local_state())) {
    // If reporting isn't enabled at all, then we can return early.
    if (!base::FeatureList::IsEnabled(
            metrics::internal::kPostFREFixMetricsReportingFeature)) {
      return false;
    }
    // Otherwise, send crashes if crash reporting is NOT disabled. By default
    // crash reporting is not disabled.
    const bool crashes_are_disabled = base::GetFieldTrialParamByFeatureAsBool(
        metrics::internal::kPostFREFixMetricsReportingFeature,
        "disable_crashes", false);
    return !crashes_are_disabled;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  // If this is a Windows client, or if this is an Android client that went
  // through the FRE before the FRE fix was deployed, then this client uses
  // the MetricsReportingFeature and its "disable_crashes" parameter to control
  // whether the client is in-sample for crash reporting.

  // If reporting isn't enabled at all, then we can return early.
  if (!base::FeatureList::IsEnabled(
          metrics::internal::kMetricsReportingFeature)) {
    return false;
  }

  const bool crashes_are_disabled = base::GetFieldTrialParamByFeatureAsBool(
      metrics::internal::kMetricsReportingFeature, "disable_crashes", false);
  return !crashes_are_disabled;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

// static
bool ChromeMetricsServicesManagerClient::GetSamplingRatePerMille(int* rate) {
#if BUILDFLAG(IS_ANDROID)
  const base::Feature& feature =
      ShouldUsePostFREFixSamplingTrial()
          ? metrics::internal::kPostFREFixMetricsReportingFeature
          : metrics::internal::kMetricsReportingFeature;
#else
  const base::Feature& feature = metrics::internal::kMetricsReportingFeature;
#endif  // BUILDFLAG(IS_ANDROID)
  std::string rate_str = base::GetFieldTrialParamValueByFeature(
      feature, metrics::internal::kRateParamName);
  if (rate_str.empty())
    return false;

  if (!base::StringToInt(rate_str, rate) || *rate > 1000)
    return false;

  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeMetricsServicesManagerClient::OnCrosSettingsCreated() {
  // Listen for changes to metrics reporting state.
  reporting_setting_subscription_ =
      ash::StatsReportingController::Get()->AddObserver(base::BindRepeating(
          &OnCrosMetricsReportingSettingChange,
          ChangeMetricsReportingStateCalledFrom::kCrosMetricsSettingsChange));
  // Invoke the callback once initially to set the metrics reporting state.
  OnCrosMetricsReportingSettingChange(
      ChangeMetricsReportingStateCalledFrom::kCrosMetricsSettingsCreated);
}
#endif

std::unique_ptr<variations::VariationsService>
ChromeMetricsServicesManagerClient::CreateVariationsService(
    variations::SyntheticTrialRegistry* synthetic_trial_registry) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return variations::VariationsService::Create(
      std::make_unique<ChromeVariationsServiceClient>(), local_state_,
      GetMetricsStateManager(), switches::kDisableBackgroundNetworking,
      chrome_variations::CreateUIStringOverrider(),
      base::BindOnce(&content::GetNetworkConnectionTracker),
      synthetic_trial_registry);
}

std::unique_ptr<metrics::MetricsServiceClient>
ChromeMetricsServicesManagerClient::CreateMetricsServiceClient(
    variations::SyntheticTrialRegistry* synthetic_trial_registry) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return ChromeMetricsServiceClient::Create(GetMetricsStateManager(),
                                            synthetic_trial_registry);
}

metrics::MetricsStateManager*
ChromeMetricsServicesManagerClient::GetMetricsStateManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!metrics_state_manager_) {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    metrics::StartupVisibility startup_visibility;
#if BUILDFLAG(IS_ANDROID)
    startup_visibility = UmaSessionStats::HasVisibleActivity()
                             ? metrics::StartupVisibility::kForeground
                             : metrics::StartupVisibility::kBackground;
    base::UmaHistogramEnumeration("UMA.StartupVisibility", startup_visibility);
#else
    startup_visibility = metrics::StartupVisibility::kForeground;
#endif  // BUILDFLAG(IS_ANDROID)

    std::string client_id;
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_, enabled_state_provider_.get(), GetRegistryBackupKey(),
        user_data_dir, startup_visibility,
        {
            .default_entropy_provider_type =
                metrics::EntropyProviderType::kDefault,
            .force_benchmarking_mode =
                base::CommandLine::ForCurrentProcess()->HasSwitch(
                    cc::switches::kEnableGpuBenchmarking),
        },
        base::BindRepeating(&PostStoreMetricsClientInfo),
        base::BindRepeating(&GoogleUpdateSettings::LoadMetricsClientInfo),
        client_id);
  }
  return metrics_state_manager_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeMetricsServicesManagerClient::GetURLLoaderFactory() {
  return g_browser_process->system_network_context_manager()
      ->GetSharedURLLoaderFactory();
}

const metrics::EnabledStateProvider&
ChromeMetricsServicesManagerClient::GetEnabledStateProvider() {
  return *enabled_state_provider_;
}

bool ChromeMetricsServicesManagerClient::IsOffTheRecordSessionActive() {
#if BUILDFLAG(IS_ANDROID)
  // This differs from TabModelList::IsOffTheRecordSessionActive in that it
  // does not ignore TabModels that have no open tabs, because it may be checked
  // before tabs get added to the TabModel. This means it may be more
  // conservative in case unused TabModels are not cleaned up, but it seems to
  // work correctly.
  // TODO(crbug.com/40529753): Check if TabModelList's version can be updated
  // safely.
  // TODO(crbug.com/40107157): This function should return true for Incognito
  // CCTs.
  for (const TabModel* model : TabModelList::models()) {
    if (model->IsOffTheRecord())
      return true;
  }

  return false;
#else
  // Depending directly on BrowserList, since that is the implementation
  // that we get correct notifications for.
  return BrowserList::IsOffTheRecordBrowserActive();
#endif
}

#if BUILDFLAG(IS_WIN)
void ChromeMetricsServicesManagerClient::UpdateRunningServices(
    bool may_record,
    bool may_upload) {
  // First, set the registry value so that Crashpad will have the sampling state
  // now and for subsequent runs. Note that Crashpad uses *both* the registry
  // value and the value sent from SetUploadConsent below.
  // We use IsClientInSampleForCrash() which checks the feature for if crashes
  // are allowed.
  install_static::SetCollectStatsInSample(IsClientInSampleForCrashes());

  // The intent here is to set the value of the consent. However, since right
  // now we have may_record which is based off both consent and the Feature
  // state, this is redundant with the above value. This is pretty confusing
  // right now, and we may want to rethink this. One extra complexity here is we
  // currently check the disable_crashes parameter, which does not go
  // into may_record. This is because this is specifically intending to test for
  // consent, and as mentioned, on the crashpad side we check both. See
  // SetUploadConsent() in components/crash/core/app/crashpad.cc for how this
  // gets used.
  SetUploadConsent_ExportThunk(may_record && may_upload);
}
#endif  // BUILDFLAG(IS_WIN)
