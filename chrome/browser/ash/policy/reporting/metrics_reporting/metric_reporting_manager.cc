// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"

#include "ash/components/settings/cros_settings_names.h"
#include "base/logging.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_info_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_telemetry_sampler.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/metrics/metric_data_collector.h"
#include "components/reporting/metrics/metric_event_observer_manager.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/sampler.h"
#include "components/user_manager/user.h"

namespace em = enterprise_management;

namespace reporting {
namespace {

constexpr base::TimeDelta kDefaultReportUploadFrequencyForTesting =
    base::Minutes(5);
constexpr base::TimeDelta kDefaultCollectionRateForTesting = base::Minutes(2);
constexpr base::TimeDelta kDefaultEventCheckingRateForTesting =
    base::Minutes(1);

constexpr base::TimeDelta kDefaultReportUploadFrequency = base::Hours(3);
constexpr base::TimeDelta kDefaultNetworkTelemetryCollectionRate =
    base::Minutes(10);
constexpr base::TimeDelta kDefaultNetworkTelemetryEventCheckingRate =
    base::Minutes(2);

base::TimeDelta GetDefaultRate(base::TimeDelta default_rate,
                               base::TimeDelta testing_rate) {
  // If telemetry testing rates flag is enabled, use `testing_rate` to reduce
  // time before metric collection and reporting.
  return base::FeatureList::IsEnabled(
             MetricRateController::kEnableTelemetryTestingRates)
             ? testing_rate
             : default_rate;
}

base::TimeDelta GetDefaultReportUploadFrequency() {
  return GetDefaultRate(kDefaultReportUploadFrequency,
                        kDefaultReportUploadFrequencyForTesting);
}

base::TimeDelta GetDefaulCollectionRate(base::TimeDelta default_rate) {
  return GetDefaultRate(default_rate, kDefaultCollectionRateForTesting);
}

base::TimeDelta GetDefaulEventCheckingRate(base::TimeDelta default_rate) {
  return GetDefaultRate(default_rate, kDefaultEventCheckingRateForTesting);
}

}  // namespace

// static
const base::Feature MetricReportingManager::kEnableNetworkTelemetryReporting{
    "EnableNetworkTelemetryReporting", base::FEATURE_DISABLED_BY_DEFAULT};

bool MetricReportingManager::Delegate::IsAffiliated(Profile* profile) {
  const user_manager::User* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  return user && user->IsAffiliated();
}

std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
MetricReportingManager::Delegate::CreateReportQueue(Destination destination) {
  return ReportQueueFactory::CreateSpeculativeReportQueue(EventType::kDevice,
                                                          destination);
}

std::unique_ptr<Sampler>
MetricReportingManager::Delegate::CreateHttpsLatencySampler() {
  return std::make_unique<HttpsLatencySampler>();
}

bool MetricReportingManager::Delegate::IsDeprovisioned() {
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

MetricReportingManager::~MetricReportingManager() = default;

void MetricReportingManager::OnLogin(Profile* profile) {
  managed_session_observation_.Reset();
  if (!delegate_->IsAffiliated(profile)) {
    return;
  }
  InitOnAffiliatedLogin();
}

void MetricReportingManager::DeviceSettingsUpdated() {
  if (delegate_->IsDeprovisioned()) {
    Shutdown();
  }
}

MetricReportingManager::MetricReportingManager(
    std::unique_ptr<Delegate> delegate,
    policy::ManagedSessionService* managed_session_service)
    : delegate_(std::move(delegate)) {
  if (delegate_->IsDeprovisioned()) {
    return;
  }
  Init();
  if (managed_session_service) {
    managed_session_observation_.Observe(managed_session_service);
  }
  if (::ash::DeviceSettingsService::IsInitialized()) {
    device_settings_observation_.Observe(::ash::DeviceSettingsService::Get());
  }
}

void MetricReportingManager::Shutdown() {
  one_shot_collectors_.clear();
  periodic_collectors_.clear();
  event_observer_managers_.clear();
  samplers_.clear();
  info_report_queue_.reset();
  telemetry_report_queue_.reset();
  event_report_queue_.reset();
}

void MetricReportingManager::Init() {
  info_report_queue_ =
      CreateMetricReportQueue(Destination::INFO_METRIC, Priority::SLOW_BATCH);
  telemetry_report_queue_ = CreateTelemetryQueue();
  event_report_queue_ =
      CreateMetricReportQueue(Destination::EVENT_METRIC, Priority::SLOW_BATCH);

  InitCrosHealthdInfoCollector(
      chromeos::cros_healthd::mojom::ProbeCategoryEnum::kCpu,
      ::ash::kReportDeviceCpuInfo, /*default_value=*/false);
  InitCrosHealthdInfoCollector(
      chromeos::cros_healthd::mojom::ProbeCategoryEnum::kMemory,
      ::ash::kReportDeviceMemoryInfo, /*default_value=*/false);
  InitCrosHealthdInfoCollector(
      chromeos::cros_healthd::mojom::ProbeCategoryEnum::kBus,
      ::ash::kReportDeviceSecurityStatus,
      /*default_value=*/false);
  if (base::FeatureList::IsEnabled(kEnableNetworkTelemetryReporting)) {
    // Network health info.
    // ReportDeviceNetworkConfiguration policy is enabled by default, so set its
    // default value to true.
    CreateOneShotCollector(
        std::make_unique<NetworkInfoSampler>(), info_report_queue_.get(),
        /*enable_setting_path=*/::ash::kReportDeviceNetworkConfiguration,
        /*setting_enabled_default_value=*/true);
  }
}

void MetricReportingManager::InitOnAffiliatedLogin() {
  InitNetworkCollectors();
}

std::unique_ptr<MetricReportQueue>
MetricReportingManager::CreateMetricReportQueue(Destination destination,
                                                Priority priority) {
  std::unique_ptr<MetricReportQueue> metric_report_queue;
  auto report_queue = delegate_->CreateReportQueue(destination);
  if (report_queue) {
    metric_report_queue =
        std::make_unique<MetricReportQueue>(std::move(report_queue), priority);
  } else {
    DVLOG(1) << "Cannot create metric report queue, report queue is null";
  }
  return metric_report_queue;
}

std::unique_ptr<MetricReportQueue>
MetricReportingManager::CreateTelemetryQueue() {
  std::unique_ptr<MetricReportQueue> telemetry_report_queue;
  auto report_queue =
      delegate_->CreateReportQueue(Destination::TELEMETRY_METRIC);
  if (report_queue) {
    telemetry_report_queue = std::make_unique<MetricReportQueue>(
        std::move(report_queue), Priority::MANUAL_BATCH, &reporting_settings_,
        ::ash::kReportUploadFrequency, GetDefaultReportUploadFrequency());
  } else {
    DVLOG(1) << "Cannot create telemetry report queue, report queue is null";
  }
  return telemetry_report_queue;
}

void MetricReportingManager::CreateOneShotCollector(
    std::unique_ptr<Sampler> sampler,
    MetricReportQueue* metric_report_queue,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value) {
  auto* const sampler_ptr = sampler.get();
  samplers_.emplace_back(std::move(sampler));
  if (!metric_report_queue) {
    return;
  }
  one_shot_collectors_.emplace_back(std::make_unique<OneShotCollector>(
      sampler_ptr, metric_report_queue, &reporting_settings_,
      enable_setting_path, setting_enabled_default_value));
}

void MetricReportingManager::CreatePeriodicCollector(
    std::unique_ptr<Sampler> sampler,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms) {
  auto* const sampler_ptr = sampler.get();
  samplers_.emplace_back(std::move(sampler));
  if (!telemetry_report_queue_) {
    return;
  }
  periodic_collectors_.emplace_back(std::make_unique<PeriodicCollector>(
      sampler_ptr, telemetry_report_queue_.get(), &reporting_settings_,
      enable_setting_path, setting_enabled_default_value, rate_setting_path,
      default_rate, rate_unit_to_ms));
}

void MetricReportingManager::CreatePeriodicEventCollector(
    std::unique_ptr<Sampler> sampler,
    std::unique_ptr<EventDetector> event_detector,
    std::vector<Sampler*> additional_samplers,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms) {
  auto* const sampler_ptr = sampler.get();
  samplers_.emplace_back(std::move(sampler));
  if (!event_report_queue_) {
    return;
  }
  periodic_collectors_.emplace_back(std::make_unique<PeriodicEventCollector>(
      sampler_ptr, std::move(event_detector), std::move(additional_samplers),
      event_report_queue_.get(), &reporting_settings_, enable_setting_path,
      setting_enabled_default_value, rate_setting_path, default_rate,
      rate_unit_to_ms));
}

void MetricReportingManager::CreateEventObserverManager(
    std::unique_ptr<MetricEventObserver> event_observer,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value,
    std::vector<Sampler*> additional_samplers) {
  if (!event_report_queue_) {
    return;
  }
  event_observer_managers_.emplace_back(
      std::make_unique<MetricEventObserverManager>(
          std::move(event_observer), event_report_queue_.get(),
          &reporting_settings_, enable_setting_path,
          setting_enabled_default_value, std::move(additional_samplers)));
}

void MetricReportingManager::InitCrosHealthdInfoCollector(
    chromeos::cros_healthd::mojom::ProbeCategoryEnum probe_category,
    const std::string& setting_path,
    bool default_value) {
  auto info_sampler = std::make_unique<CrosHealthdMetricSampler>(
      probe_category, CrosHealthdMetricSampler::MetricType::kInfo);
  CreateOneShotCollector(std::move(info_sampler), info_report_queue_.get(),
                         setting_path, default_value);
}

void MetricReportingManager::InitNetworkCollectors() {
  if (!base::FeatureList::IsEnabled(kEnableNetworkTelemetryReporting)) {
    return;
  }

  auto https_latency_sampler = delegate_->CreateHttpsLatencySampler();
  auto network_telemetry_sampler =
      std::make_unique<NetworkTelemetrySampler>(https_latency_sampler.get());
  // Network health telemetry.
  // ReportDeviceNetworkStatus policy is enabled by default, so set its default
  // value to true.
  const bool kReportDeviceNetworkStatusDefaultValue = true;
  CreatePeriodicCollector(
      std::move(network_telemetry_sampler),
      /*enable_setting_path=*/::ash::kReportDeviceNetworkStatus,
      kReportDeviceNetworkStatusDefaultValue,
      ::ash::kReportDeviceNetworkTelemetryCollectionRateMs,
      GetDefaulCollectionRate(kDefaultNetworkTelemetryCollectionRate));
  // HttpsLatency events.
  CreatePeriodicEventCollector(
      std::move(https_latency_sampler),
      std::make_unique<HttpsLatencyEventDetector>(), /*additional_samplers=*/{},
      /*enable_setting_path=*/::ash::kReportDeviceNetworkStatus,
      kReportDeviceNetworkStatusDefaultValue,
      ::ash::kReportDeviceNetworkTelemetryEventCheckingRateMs,
      GetDefaulEventCheckingRate(kDefaultNetworkTelemetryEventCheckingRate));
  // Network health events observer.
  CreateEventObserverManager(
      std::make_unique<NetworkEventsObserver>(),
      /*enable_setting_path=*/::ash::kReportDeviceNetworkStatus,
      kReportDeviceNetworkStatusDefaultValue);
}

}  // namespace reporting
