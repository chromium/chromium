// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_LACROS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/reporting/device_reporting_settings_lacros.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_delegate_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/metric_report_queue.h"

static_assert(BUILDFLAG(IS_CHROMEOS_LACROS), "For Lacros only");

namespace reporting::metrics {

// Manages the initialization, collection and reporting of certain user event,
// info and telemetry metrics in Lacros.
class MetricReportingManagerLacros : public KeyedService,
                                     public DeviceSettingsLacros::Observer {
 public:
  // Delegate that implements certain functional components (like device
  // deprovisioning checks, etc.) required by the reporting manager and can be
  // stubbed for testing purposes.
  class Delegate : public MetricReportingManagerDelegateBase {
   public:
    Delegate() = default;
    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;
    ~Delegate() override = default;

    // Checks if the device is deprovisioned via crosapi and triggers the
    // supplied callback with the corresponding result.
    virtual void CheckDeviceDeprovisioned(
        crosapi::mojom::DeviceSettingsService::IsDeviceDeprovisionedCallback
            callback);

    // Creates a relevant `DeviceReportingSettingsLacros` instance that can be
    // used by this reporting manager to fetch device reporting settings.
    virtual std::unique_ptr<DeviceReportingSettingsLacros>
    CreateDeviceReportingSettings();

    // Registers the specified instance so it can start listening to device
    // setting updates.
    virtual void RegisterObserverWithCrosApiClient(
        MetricReportingManagerLacros* const instance);
  };

  // Retrieves the `MetricReportingManagerLacros` for the given
  // profile.
  static MetricReportingManagerLacros* GetForProfile(Profile* profile);

  MetricReportingManagerLacros(
      Profile* profile,
      std::unique_ptr<MetricReportingManagerLacros::Delegate> delegate);
  MetricReportingManagerLacros(const MetricReportingManagerLacros& other) =
      delete;
  MetricReportingManagerLacros& operator=(
      const MetricReportingManagerLacros& other) = delete;
  ~MetricReportingManagerLacros() override;

  // DeviceSettingsLacros::Observer:
  void OnDeviceSettingsUpdated() override;

 private:
  void Shutdown() override;

  // Init collectors that need to start on startup after a delay, should
  // only be scheduled once on construction.
  void DelayedInit();

  void InitNetworkCollectors();

  void InitPeriodicCollector(std::unique_ptr<Sampler> sampler,
                             MetricReportQueue* metric_report_queue,
                             const std::string& enable_setting_path,
                             bool setting_enabled_default_value,
                             const std::string& rate_setting_path,
                             base::TimeDelta default_rate,
                             int rate_unit_to_ms = 1);

  // Flushes all enqueued telemetry data and uploads them.
  void UploadTelemetry();

  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<Profile> profile_;
  const std::unique_ptr<MetricReportingManagerLacros::Delegate> delegate_;
  const std::unique_ptr<DeviceReportingSettingsLacros>
      device_reporting_settings_;

  std::vector<std::unique_ptr<Sampler>> samplers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::vector<std::unique_ptr<CollectorBase>> periodic_collectors_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<MetricReportQueue> telemetry_report_queue_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Cache device deprovisioned state so we minimize crosapi calls. Updated on
  // device settings updates.
  bool is_device_deprovisioned_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::OneShotTimer delayed_init_timer_;
  base::OneShotTimer initial_upload_timer_;

  base::WeakPtrFactory<MetricReportingManagerLacros> weak_ptr_factory_{this};
};
}  // namespace reporting::metrics

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_LACROS_H_
