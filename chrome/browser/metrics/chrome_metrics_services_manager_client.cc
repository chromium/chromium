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
#include "components/variations/service/variations_service.h"
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
#include "components/metrics/structured/neutrino_logging.h"       // nogncheck
#include "components/metrics/structured/neutrino_logging_util.h"  // nogncheck
#include "components/metrics/structured/recorder.h"               // nogncheck
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

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
  // TODO(crbug/1321823): Fix the unit tests so that we do not need to check for
  // g_browser_process and local_state().
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

  // TODO(crbug.com/1234538): This call ensures that structured metrics' state
  // is deleted when the reporting state is disabled. Long-term this should
  // happen via a call to all MetricsProviders eg. OnClientStateCleared. This is
  // temporarily called here because it is close to the settings UI, and doesn't
  // greatly affect the logging in crbug.com/1227585.
  auto* recorder = metrics::structured::Recorder::GetInstance();
  if (recorder) {
    recorder->OnReportingStateChanged(enable_metrics);
  }
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
bool ChromeMetricsServicesManagerClient::IsClientInSample() {
  return IsClientInSampleImpl(g_browser_process->local_state());
}

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

const metrics::EnabledStateProvider&
ChromeMetricsServicesManagerClient::GetEnabledStateProviderForTesting() {
  return *enabled_state_provider_;
}

std::unique_ptr<variations::VariationsService>
ChromeMetricsServicesManagerClient::CreateVariationsService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  metrics::structured::NeutrinoDevicesLogWithLocalState(
      local_state_,
      metrics::structured::NeutrinoDevicesLocation::kCreateVariationsService);
#endif
  return variations::VariationsService::Create(
      std::make_unique<ChromeVariationsServiceClient>(), local_state_,
      GetMetricsStateManager(), switches::kDisableBackgroundNetworking,
      chrome_variations::CreateUIStringOverrider(),
      base::BindOnce(&content::GetNetworkConnectionTracker));
}

std::unique_ptr<metrics::MetricsServiceClient>
ChromeMetricsServicesManagerClient::CreateMetricsServiceClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  metrics::structured::NeutrinoDevicesLogWithLocalState(
      local_state_, metrics::structured::NeutrinoDevicesLocation::
                        kCreateMetricsServiceClient);
#endif
  return ChromeMetricsServiceClient::Create(GetMetricsStateManager());
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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Read metrics service client id from ash chrome if it's present.
    auto* init_params = chromeos::BrowserParamsProxy::Get();
    if (init_params->MetricsServiceClientId().has_value())
      client_id = init_params->MetricsServiceClientId().value();
#endif

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

bool ChromeMetricsServicesManagerClient::IsMetricsReportingEnabled() {
  return enabled_state_provider_->IsReportingEnabled();
}

bool ChromeMetricsServicesManagerClient::IsMetricsConsentGiven() {
  return enabled_state_provider_->IsConsentGiven();
}

bool ChromeMetricsServicesManagerClient::IsOffTheRecordSessionActive() {
#if BUILDFLAG(IS_ANDROID)
  // This differs from TabModelList::IsOffTheRecordSessionActive in that it
  // does not ignore TabModels that have no open tabs, because it may be checked
  // before tabs get added to the TabModel. This means it may be more
  // conservative in case unused TabModels are not cleaned up, but it seems to
  // work correctly.
  // TODO(crbug/741888): Check if TabModelList's version can be updated safely.
  // TODO(crbug/1023759): This function should return true for Incognito CCTs.
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
  // now and for subsequent runs.
  install_static::SetCollectStatsInSample(IsClientInSample());

  // Next, get Crashpad to pick up the sampling state for this session.
  // Crashpad will use the kRegUsageStatsInSample registry value to apply
  // sampling correctly, but may_record already reflects the sampling state.
  // This isn't a problem though, since they will be consistent.
  SetUploadConsent_ExportThunk(may_record && may_upload);
}
#endif  // BUILDFLAG(IS_WIN)
