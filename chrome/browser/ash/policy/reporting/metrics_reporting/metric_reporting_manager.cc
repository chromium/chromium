// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_platform_metrics_retriever.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_telemetry_periodic_collector.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_telemetry_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/audio/audio_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_audio_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_boot_performance_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_bus_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_cpu_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_display_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_input_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_memory_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_psr_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/device_activity/device_activity_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_event_detector.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_info_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_telemetry_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/usb/usb_events_observer.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/network/network_bandwidth_sampler.h"
#include "chrome/browser/chromeos/reporting/user_reporting_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/delayed_sampler.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/metric_event_observer_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/periodic_event_collector.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/rate_limiter_slide_window.h"
#include "components/user_manager/user.h"

namespace em = enterprise_management;

namespace reporting {
namespace {

constexpr char kAppTelemetry[] = "app_telemetry";
constexpr char kAudioTelemetry[] = "audio_telemetry";
constexpr char kBootPerformance[] = "boot_performance";
constexpr char kHttpsLatency[] = "https_latency";
constexpr char kNetworkTelemetry[] = "network_telemetry";
constexpr char kPeripheralTelemetry[] = "peripheral_telemetry";
constexpr char kPsrTelemetry[] = "psr_telemetry";
constexpr char kDelayedPeripheralTelemetry[] = "delayed_peripheral_telemetry";
constexpr char kDisplaysTelemetry[] = "displays_telemetry";
constexpr char kDeviceActivityTelemetry[] = "device_activity_telemetry";

// App event rate limiter configuration.
constexpr size_t kAppEventsTotalSize = 4096u /**bytes**/ * 1024;
constexpr base::TimeDelta kAppEventsWindow = base::Seconds(10);
constexpr size_t kAppEventsBucketCount = 10;

}  // namespace

// static
BASE_FEATURE(kEnableAppEventsObserver,
             "EnableAppEventsObserver",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableFatalCrashEventsObserver,
             "EnableFatalCrashEventsObserver",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kEnableRuntimeCounters,
             "EnableRuntimeCounters",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool MetricReportingManager::Delegate::IsUserAffiliated(
    Profile& profile) const {
  const user_manager::User* const user =
      ::ash::ProfileHelper::Get()->GetUserByProfile(&profile);
  return user && user->IsAffiliated();
}

bool MetricReportingManager::Delegate::IsDeprovisioned() const {
  return ::ash::DeviceSettingsService::IsInitialized() &&
         ::ash::DeviceSettingsService::Get()->policy_data() &&
         ::ash::DeviceSettingsService::Get()->policy_data()->state() ==
             em::PolicyData::DEPROVISIONED;
}

std::unique_ptr<Sampler>
MetricReportingManager::Delegate::GetHttpsLatencySampler() const {
  return std::make_unique<HttpsLatencySampler>();
}

std::unique_ptr<Sampler>
MetricReportingManager::Delegate::GetNetworkTelemetrySampler() const {
  return std::make_unique<NetworkTelemetrySampler>();
}

bool MetricReportingManager::Delegate::IsAppServiceAvailableForProfile(
    Profile* profile) const {
  return ::apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
      profile);
}

// static
std::unique_ptr<MetricReportingManager> MetricReportingManager::Create(
    policy::ManagedSessionService* managed_session_service) {
  return base::WrapUnique(new MetricReportingManager(
      std::make_unique<Delegate>(), managed_session_service));
}

// static
std::unique_ptr<MetricReportingManager>
MetricReportingManager::CreateForTesting(
    std::unique_ptr<Delegate> delegate,
    policy::ManagedSessionService* managed_session_service) {
  return base::WrapUnique(
      new MetricReportingManager(std::move(delegate), managed_session_service));
}

MetricReportingManager::~MetricReportingManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Shutdown();
}

void MetricReportingManager::OnLogin(Profile* profile) {
  managed_session_observation_.Reset();
  CHECK_NE(profile, nullptr);
  if (!delegate_->IsUserAffiliated(*profile)) {
    return;
  }

  // Create user metric report queues here since they depend on the user
  // profile only available after login. These should rely on the
  // `telemetry_report_queue_` for periodic uploads to avoid overlapping flush
  // operations.
  SourceInfo source_info;
  source_info.set_source(SourceInfo::ASH);
  user_telemetry_report_queue_ = delegate_->CreateMetricReportQueue(
      EventType::kUser, Destination::TELEMETRY_METRIC, Priority::MANUAL_BATCH,
      /*rate_limiter=*/nullptr, source_info);
  user_event_report_queue_ = delegate_->CreateMetricReportQueue(
      EventType::kUser, Destination::EVENT_METRIC, Priority::SLOW_BATCH,
      /*rate_limiter=*/nullptr, source_info);

  auto app_event_rate_limiter = std::make_unique<RateLimiterSlideWindow>(
      kAppEventsTotalSize, kAppEventsWindow, kAppEventsBucketCount);
  app_event_report_queue_ = delegate_->CreateMetricReportQueue(
      EventType::kUser, Destination::EVENT_METRIC, Priority::SLOW_BATCH,
      std::move(app_event_rate_limiter), source_info);
  user_peripheral_events_and_telemetry_report_queue_ =
      delegate_->CreateMetricReportQueue(
          EventType::kUser, Destination::PERIPHERAL_EVENTS, Priority::SECURITY,
          /*rate_limiter=*/nullptr, std::move(source_info));

  DCHECK(profile);
  user_reporting_settings_ =
      std::make_unique<UserReportingSettings>(profile->GetWeakPtr());

  InitOnAffiliatedLogin(profile);
  DelayedInitOnAffiliatedLogin(profile);
}

void MetricReportingManager::DeviceSettingsUpdated() {
  if (delegate_->IsDeprovisioned()) {
    Shutdown();
  }
}

std::vector<CollectorBase*> MetricReportingManager::GetTelemetryCollectors(
    MetricEventType event_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (event_type) {
    case WIFI_SIGNAL_STRENGTH_LOW:
    case WIFI_SIGNAL_STRENGTH_RECOVERED:
      return GetTelemetryCollectorsFromSetting(
          ::ash::kReportDeviceSignalStrengthEventDrivenTelemetry);
    case USB_ADDED:
    case USB_REMOVED:
      if (base::Contains(telemetry_collectors_, kDelayedPeripheralTelemetry)) {
        return {telemetry_collectors_.at(kDelayedPeripheralTelemetry).get()};
      }
      // Return statement or `ABSL_FALLTHROUGH_INTENDED` is necessary to silence
      // "unannotated fall-through" errors on builds.
      return {};
    default:
      return {};
  }
}

MetricReportingManager::MetricReportingManager(
    std::unique_ptr<Delegate> delegate,
    policy::ManagedSessionService* managed_session_service)
    : delegate_(std::move(delegate)) {
  if (delegate_->IsDeprovisioned()) {
    return;
  }

  SourceInfo source_info;
  source_info.set_source(SourceInfo::ASH);
  info_report_queue_ = delegate_->CreateMetricReportQueue(
      EventType::kDevice, Destination::INFO_METRIC, Priority::SLOW_BATCH,
      /*rate_limiter=*/nullptr, source_info);
  telemetry_report_queue_ = delegate_->CreatePeriodicUploadReportQueue(
      EventType::kDevice, Destination::TELEMETRY_METRIC, Priority::MANUAL_BATCH,
      &reporting_settings_, ::ash::kReportUploadFrequency,
      metrics::GetDefaultReportUploadFrequency(), /*rate_unit_to_ms=*/1,
      source_info);
  event_report_queue_ = delegate_->CreateMetricReportQueue(
      EventType::kDevice, Destination::EVENT_METRIC, Priority::SLOW_BATCH,
      /*rate_limiter=*/nullptr, std::move(source_info));
  DelayedInit();

  if (managed_session_service) {
    managed_session_observation_.Observe(managed_session_service);
  }
  if (::ash::DeviceSettingsService::IsInitialized()) {
    device_settings_observation_.Observe(::ash::DeviceSettingsService::Get());
  }
}

void MetricReportingManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  app_usage_observer_.reset();
  delegate_.reset();
  event_observer_managers_.clear();
  info_collectors_.clear();
  telemetry_collectors_.clear();
  network_bandwidth_collector_.reset();
  samplers_.clear();
  info_report_queue_.reset();
  telemetry_report_queue_.reset();
  user_telemetry_report_queue_.reset();
  event_report_queue_.reset();
  user_event_report_queue_.reset();
  app_event_report_queue_.reset();
  user_peripheral_events_and_telemetry_report_queue_.reset();
  user_reporting_settings_.reset();
}

void MetricReportingManager::DelayedInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegate_->IsDeprovisioned()) {
    return;
  }
  // Info collectors init is delayed by default.
  CreateCrosHealthdInfoCollector(
      std::make_unique<CrosHealthdCpuSamplerHandler>(),
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kCpu,
      ::ash::kReportDeviceCpuInfo,
      /*default_value=*/false);
  CreateCrosHealthdInfoCollector(
      std::make_unique<CrosHealthdMemorySamplerHandler>(),
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kMemory,
      ::ash::kReportDeviceMemoryInfo,
      /*default_value=*/false);
  CreateCrosHealthdInfoCollector(
      std::make_unique<CrosHealthdBusSamplerHandler>(
          CrosHealthdSamplerHandler::MetricType::kInfo),
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kBus,
      ::ash::kReportDeviceSecurityStatus,
      /*default_value=*/false);
  CreateCrosHealthdInfoCollector(
      std::make_unique<CrosHealthdInputSamplerHandler>(),
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kInput,
      ::ash::kReportDeviceGraphicsStatus,
      /*default_value=*/false);
  CreateCrosHealthdInfoCollector(
      std::make_unique<CrosHealthdDisplaySamplerHandler>(
          CrosHealthdSamplerHandler::MetricType::kInfo),
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kDisplay,
      ::ash::kReportDeviceGraphicsStatus,
      /*default_value=*/false);

  // Network health info.
  // ReportDeviceNetworkConfiguration policy is enabled by default, so set its
  // default value to true.
  InitInfoCollector(
      std::make_unique<NetworkInfoSampler>(),
      /*enable_setting_path=*/::ash::kReportDeviceNetworkConfiguration,
      /*setting_enabled_default_value=*/true);

  InitBootPerformanceCollector();
  InitRuntimeCountersCollectors();
  InitFatalCrashCollectors();

  initial_upload_timer_.Start(FROM_HERE, GetUploadDelay(), this,
                              &MetricReportingManager::UploadTelemetry);
}

void MetricReportingManager::InitOnAffiliatedLogin(Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (delegate_->IsDeprovisioned()) {
    return;
  }

  InitEventObserverManager(
      std::make_unique<AudioEventsObserver>(), user_event_report_queue_.get(),
      &reporting_settings_,
      /*enable_setting_path=*/::ash::kReportDeviceAudioStatus,
      metrics::kReportDeviceAudioStatusDefaultValue,
      /*init_delay=*/base::TimeDelta());
  // Network health events observer.
  InitEventObserverManager(
      std::make_unique<NetworkEventsObserver>(), event_report_queue_.get(),
      &reporting_settings_,
      /*enable_setting_path=*/::ash::kDeviceReportNetworkEvents,
      metrics::kDeviceReportNetworkEventsDefaultValue,
      /*init_delay=*/base::TimeDelta());
  InitPeripheralsCollectors();

  // Start observing app events and telemetry only if the app service is
  // available for the given profile.
  if (delegate_->IsAppServiceAvailableForProfile(profile)) {
    InitAppCollectors(profile);
  }
}

void MetricReportingManager::DelayedInitOnAffiliatedLogin(Profile* profile) {
  if (delegate_->IsDeprovisioned()) {
    return;
  }
  InitNetworkCollectors(profile);
  InitAudioCollectors();
  InitDisplayCollectors();
  InitDeviceActivityCollector();

  initial_upload_timer_.Start(FROM_HERE, GetUploadDelay(), this,
                              &MetricReportingManager::UploadTelemetry);
}

void MetricReportingManager::InitInfoCollector(
    std::unique_ptr<Sampler> sampler,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!info_report_queue_) {
    return;
  }

  info_collectors_.push_back(delegate_->CreateOneShotCollector(
      sampler.get(), info_report_queue_.get(), &reporting_settings_,
      enable_setting_path, setting_enabled_default_value,
      delegate_->GetInitDelay()));
  samplers_.push_back(std::move(sampler));
}

void MetricReportingManager::InitOneShotTelemetryCollector(
    const std::string& collector_name,
    Sampler* sampler,
    MetricReportQueue* metric_report_queue,
    const std::string& enable_setting_path,
    bool enable_default_value,
    base::TimeDelta init_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(telemetry_collectors_, collector_name));
  if (!metric_report_queue) {
    return;
  }

  auto collector = delegate_->CreateOneShotCollector(
      sampler, metric_report_queue, &reporting_settings_, enable_setting_path,
      enable_default_value, init_delay);
  telemetry_collectors_.insert({collector_name, std::move(collector)});
}

void MetricReportingManager::InitManualTelemetryCollector(
    const std::string& collector_name,
    Sampler* sampler,
    MetricReportQueue* metric_report_queue,
    const std::string& enable_setting_path,
    bool enable_default_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(telemetry_collectors_, collector_name));
  if (!metric_report_queue) {
    return;
  }

  auto collector = delegate_->CreateManualCollector(
      sampler, metric_report_queue, &reporting_settings_, enable_setting_path,
      enable_default_value);
  telemetry_collectors_.insert({collector_name, std::move(collector)});
}

void MetricReportingManager::InitPeriodicTelemetryCollector(
    const std::string& collector_name,
    Sampler* sampler,
    MetricReportQueue* metric_report_queue,
    const std::string& enable_setting_path,
    bool enable_default_value,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms,
    base::TimeDelta init_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(telemetry_collectors_, collector_name));
  if (!metric_report_queue) {
    return;
  }

  auto collector = delegate_->CreatePeriodicCollector(
      sampler, metric_report_queue, &reporting_settings_, enable_setting_path,
      enable_default_value, rate_setting_path, default_rate, rate_unit_to_ms,
      init_delay);
  telemetry_collectors_.insert({collector_name, std::move(collector)});
}

void MetricReportingManager::InitPeriodicEventCollector(
    Sampler* sampler,
    std::unique_ptr<PeriodicEventCollector::EventDetector> event_detector,
    MetricReportQueue* metric_report_queue,
    const std::string& enable_setting_path,
    bool enable_default_value,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms,
    base::TimeDelta init_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!metric_report_queue) {
    return;
  }

  auto periodic_event_collector = std::make_unique<PeriodicEventCollector>(
      sampler, std::move(event_detector), &reporting_settings_,
      rate_setting_path, default_rate, rate_unit_to_ms);
  InitEventObserverManager(std::move(periodic_event_collector),
                           metric_report_queue, &reporting_settings_,
                           enable_setting_path, enable_default_value,
                           init_delay);
}

void MetricReportingManager::InitEventObserverManager(
    std::unique_ptr<MetricEventObserver> event_observer,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value,
    base::TimeDelta init_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!metric_report_queue) {
    return;
  }
  event_observer_managers_.emplace_back(delegate_->CreateEventObserverManager(
      std::move(event_observer), metric_report_queue, reporting_settings,
      enable_setting_path, setting_enabled_default_value,
      /*collector_pool=*/this, init_delay));
}

void MetricReportingManager::UploadTelemetry() {
  if (!telemetry_report_queue_) {
    return;
  }
  telemetry_report_queue_->Upload();
}

void MetricReportingManager::CreateCrosHealthdInfoCollector(
    std::unique_ptr<CrosHealthdSamplerHandler> info_handler,
    ::ash::cros_healthd::mojom::ProbeCategoryEnum probe_category,
    const std::string& setting_path,
    bool default_value) {
  auto croshealthd_sampler = std::make_unique<CrosHealthdMetricSampler>(
      std::move(info_handler), probe_category);
  InitInfoCollector(std::move(croshealthd_sampler), setting_path,
                    default_value);
}

void MetricReportingManager::InitNetworkCollectors(Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Network health telemetry.
  InitNetworkPeriodicCollector(kNetworkTelemetry,
                               delegate_->GetNetworkTelemetrySampler());

  // HttpsLatency collectors.
  auto https_latency_sampler = delegate_->GetHttpsLatencySampler();
  // HttpsLatency events.
  InitPeriodicEventCollector(
      https_latency_sampler.get(),
      std::make_unique<HttpsLatencyEventDetector>(), event_report_queue_.get(),
      /*enable_setting_path=*/::ash::kReportDeviceNetworkStatus,
      metrics::kReportDeviceNetworkStatusDefaultValue,
      ::ash::kReportDeviceNetworkTelemetryEventCheckingRateMs,
      metrics::GetDefaultEventCheckingRate(
          metrics::kDefaultNetworkTelemetryEventCheckingRate),
      /*rate_unit_to_ms=*/1, delegate_->GetInitDelay());
  // HttpsLatency telemetry.
  InitNetworkPeriodicCollector(kHttpsLatency, std::move(https_latency_sampler));

  // Network bandwidth telemetry.
  auto network_bandwidth_sampler = std::make_unique<NetworkBandwidthSampler>(
      g_browser_process->network_quality_tracker(), profile->GetWeakPtr());
  network_bandwidth_collector_ = delegate_->CreatePeriodicCollector(
      network_bandwidth_sampler.get(), user_telemetry_report_queue_.get(),
      &reporting_settings_,
      /*enable_setting_path=*/
      ::ash::kReportDeviceNetworkStatus,
      metrics::kReportDeviceNetworkStatusDefaultValue,
      ::ash::kReportDeviceNetworkTelemetryCollectionRateMs,
      metrics::GetDefaultCollectionRate(
          metrics::kDefaultNetworkTelemetryCollectionRate),
      /*rate_unit_to_ms=*/1, delegate_->GetInitDelay());
  samplers_.push_back(std::move(network_bandwidth_sampler));
}

void MetricReportingManager::InitNetworkPeriodicCollector(
    const std::string& collector_name,
    std::unique_ptr<Sampler> sampler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InitPeriodicTelemetryCollector(
      collector_name, sampler.get(), telemetry_report_queue_.get(),
      /*enable_setting_path=*/::ash::kReportDeviceNetworkStatus,
      metrics::kReportDeviceNetworkStatusDefaultValue,
      ::ash::kReportDeviceNetworkTelemetryCollectionRateMs,
      metrics::GetDefaultCollectionRate(
          metrics::kDefaultNetworkTelemetryCollectionRate),
      /*rate_unit_to_ms=*/1, delegate_->GetInitDelay());
  samplers_.push_back(std::move(sampler));
}

void MetricReportingManager::InitAppCollectors(Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(telemetry_collectors_, kAppTelemetry));
  DCHECK(user_event_report_queue_);
  DCHECK(user_reporting_settings_);
  DCHECK(user_telemetry_report_queue_);
  // App events.
  if (base::FeatureList::IsEnabled(kEnableAppEventsObserver)) {
    auto app_events_observer = AppEventsObserver::CreateForProfile(
        profile, user_reporting_settings_.get());
    InitEventObserverManager(
        std::move(app_events_observer), app_event_report_queue_.get(),
        user_reporting_settings_.get(),
        /*enable_setting_path=*/::ash::reporting::kReportAppInventory,
        metrics::kReportAppInventoryEnabledDefaultValue,
        /*init_delay=*/base::TimeDelta());
  }

  // App telemetry.
  app_usage_observer_ =
      AppUsageObserver::Create(profile, user_reporting_settings_.get());
  auto app_usage_telemetry_sampler =
      std::make_unique<AppUsageTelemetrySampler>(profile->GetWeakPtr());
  auto app_usage_telemetry_collector =
      std::make_unique<AppUsageTelemetryPeriodicCollector>(
          app_usage_telemetry_sampler.get(), user_telemetry_report_queue_.get(),
          user_reporting_settings_.get());
  samplers_.push_back(std::move(app_usage_telemetry_sampler));
  telemetry_collectors_.insert(
      {kAppTelemetry, std::move(app_usage_telemetry_collector)});
}

void MetricReportingManager::InitAudioCollectors() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto audio_telemetry_handler =
      std::make_unique<CrosHealthdAudioSamplerHandler>();
  auto audio_telemetry_sampler = std::make_unique<CrosHealthdMetricSampler>(
      std::move(audio_telemetry_handler),
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kAudio);
  InitPeriodicTelemetryCollector(
      kAudioTelemetry, audio_telemetry_sampler.get(),
      user_telemetry_report_queue_.get(),
      /*enable_setting_path=*/::ash::kReportDeviceAudioStatus,
      metrics::kReportDeviceAudioStatusDefaultValue,
      ::ash::kReportDeviceAudioStatusCheckingRateMs,
      metrics::GetDefaultCollectionRate(
          metrics::kDefaultAudioTelemetryCollectionRate),
      /*rate_unit_to_ms=*/1, delegate_->GetInitDelay());
  samplers_.push_back(std::move(audio_telemetry_sampler));
}

void MetricReportingManager::InitBootPerformanceCollector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto boot_performance_handler =
      std::make_unique<CrosHealthdBootPerformanceSamplerHandler>();
  auto boot_performance_sampler = std::make_unique<CrosHealthdMetricSampler>(
      std::move(boot_performance_handler),
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kBootPerformance);
  InitOneShotTelemetryCollector(
      /*collector_name=*/kBootPerformance, boot_performance_sampler.get(),
      telemetry_report_queue_.get(),
      /*enable_setting_path=*/::ash::kReportDeviceBootMode,
      /*enable_default_value=*/true, delegate_->GetInitDelay());
  samplers_.push_back(std::move(boot_performance_sampler));
}

void MetricReportingManager::InitRuntimeCountersCollectors() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::FeatureList::IsEnabled(kEnableRuntimeCounters)) {
    auto psr_telemetry_handler =
        std::make_unique<CrosHealthdPsrSamplerHandler>();
    auto psr_telemetry_sampler = std::make_unique<CrosHealthdMetricSampler>(
        std::move(psr_telemetry_handler),
        ::ash::cros_healthd::mojom::ProbeCategoryEnum::kSystem);
    InitPeriodicTelemetryCollector(
        kPsrTelemetry, psr_telemetry_sampler.get(),
        telemetry_report_queue_.get(),
        /*enable_setting_path=*/::ash::kDeviceReportRuntimeCounters,
        metrics::kDeviceReportRuntimeCountersDefaultValue,
        /*rate_setting_path=*/::ash::kDeviceReportRuntimeCountersCheckingRateMs,
        metrics::GetDefaultCollectionRate(
            metrics::kDefaultRuntimeCountersTelemetryCollectionRate),
        /*rate_unit_to_ms=*/1, delegate_->GetInitDelay());
    samplers_.push_back(std::move(psr_telemetry_sampler));
  }
}

void MetricReportingManager::InitFatalCrashCollectors() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (base::FeatureList::IsEnabled(kEnableFatalCrashEventsObserver)) {
    event_observer_managers_.emplace_back(delegate_->CreateEventObserverManager(
        std::make_unique<FatalCrashEventsObserver>(),
        telemetry_report_queue_.get(), &reporting_settings_,
        ash::kReportDeviceCrashReportInfo,
        metrics::kReportDeviceCrashReportInfoDefaultValue,
        /*collector_pool=*/this));
  }
}

void MetricReportingManager::InitPeripheralsCollectors() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!user_peripheral_events_and_telemetry_report_queue_) {
    return;
  }
  // Peripheral events
  event_observer_managers_.emplace_back(delegate_->CreateEventObserverManager(
      std::make_unique<UsbEventsObserver>(),
      user_peripheral_events_and_telemetry_report_queue_.get(),
      &reporting_settings_, ::ash::kReportDevicePeripherals,
      metrics::kReportDevicePeripheralsDefaultValue,
      /*collector_pool=*/this));

  // Peripheral telemetry
  auto peripheral_telemetry_sampler =
      /*sampler=*/std::make_unique<CrosHealthdMetricSampler>(
          /*handler=*/std::make_unique<CrosHealthdBusSamplerHandler>(
              CrosHealthdSamplerHandler::MetricType::kTelemetry),
          ::ash::cros_healthd::mojom::ProbeCategoryEnum::kBus);

  InitOneShotTelemetryCollector(
      kPeripheralTelemetry, peripheral_telemetry_sampler.get(),
      user_peripheral_events_and_telemetry_report_queue_.get(),
      /*enable_setting_path=*/::ash::kReportDevicePeripherals,
      metrics::kReportDevicePeripheralsDefaultValue,
      /*init_delay=*/base::TimeDelta());
  samplers_.push_back(std::move(peripheral_telemetry_sampler));

  // Event-driven peripheral telemetry.
  // We add a short delay to mitigate a
  // race condition that occurs when cros healthd queries the firmware updater
  // tool (fwupd) for USB firmware version data before fwupd has a chance to
  // read all of the USB devices plugged into the machine.
  auto delayed_peripheral_telemetry_sampler = std::make_unique<DelayedSampler>(
      /*sampler=*/std::make_unique<CrosHealthdMetricSampler>(
          /*handler=*/std::make_unique<CrosHealthdBusSamplerHandler>(
              CrosHealthdSamplerHandler::MetricType::kTelemetry),
          ::ash::cros_healthd::mojom::ProbeCategoryEnum::kBus),
      /*delay=*/metrics::kPeripheralCollectionDelay);

  InitManualTelemetryCollector(
      kDelayedPeripheralTelemetry, delayed_peripheral_telemetry_sampler.get(),
      user_peripheral_events_and_telemetry_report_queue_.get(),
      /*enable_setting_path=*/::ash::kReportDevicePeripherals,
      metrics::kReportDevicePeripheralsDefaultValue);

  samplers_.push_back(std::move(delayed_peripheral_telemetry_sampler));
}

void MetricReportingManager::InitDisplayCollectors() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto displays_telemetry_handler =
      std::make_unique<CrosHealthdDisplaySamplerHandler>(
          CrosHealthdSamplerHandler::MetricType::kTelemetry);
  auto displays_telemetry_sampler = std::make_unique<CrosHealthdMetricSampler>(
      std::move(displays_telemetry_handler),
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kDisplay);
  InitPeriodicTelemetryCollector(
      kDisplaysTelemetry, displays_telemetry_sampler.get(),
      telemetry_report_queue_.get(),
      /*enable_setting_path=*/::ash::kReportDeviceGraphicsStatus,
      metrics::kReportDeviceGraphicsStatusDefaultValue,
      ::ash::kReportUploadFrequency,
      metrics::GetDefaultCollectionRate(metrics::kDefaultReportUploadFrequency),
      /*rate_unit_to_ms=*/1, delegate_->GetInitDelay());
  samplers_.push_back(std::move(displays_telemetry_sampler));
}

void MetricReportingManager::InitDeviceActivityCollector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto device_activity_sampler = std::make_unique<DeviceActivitySampler>();
  InitPeriodicTelemetryCollector(
      kDeviceActivityTelemetry, device_activity_sampler.get(),
      user_telemetry_report_queue_.get(),
      /*enable_setting_path=*/::ash::kDeviceActivityHeartbeatEnabled,
      metrics::kDeviceActivityHeartbeatEnabledDefaultValue,
      ::ash::kDeviceActivityHeartbeatCollectionRateMs,
      metrics::GetDefaultCollectionRate(
          metrics::kDefaultDeviceActivityHeartbeatCollectionRate),
      /*rate_unit_to_ms=*/1, delegate_->GetInitDelay());
  samplers_.push_back(std::move(device_activity_sampler));
}

std::vector<CollectorBase*>
MetricReportingManager::GetTelemetryCollectorsFromSetting(
    std::string_view setting_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value::List* telemetry_list = nullptr;
  const bool valid = ::ash::CrosSettings::Get()->GetList(
      std::string(setting_name), &telemetry_list);
  if (!valid || !telemetry_list) {
    return {};
  }

  std::vector<CollectorBase*> samplers;
  for (const base::Value& telemetry : *telemetry_list) {
    if (samplers.size() == telemetry_collectors_.size()) {
      // All samplers are already used, remaining telemetry names would be
      // either invalid or duplicates.
      break;
    }

    const std::string* telemetry_name = telemetry.GetIfString();
    if (telemetry_name &&
        base::Contains(telemetry_collectors_, *telemetry_name) &&
        !base::Contains(samplers,
                        telemetry_collectors_.at(*telemetry_name).get())) {
      samplers.push_back(telemetry_collectors_.at(*telemetry_name).get());
    }
  }
  return samplers;
}

base::TimeDelta MetricReportingManager::GetUploadDelay() const {
  // Upload delay time starts after init delay.
  return delegate_->GetInitDelay() + delegate_->GetInitialUploadDelay();
}

}  // namespace reporting
