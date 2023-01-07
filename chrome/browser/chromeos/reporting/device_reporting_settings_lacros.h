// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_DEVICE_REPORTING_SETTINGS_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_DEVICE_REPORTING_SETTINGS_LACROS_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/lacros/device_settings_lacros.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "components/reporting/metrics/reporting_settings.h"

namespace reporting {

// `DeviceReportingSettingsLacros` is used in Lacros to fetch device repprting
// settings from Ash via crosapi. It also facilitates components to subscribe to
// device reporting settings updates.
class DeviceReportingSettingsLacros : public ReportingSettings,
                                      public DeviceSettingsLacros::Observer {
 public:
  // Delegate that implements functionality around device settings retrieval via
  // crosapi. Also facilitates the observer to subscribe to device settings
  // updates by registering with the corresponding crosapi client
  // (`DeviceSettingsLacros`).
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;
    virtual ~Delegate() = default;

    // Registers the instance so it can subscribe to device setting updates.
    virtual void RegisterObserverWithCrosApiClient(
        DeviceReportingSettingsLacros* const instance);

    // Retrieves device settings.
    virtual crosapi::mojom::DeviceSettings* GetDeviceSettings();
  };

  // Helper to create a new instance of 'DeviceReportingSettingsLacros`.
  static std::unique_ptr<DeviceReportingSettingsLacros> Create();

  // Test helper to create new instance of `DeviceReportingSettingsLacros` using
  // the stubbed delegate.
  static std::unique_ptr<DeviceReportingSettingsLacros> CreateForTest(
      std::unique_ptr<Delegate> delegate);

  DeviceReportingSettingsLacros(const DeviceReportingSettingsLacros& other) =
      delete;
  DeviceReportingSettingsLacros& operator=(
      const DeviceReportingSettingsLacros& other) = delete;
  ~DeviceReportingSettingsLacros() override;

  // DeviceSettingsLacros::Observer:
  void OnDeviceSettingsUpdated() override;

  // ReportingSettings::
  base::CallbackListSubscription AddSettingsObserver(
      const std::string& policy_name,
      base::RepeatingClosure callback) override;

  // ReportingSettings::
  bool PrepareTrustedValues(base::OnceClosure callback) override;

  // ReportingSettings::
  bool GetBoolean(const std::string& policy_name,
                  bool* out_value) const override;
  bool GetInteger(const std::string& policy_name,
                  int* out_value) const override;

 private:
  explicit DeviceReportingSettingsLacros(std::unique_ptr<Delegate> delegate);

  // Retrieves corresponding device setting value for caching purposes,
  const base::Value GetDeviceSettingValueForCache(
      const std::string& policy_name);

  SEQUENCE_CHECKER(sequence_checker_);

  // A map from policy names to a list of observers. Observers get fired in
  // the order they are added.
  std::map<std::string, std::unique_ptr<base::RepeatingClosureList>>
      observer_map_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A map from policy name to the corresponding device setting value. This
  // cache keeps track of device setting values for registered observers and is
  // used to derive individual device setting updates so we can notify relevant
  // observers.
  std::map<std::string, base::Value> device_settings_cache_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<Delegate> delegate_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_DEVICE_REPORTING_SETTINGS_LACROS_H_
