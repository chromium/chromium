// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"

#include "ash/components/settings/cros_settings_names.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/https_latency_sampler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_telemetry_sampler.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/metrics/metric_data_collector.h"
#include "components/reporting/metrics/metric_rate_controller.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/user_manager/user.h"

namespace reporting {
namespace {

constexpr base::TimeDelta kDefaultReportUploadFrequency = base::Hours(3);
constexpr base::TimeDelta kDefaultNetworkTelemetryCollectionRate =
    base::Minutes(10);

base::TimeDelta GetDefaultReportUploadFrequency() {
  // If telemetry testing rates flag is enabled, use an upload interval, to
  // avoid waiting for at least 60 mins which is the status upload minimum rate
  // in the admin console.
  return base::FeatureList::IsEnabled(
             MetricRateController::kEnableTelemetryTestingRates)
             ? base::Minutes(5)
             : kDefaultReportUploadFrequency;
}

base::TimeDelta GetDefaulCollectionRate(base::TimeDelta default_rate) {
  // If telemetry testing rates flag is enabled, use a 2 mins collection to
  // avoid waiting for long time for collection in the case of low default rate
  // and uncontrollable rate policy.
  return base::FeatureList::IsEnabled(
             MetricRateController::kEnableTelemetryTestingRates)
             ? base::Minutes(2)
             : default_rate;
}

}  // namespace

// static
const base::Feature MetricReportingManager::kEnableNetworkTelemetryReporting{
    "EnableNetworkTelemetryReporting", base::FEATURE_DISABLED_BY_DEFAULT};

MetricReportingManager::Delegate::Delegate() = default;

MetricReportingManager::Delegate::~Delegate() = default;

std::unique_ptr<MetricReportQueue>
MetricReportingManager::Delegate::CreateInfoReportQueue() {
  // Pass empty dm token value so the report queue get and use the device dm
  // token.
  auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      /*dm_token_value=*/"", Destination::INFO_METRIC);

  return std::make_unique<MetricReportQueue>(std::move(report_queue),
                                             Priority::SLOW_BATCH);
}

std::unique_ptr<MetricReportQueue>
MetricReportingManager::Delegate::CreateEventReportQueue() {
  // Pass empty dm token value so the report queue get and use the device dm
  // token.
  auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      /*dm_token_value=*/"", Destination::EVENT_METRIC);

  return std::make_unique<MetricReportQueue>(std::move(report_queue),
                                             Priority::FAST_BATCH);
}

std::unique_ptr<MetricReportQueue>
MetricReportingManager::Delegate::CreateTelemetryReportQueue(
    ReportingSettings* reporting_settings,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate) {
  // Pass empty dm token value so the report queue get and use the device dm
  // token.
  auto report_queue = ReportQueueFactory::CreateSpeculativeReportQueue(
      /*dm_token_value=*/"", Destination::TELEMETRY_METRIC);

  // TODO(b/177847653):: Add new manual priority specific to telemetry
  // upload and use it here instead of the general `Priority::MANUAL_BATCH`.
  return std::make_unique<MetricReportQueue>(
      std::move(report_queue), Priority::MANUAL_BATCH, reporting_settings,
      rate_setting_path, default_rate);
}

Sampler* MetricReportingManager::Delegate::AddSampler(
    std::unique_ptr<Sampler> sampler) {
  auto* sampler_ptr = sampler.get();
  samplers_.emplace_back(std::move(sampler));
  return sampler_ptr;
}

bool MetricReportingManager::Delegate::IsAffiliated(Profile* profile) {
  const user_manager::User* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  return user && user->IsAffiliated();
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

MetricReportingManager::MetricReportingManager(
    std::unique_ptr<Delegate> delegate,
    policy::ManagedSessionService* managed_session_service)
    : delegate_(std::move(delegate)) {
  if (managed_session_service) {
    managed_session_observation_.Observe(managed_session_service);
  }
  Init();
}

void MetricReportingManager::Init() {
  telemetry_report_queue_ = delegate_->CreateTelemetryReportQueue(
      &reporting_settings_, ash::kReportUploadFrequency,
      GetDefaultReportUploadFrequency());
}

void MetricReportingManager::InitOnAffiliatedLogin() {
  InitNetworkCollectors();
}

void MetricReportingManager::CreatePeriodicCollector(
    Sampler* sampler,
    const std::string& enable_setting_path,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms) {
  periodic_collectors_.emplace_back(std::make_unique<PeriodicCollector>(
      sampler, telemetry_report_queue_.get(), &reporting_settings_,
      enable_setting_path, rate_setting_path, default_rate, rate_unit_to_ms));
}

void MetricReportingManager::InitNetworkCollectors() {
  if (!base::FeatureList::IsEnabled(kEnableNetworkTelemetryReporting)) {
    return;
  }

  auto* https_latency_sampler =
      delegate_->AddSampler(std::make_unique<HttpsLatencySampler>());
  auto* network_telemetry_sampler = delegate_->AddSampler(
      std::make_unique<NetworkTelemetrySampler>(https_latency_sampler));
  CreatePeriodicCollector(
      network_telemetry_sampler, ash::kReportDeviceNetworkStatus,
      ash::kReportDeviceNetworkTelemetryCollectionRateMs,
      GetDefaulCollectionRate(kDefaultNetworkTelemetryCollectionRate));
}
}  // namespace reporting
