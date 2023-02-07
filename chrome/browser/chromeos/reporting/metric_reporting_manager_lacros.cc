// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/reporting/device_reporting_settings_lacros.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/network/network_bandwidth_sampler.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/policy/policy_constants.h"

namespace reporting::metrics {
namespace {

// Factory implementation for the `MetricReportingManagerLacros` for a given
// `BrowserContext`.
class MetricReportingManagerLacrosFactory : public ProfileKeyedServiceFactory {
 public:
  MetricReportingManagerLacrosFactory();
  MetricReportingManagerLacrosFactory(
      const MetricReportingManagerLacrosFactory&) = delete;
  MetricReportingManagerLacrosFactory& operator=(
      const MetricReportingManagerLacrosFactory&) = delete;
  ~MetricReportingManagerLacrosFactory() override;

  // Returns an instance of `MetricReportingManagerLacros` for the
  // given profile.
  static MetricReportingManagerLacros* GetForProfile(Profile* profile);

  static void EnsureFactoryBuilt();

 private:
  static MetricReportingManagerLacrosFactory* GetInstance();

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

MetricReportingManagerLacrosFactory::MetricReportingManagerLacrosFactory()
    : ProfileKeyedServiceFactory("MetricReportingManagerLacros") {}

MetricReportingManagerLacrosFactory::~MetricReportingManagerLacrosFactory() =
    default;

MetricReportingManagerLacros*
MetricReportingManagerLacrosFactory::GetForProfile(Profile* profile) {
  DCHECK(profile);
  if (!profile->IsMainProfile()) {
    // We only report metrics and events for main profile today.
    return nullptr;
  }
  return static_cast<MetricReportingManagerLacros*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
void MetricReportingManagerLacrosFactory::EnsureFactoryBuilt() {
  GetInstance();
}

// static
MetricReportingManagerLacrosFactory*
MetricReportingManagerLacrosFactory::GetInstance() {
  static base::NoDestructor<MetricReportingManagerLacrosFactory> g_factory;
  return g_factory.get();
}

KeyedService* MetricReportingManagerLacrosFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* const profile = Profile::FromBrowserContext(context);
  return new MetricReportingManagerLacros(
      profile, std::make_unique<MetricReportingManagerLacros::Delegate>());
}
}  // namespace

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

// static
MetricReportingManagerLacros* MetricReportingManagerLacros::GetForProfile(
    Profile* profile) {
  return MetricReportingManagerLacrosFactory::GetForProfile(profile);
}

MetricReportingManagerLacros::MetricReportingManagerLacros(
    Profile* profile,
    std::unique_ptr<MetricReportingManagerLacros::Delegate> delegate)
    : profile_(profile),
      delegate_(std::move(delegate)),
      device_reporting_settings_(delegate_->CreateDeviceReportingSettings()),
      is_device_deprovisioned_(false) {
  if (!delegate_->IsAffiliated(profile_)) {
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

        instance->telemetry_report_queue_ =
            instance->delegate_->CreatePeriodicUploadReportQueue(
                EventType::kUser, Destination::TELEMETRY_METRIC,
                Priority::MANUAL_BATCH_LACROS,
                instance->device_reporting_settings_.get(),
                ::policy::key::kReportUploadFrequency,
                GetDefaultReportUploadFrequency());

        instance->delegate_->RegisterObserverWithCrosApiClient(instance.get());
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
  samplers_.clear();
  periodic_collectors_.clear();
  telemetry_report_queue_.reset();
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
      g_browser_process->network_quality_tracker(), profile_);

  // Network bandwidth telemetry.
  InitPeriodicCollector(
      std::move(network_bandwidth_sampler), telemetry_report_queue_.get(),
      /*enable_setting_path=*/::policy::key::kReportDeviceNetworkStatus,
      kReportDeviceNetworkStatusDefaultValue,
      ::policy::key::kReportDeviceNetworkTelemetryCollectionRateMs,
      GetDefaultCollectionRate(kDefaultNetworkTelemetryCollectionRate));
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

void MetricReportingManagerLacros::UploadTelemetry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!telemetry_report_queue_) {
    return;
  }
  telemetry_report_queue_->Upload();
}

// static
void MetricReportingManagerLacros::EnsureFactoryBuilt() {
  MetricReportingManagerLacrosFactory::EnsureFactoryBuilt();
}

}  // namespace reporting::metrics
