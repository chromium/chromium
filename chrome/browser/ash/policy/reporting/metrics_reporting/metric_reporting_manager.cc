// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/components/settings/cros_settings_names.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/audio/audio_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_info_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_telemetry_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/usb/usb_events_observer.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/network/network_bandwidth_sampler.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/metrics/configured_sampler.h"
#include "components/reporting/metrics/metric_data_collector.h"
#include "components/reporting/metrics/metric_event_observer_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/sampler.h"
#include "components/user_manager/user.h"

namespace em = enterprise_management;

namespace reporting {
namespace {

constexpr char kSamplerAudioTelemetry[] = "audio_telemetry";
constexpr char kSamplerBootPerformance[] = "boot_performance";
constexpr char kSamplerHttpsLatency[] = "https_latency";
constexpr char kSamplerNetworkTelemetry[] = "network_telemetry";
constexpr char kSamplerPeripheralTelemetry[] = "peripheral_telemetry";
constexpr char kSamplerDisplaysTelemetry[] = "displays_telemetry";

}  // namespace

bool MetricReportingManager::Delegate::IsAffiliated(Profile* profile) const {
  const user_manager::User* const user =
      ::ash::ProfileHelper::Get()->GetUserByProfile(profile);
  return user && user->IsAffiliated();
}

bool MetricReportingManager::Delegate::IsDeprovisioned() const {
  return ::ash::DeviceSettingsService::IsInitialized() &&
         ::ash::DeviceSettingsService::Get()->policy_data() &&
         ::ash::DeviceSettingsService::Get()->policy_data()->state() ==
             em::PolicyData::DEPROVISIONED;
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
}

void MetricReportingManager::OnLogin(Profile* profile) {
  managed_session_observation_.Reset();
  if (!delegate_->IsAffiliated(profile)) {
    return;
  }

  // Create user metric report queues here since they depend on the user
  // profile only available after login.
  user_telemetry_report_queue_ = delegate_->CreatePeriodicUploadReportQueue(
      EventType::kUser, Destination::TELEMETRY_METRIC, Priority::MANUAL_BATCH,
      &reporting_settings_, ::ash::kReportUploadFrequency,
      metrics::GetDefaultReportUploadFrequency());

  InitOnAffiliatedLogin();
  delayed_init_on_login_timer_.Start(
      FROM_HERE, delegate_->GetInitDelay(),
      base::BindOnce(&MetricReportingManager::DelayedInitOnAffiliatedLogin,
                     base::Unretained(this), profile));
}

void MetricReportingManager::DeviceSettingsUpdated() {
  if (delegate_->IsDeprovisioned()) {
    Shutdown();
  }
}

std::vector<ConfiguredSampler*> MetricReportingManager::GetTelemetrySamplers(
    MetricEventType event_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

MetricReportingManager::MetricReportingManager(
    std::unique_ptr<Delegate> delegate,
    policy::ManagedSessionService* managed_session_service)
    : delegate_(std::move(delegate)) {
  if (delegate_->IsDeprovisioned()) {
    return;
  }
  // Initialize telemetry samplers that can be used before login.
  InitDeviceTelemetrySamplers();

  info_report_queue_ = delegate_->CreateMetricReportQueue(
      EventType::kDevice, Destination::INFO_METRIC, Priority::SLOW_BATCH);
  telemetry_report_queue_ = delegate_->CreatePeriodicUploadReportQueue(
      EventType::kDevice, Destination::TELEMETRY_METRIC, Priority::MANUAL_BATCH,
      &reporting_settings_, ::ash::kReportUploadFrequency,
      metrics::GetDefaultReportUploadFrequency());
  event_report_queue_ = delegate_->CreateMetricReportQueue(
      EventType::kDevice, Destination::EVENT_METRIC, Priority::SLOW_BATCH);
  peripheral_events_and_telemetry_report_queue_ =
      delegate_->CreateMetricReportQueue(EventType::kDevice,
                                         Destination::PERIPHERAL_EVENTS,
                                         Priority::SECURITY);
  delayed_init_timer_.Start(FROM_HERE, delegate_->GetInitDelay(), this,
                            &MetricReportingManager::DelayedInit);

  if (managed_session_service) {
    managed_session_observation_.Observe(managed_session_service);
  }
  if (::ash::DeviceSettingsService::IsInitialized()) {
    device_settings_observation_.Observe(::ash::DeviceSettingsService::Get());
  }
}

void MetricReportingManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  delegate_.reset();
  one_shot_collectors_.clear();
  periodic_collectors_.clear();
  event_observer_managers_.clear();
  info_samplers_.clear();
  telemetry_sampler_map_.clear();
  info_report_queue_.reset();
  telemetry_report_queue_.reset();
  user_telemetry_report_queue_.reset();
  event_report_queue_.reset();
  peripheral_events_and_telemetry_report_queue_.reset();
}

void MetricReportingManager::InitDeviceTelemetrySamplers() {
  auto boot_performance_sampler = std::make_unique<CrosHealthdMetricSampler>(
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kBootPerformance,
      CrosHealthdMetricSampler::MetricType::kTelemetry);
  InitTelemetryConfiguredSampler(
      /*sampler_name=*/kSamplerBootPerformance,
      std::move(boot_performance_sampler),
      /*enable_setting_path=*/::ash::kReportDeviceBootMode,
      /*default_value=*/true);
}

void MetricReportingManager::DelayedInit() {
  if (delegate_->IsDeprovisioned()) {
    return;
  }

  CreateCrosHealthdInfoCollector(
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kCpu,
      ::ash::kReportDeviceCpuInfo,
      /*default_value=*/false);
  CreateCrosHealthdInfoCollector(
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kMemory,
      ::ash::kReportDeviceMemoryInfo,
      /*default_value=*/false);
  CreateCrosHealthdInfoCollector(
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kBus,
      ::ash::kReportDeviceSecurityStatus,
      /*default_value=*/false);
  CreateCrosHealthdInfoCollector(
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kInput,
      ::ash::kReportDeviceGraphicsStatus,
      /*default_value=*/false);
  CreateCrosHealthdInfoCollector(
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

  // Boot performance telemetry collector.
  InitOneShotTelemetryCollector(kSamplerBootPerformance,
                                telemetry_report_queue_.get());

  initial_upload_timer_.Start(FROM_HERE, delegate_->GetInitialUploadDelay(),
                              this, &MetricReportingManager::UploadTelemetry);
}

void MetricReportingManager::InitOnAffiliatedLogin() {
  if (delegate_->IsDeprovisioned()) {
    return;
  }

  InitTelemetrySamplersOnAffiliatedLogin();

  InitEventObserverManager(
      std::make_unique<AudioEventsObserver>(),
      /*enable_setting_path=*/::ash::kReportDeviceAudioStatus,
      metrics::kReportDeviceAudioStatusDefaultValue);
  // Network health events observer.
  InitEventObserverManager(
      std::make_unique<NetworkEventsObserver>(),
      /*enable_setting_path=*/::ash::kReportDeviceNetworkStatus,
      metrics::kReportDeviceNetworkStatusDefaultValue);
  InitPeripheralsCollectors();
}

void MetricReportingManager::InitTelemetrySamplersOnAffiliatedLogin() {
  // Initialize telemetry samplers that can only be used after affiliated login.
  auto audio_telemetry_sampler = std::make_unique<CrosHealthdMetricSampler>(
      ::ash::cros_healthd::mojom::ProbeCategoryEnum::kAudio,
      CrosHealthdMetricSampler::MetricType::kTelemetry);
  InitTelemetryConfiguredSampler(
      /*sampler_name=*/kSamplerAudioTelemetry,
      std::move(audio_telemetry_sampler),
      /*enable_setting_path=*/::ash::kReportDeviceAudioStatus,
      metrics::kReportDeviceAudioStatusDefaultValue);
  auto https_latency_sampler = std::make_unique<HttpsLatencySampler>();
  InitNetworkConfiguredSampler(/*sampler_name=*/kSamplerHttpsLatency,
                               std::move(https_latency_sampler));
  auto network_telemetry_sampler = std::make_unique<NetworkTelemetrySampler>();
  InitNetworkConfiguredSampler(
      /*sampler_name=*/kSamplerNetworkTelemetry,
      std::move(network_telemetry_sampler));
  auto peripheral_telemetry_sampler =
      std::make_unique<CrosHealthdMetricSampler>(
          ::ash::cros_healthd::mojom::ProbeCategoryEnum::kBus,
          CrosHealthdMetricSampler::MetricType::kTelemetry);
  InitTelemetryConfiguredSampler(
      /*sampler_name=*/kSamplerPeripheralTelemetry,
      std::move(peripheral_telemetry_sampler),
      /*enable_setting_path=*/::ash::kReportDevicePeripherals,
      metrics::kReportDevicePeripheralsDefaultValue);
  auto displays_telemetry_sampler = std::make_unique<CrosHealthdMetricSampler>(
      ash::cros_healthd::mojom::ProbeCategoryEnum::kDisplay,
      CrosHealthdMetricSampler::MetricType::kTelemetry);
  InitTelemetryConfiguredSampler(
      /*sampler_name=*/kSamplerDisplaysTelemetry,
      std::move(displays_telemetry_sampler),
      /*enable_setting_path=*/::ash::kReportDeviceGraphicsStatus,
      metrics::kReportDeviceGraphicsStatusDefaultValue);
}

void MetricReportingManager::DelayedInitOnAffiliatedLogin(Profile* profile) {
  if (delegate_->IsDeprovisioned()) {
    return;
  }

  InitNetworkCollectors(profile);
  InitAudioCollectors();
  InitDisplayCollectors();

  initial_upload_timer_.Start(FROM_HERE, delegate_->GetInitialUploadDelay(),
                              this, &MetricReportingManager::UploadTelemetry);
}

void MetricReportingManager::InitInfoCollector(
    std::unique_ptr<Sampler> sampler,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!info_report_queue_) {
    return;
  }
  one_shot_collectors_.emplace_back(delegate_->CreateOneShotCollector(
      sampler.get(), info_report_queue_.get(), &reporting_settings_,
      enable_setting_path, setting_enabled_default_value));
  info_samplers_.emplace_back(std::move(sampler));
}

void MetricReportingManager::InitOneShotTelemetryCollector(
    const std::string& sampler_name,
    MetricReportQueue* metric_report_queue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(telemetry_sampler_map_, sampler_name));

  if (!metric_report_queue) {
    return;
  }

  auto* const configured_sampler =
      telemetry_sampler_map_.at(sampler_name).get();
  periodic_collectors_.emplace_back(delegate_->CreateOneShotCollector(
      configured_sampler->GetSampler(), metric_report_queue,
      &reporting_settings_, configured_sampler->GetEnableSettingPath(),
      configured_sampler->GetSettingEnabledDefaultValue()));
}

void MetricReportingManager::InitPeriodicCollector(
    const std::string& sampler_name,
    MetricReportQueue* metric_report_queue,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(telemetry_sampler_map_, sampler_name));

  if (!metric_report_queue) {
    return;
  }

  auto* const configured_sampler =
      telemetry_sampler_map_.at(sampler_name).get();
  periodic_collectors_.emplace_back(delegate_->CreatePeriodicCollector(
      configured_sampler->GetSampler(), metric_report_queue,
      &reporting_settings_, configured_sampler->GetEnableSettingPath(),
      configured_sampler->GetSettingEnabledDefaultValue(), rate_setting_path,
      default_rate, rate_unit_to_ms));
}

void MetricReportingManager::InitPeriodicEventCollector(
    const std::string& sampler_name,
    std::unique_ptr<EventDetector> event_detector,
    MetricReportQueue* metric_report_queue,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(telemetry_sampler_map_, sampler_name));

  if (!metric_report_queue) {
    return;
  }

  auto* const configured_sampler =
      telemetry_sampler_map_.at(sampler_name).get();
  periodic_collectors_.emplace_back(delegate_->CreatePeriodicEventCollector(
      configured_sampler->GetSampler(), std::move(event_detector),
      /*sampler_pool=*/this, metric_report_queue, &reporting_settings_,
      configured_sampler->GetEnableSettingPath(),
      configured_sampler->GetSettingEnabledDefaultValue(), rate_setting_path,
      default_rate, rate_unit_to_ms));
}

void MetricReportingManager::InitEventObserverManager(
    std::unique_ptr<MetricEventObserver> event_observer,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value) {
  if (!event_report_queue_) {
    return;
  }
  event_observer_managers_.emplace_back(delegate_->CreateEventObserverManager(
      std::move(event_observer), event_report_queue_.get(),
      &reporting_settings_, enable_setting_path, setting_enabled_default_value,
      /*sampler_pool=*/this));
}

void MetricReportingManager::UploadTelemetry() {
  if (!telemetry_report_queue_) {
    return;
  }
  telemetry_report_queue_->Upload();
}

void MetricReportingManager::CreateCrosHealthdInfoCollector(
    ::ash::cros_healthd::mojom::ProbeCategoryEnum probe_category,
    const std::string& setting_path,
    bool default_value) {
  auto croshealthd_sampler = std::make_unique<CrosHealthdMetricSampler>(
      probe_category, CrosHealthdMetricSampler::MetricType::kInfo);
  InitInfoCollector(std::move(croshealthd_sampler), setting_path,
                    default_value);
}

void MetricReportingManager::InitTelemetryConfiguredSampler(
    const std::string& sampler_name,
    std::unique_ptr<Sampler> sampler,
    const std::string& enable_setting_path,
    bool default_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto configured_sampler = std::make_unique<ConfiguredSampler>(
      std::move(sampler), enable_setting_path, default_value,
      &reporting_settings_);
  telemetry_sampler_map_.insert({sampler_name, std::move(configured_sampler)});
}

void MetricReportingManager::InitNetworkCollectors(Profile* profile) {
  // Network health telemetry.
  InitNetworkPeriodicCollector(kSamplerNetworkTelemetry,
                               telemetry_report_queue_.get());

  // HttpsLatency telemetry.
  InitNetworkPeriodicCollector(kSamplerHttpsLatency,
                               telemetry_report_queue_.get());

  // HttpsLatency events.
  InitPeriodicEventCollector(
      kSamplerHttpsLatency, std::make_unique<HttpsLatencyEventDetector>(),
      event_report_queue_.get(),
      ::ash::kReportDeviceNetworkTelemetryEventCheckingRateMs,
      metrics::GetDefaultEventCheckingRate(
          metrics::kDefaultNetworkTelemetryEventCheckingRate));

  // Network bandwidth telemetry.
  network_bandwidth_sampler_ = std::make_unique<NetworkBandwidthSampler>(
      g_browser_process->network_quality_tracker(), profile);
  periodic_collectors_.emplace_back(delegate_->CreatePeriodicCollector(
      network_bandwidth_sampler_.get(), user_telemetry_report_queue_.get(),
      &reporting_settings_,
      /*enable_setting_path=*/
      ::ash::kReportDeviceNetworkStatus,
      metrics::kReportDeviceNetworkStatusDefaultValue,
      ::ash::kReportDeviceNetworkTelemetryCollectionRateMs,
      metrics::GetDefaultCollectionRate(
          metrics::kDefaultNetworkTelemetryCollectionRate),
      /*rate_unit_to_ms=*/1));
}

void MetricReportingManager::InitNetworkPeriodicCollector(
    const std::string& sampler_name,
    MetricReportQueue* metric_report_queue) {
  InitPeriodicCollector(sampler_name, metric_report_queue,
                        ::ash::kReportDeviceNetworkTelemetryCollectionRateMs,
                        metrics::GetDefaultCollectionRate(
                            metrics::kDefaultNetworkTelemetryCollectionRate));
}

void MetricReportingManager::InitNetworkConfiguredSampler(
    const std::string& sampler_name,
    std::unique_ptr<Sampler> sampler) {
  InitTelemetryConfiguredSampler(
      sampler_name, std::move(sampler),
      /*enable_setting_path=*/::ash::kReportDeviceNetworkStatus,
      metrics::kReportDeviceNetworkStatusDefaultValue);
}

void MetricReportingManager::InitAudioCollectors() {
  InitPeriodicCollector(kSamplerAudioTelemetry, telemetry_report_queue_.get(),
                        ::ash::kReportDeviceAudioStatusCheckingRateMs,
                        metrics::GetDefaultCollectionRate(
                            metrics::kDefaultAudioTelemetryCollectionRate));
}

void MetricReportingManager::InitPeripheralsCollectors() {
  // Peripheral events
  if (!peripheral_events_and_telemetry_report_queue_) {
    return;
  }
  event_observer_managers_.emplace_back(delegate_->CreateEventObserverManager(
      std::make_unique<UsbEventsObserver>(),
      peripheral_events_and_telemetry_report_queue_.get(), &reporting_settings_,
      ::ash::kReportDevicePeripherals,
      metrics::kReportDevicePeripheralsDefaultValue,
      /*sampler_pool=*/this));

  // Peripheral telemetry
  InitOneShotTelemetryCollector(
      kSamplerPeripheralTelemetry,
      peripheral_events_and_telemetry_report_queue_.get());
}

void MetricReportingManager::InitDisplayCollectors() {
  InitPeriodicCollector(kSamplerDisplaysTelemetry,
                        telemetry_report_queue_.get(),
                        ::ash::kReportUploadFrequency,
                        metrics::GetDefaultCollectionRate(
                            metrics::kDefaultGraphicsTelemetryCollectionRate));
}
}  // namespace reporting
