// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/reporting/device_reporting_settings_lacros.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/chromeos/reporting/network/network_bandwidth_sampler.h"
#include "chrome/browser/chromeos/reporting/user_reporting_settings.h"
#include "chrome/browser/chromeos/reporting/websites/website_events_observer.h"
#include "chrome/browser/chromeos/reporting/websites/website_metrics_retriever_lacros.h"
#include "chrome/browser/chromeos/reporting/websites/website_usage_observer.h"
#include "chrome/browser/chromeos/reporting/websites/website_usage_telemetry_periodic_collector_lacros.h"
#include "chrome/browser/chromeos/reporting/websites/website_usage_telemetry_sampler.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/policy/policy_constants.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/version_info/version_info.h"

namespace reporting::metrics {

void MetricReportingManagerLacros::Delegate::CheckDeviceDeprovisioned(
    crosapi::mojom::DeviceSettingsService::IsDeviceDeprovisionedCallback
        callback) {
  auto* const lacros_service = ::chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsRegistered<crosapi::mojom::DeviceSettingsService>() ||
      !lacros_service->IsAvailable<crosapi::mojom::DeviceSettingsService>()) {
    // We should rarely get here since Lacros is initialized via Ash on ChromeOS
    // devices, but we return false anyway.
    std::move(callback).Run(false);
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::DeviceSettingsService>()
      ->IsDeviceDeprovisioned(std::move(callback));
}

std::unique_ptr<DeviceReportingSettingsLacros>
MetricReportingManagerLacros::Delegate::CreateDeviceReportingSettings() {
  return DeviceReportingSettingsLacros::Create();
}

void MetricReportingManagerLacros::Delegate::RegisterObserverWithCrosApiClient(
    MetricReportingManagerLacros* const instance) {
  g_browser_process->browser_policy_connector()
      ->device_settings_lacros()
      ->AddObserver(instance);
}

MetricReportingManagerLacros::MetricReportingManagerLacros(
    Profile* profile,
    std::unique_ptr<MetricReportingManagerLacros::Delegate> delegate)
    : profile_(profile),
      delegate_(std::move(delegate)),
      device_reporting_settings_(delegate_->CreateDeviceReportingSettings()),
      user_reporting_settings_(
          std::make_unique<UserReportingSettings>(profile_->GetWeakPtr())),
      is_device_deprovisioned_(false) {
  CHECK_NE(profile, nullptr);
  if (!delegate_->IsUserAffiliated(*profile_)) {
    // We only report data for affiliated users on managed devices as of today.
    return;
  }

  // Update cached device deprovisioned state and initialize appropriate
  // components.
  auto is_deprovisioned_callback = base::BindOnce(
      [](base::WeakPtr<MetricReportingManagerLacros> instance,
         bool is_deprovisioned) {
        if (!instance) {
          return;
        }

        DCHECK_CALLED_ON_VALID_SEQUENCE(instance->sequence_checker_);
        instance->is_device_deprovisioned_ = is_deprovisioned;
        if (is_deprovisioned) {
          return;
        }

        SourceInfo source_info;
        source_info.set_source(SourceInfo::LACROS);
        source_info.set_source_version(
            std::string(::version_info::GetVersionNumber()));
        instance->telemetry_report_queue_ =
            instance->delegate_->CreatePeriodicUploadReportQueue(
                EventType::kUser, Destination::TELEMETRY_METRIC,
                Priority::MANUAL_BATCH_LACROS,
                instance->device_reporting_settings_.get(),
                ::policy::key::kReportUploadFrequency,
                GetDefaultReportUploadFrequency(),
                /*rate_unit_to_ms=*/1, source_info);
        auto website_event_rate_limiter =
            instance->delegate_->CreateSlidingWindowRateLimiter(
                kWebsiteEventsTotalSize, kWebsiteEventsWindow,
                kWebsiteEventsBucketCount);
        instance->website_event_report_queue_ =
            instance->delegate_->CreateMetricReportQueue(
                EventType::kUser, Destination::EVENT_METRIC,
                Priority::SLOW_BATCH, std::move(website_event_rate_limiter),
                std::move(source_info));

        instance->delegate_->RegisterObserverWithCrosApiClient(instance.get());
        instance->Init();
        instance->delayed_init_timer_.Start(
            FROM_HERE, instance->delegate_->GetInitDelay(),
            base::BindOnce(&MetricReportingManagerLacros::DelayedInit,
                           instance));
      },
      weak_ptr_factory_.GetWeakPtr());
  delegate_->CheckDeviceDeprovisioned(std::move(is_deprovisioned_callback));
}

MetricReportingManagerLacros::~MetricReportingManagerLacros() = default;

void MetricReportingManagerLacros::OnDeviceSettingsUpdated() {
  // Update cached device deprovisioned state and trigger shutdown if needed.
  auto is_deprovisioned_callback = base::BindOnce(
      [](base::WeakPtr<MetricReportingManagerLacros> instance,
         bool is_deprovisioned) {
        if (!instance) {
          return;
        }

        DCHECK_CALLED_ON_VALID_SEQUENCE(instance->sequence_checker_);
        instance->is_device_deprovisioned_ = is_deprovisioned;
        if (is_deprovisioned) {
          instance->Shutdown();
        }
      },
      weak_ptr_factory_.GetWeakPtr());
  delegate_->CheckDeviceDeprovisioned(std::move(is_deprovisioned_callback));
}

void MetricReportingManagerLacros::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  website_usage_observer_.reset();
  samplers_.clear();
  event_observer_managers_.clear();
  periodic_collectors_.clear();
  website_event_report_queue_.reset();
  telemetry_report_queue_.reset();
}

void MetricReportingManagerLacros::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_device_deprovisioned_) {
    return;
  }
  InitWebsiteMetricCollectors();
}

void MetricReportingManagerLacros::DelayedInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_device_deprovisioned_) {
    return;
  }

  InitNetworkCollectors();
  initial_upload_timer_.Start(FROM_HERE, delegate_->GetInitialUploadDelay(),
                              this,
                              &MetricReportingManagerLacros::UploadTelemetry);
}

void MetricReportingManagerLacros::InitNetworkCollectors() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto network_bandwidth_sampler = std::make_unique<NetworkBandwidthSampler>(
      g_browser_process->network_quality_tracker(), profile_->GetWeakPtr());

  // Network bandwidth telemetry.
  InitPeriodicCollector(
      std::move(network_bandwidth_sampler), telemetry_report_queue_.get(),
      /*enable_setting_path=*/::policy::key::kReportDeviceNetworkStatus,
      kReportDeviceNetworkStatusDefaultValue,
      ::policy::key::kReportDeviceNetworkTelemetryCollectionRateMs,
      GetDefaultCollectionRate(kDefaultNetworkTelemetryCollectionRate));
}

void MetricReportingManagerLacros::InitWebsiteMetricCollectors() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(profile_);
  const auto profile_weak_ptr = profile_->GetWeakPtr();

  // Website events.
  auto website_events_observer = std::make_unique<WebsiteEventsObserver>(
      std::make_unique<WebsiteMetricsRetrieverLacros>(profile_weak_ptr),
      user_reporting_settings_.get());
  InitEventObserverManager(
      std::move(website_events_observer), website_event_report_queue_.get(),
      user_reporting_settings_.get(), kReportWebsiteActivityAllowlist,
      kReportWebsiteActivityEnabledDefaultValue,
      /*init_delay=*/base::TimeDelta());

  // Website telemetry.
  website_usage_observer_ = std::make_unique<WebsiteUsageObserver>(
      profile_weak_ptr, user_reporting_settings_.get(),
      std::make_unique<WebsiteMetricsRetrieverLacros>(profile_weak_ptr));
  auto website_usage_telemetry_sampler =
      std::make_unique<WebsiteUsageTelemetrySampler>(profile_weak_ptr);
  auto website_usage_telemetry_periodic_collector =
      std::make_unique<WebsiteUsageTelemetryPeriodicCollectorLacros>(
          profile_, website_usage_telemetry_sampler.get(),
          telemetry_report_queue_.get(), user_reporting_settings_.get());
  samplers_.emplace_back(std::move(website_usage_telemetry_sampler));
  periodic_collectors_.emplace_back(
      std::move(website_usage_telemetry_periodic_collector));
}

void MetricReportingManagerLacros::InitPeriodicCollector(
    std::unique_ptr<Sampler> sampler,
    MetricReportQueue* metric_report_queue,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* const sampler_ptr = sampler.get();
  samplers_.emplace_back(std::move(sampler));
  if (!metric_report_queue) {
    return;
  }
  periodic_collectors_.emplace_back(delegate_->CreatePeriodicCollector(
      sampler_ptr, metric_report_queue, device_reporting_settings_.get(),
      enable_setting_path, setting_enabled_default_value, rate_setting_path,
      default_rate, rate_unit_to_ms));
}

void MetricReportingManagerLacros::InitEventObserverManager(
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
      /*collector_pool=*/nullptr, init_delay));
}

void MetricReportingManagerLacros::UploadTelemetry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!telemetry_report_queue_) {
    return;
  }
  telemetry_report_queue_->Upload();
}

}  // namespace reporting::metrics
