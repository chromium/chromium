// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_REPORTING_SETTINGS_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_REPORTING_SETTINGS_H_

#include <string>

#include "base/callback_forward.h"
#include "base/callback_list.h"
#include "components/reporting/metrics/reporting_settings.h"

namespace reporting {

// Wrapper of `ash::CrosSettings` methods needed for telemetry collection and
// reporting.
class CrosReportingSettings : public ReportingSettings {
 public:
  CrosReportingSettings() = default;

  ~CrosReportingSettings() override = default;

  base::CallbackListSubscription AddSettingsObserver(
      const std::string& path,
      base::RepeatingClosure callback) override;

  bool PrepareTrustedValues(base::OnceClosure callback) override;

  bool GetBoolean(const std::string& path, bool* out_value) const override;
  bool GetInteger(const std::string& path, int* out_value) const override;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_CROS_REPORTING_SETTINGS_H_
