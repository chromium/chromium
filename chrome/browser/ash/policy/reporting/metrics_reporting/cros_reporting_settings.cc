// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_reporting_settings.h"

#include "base/notreached.h"
#include "base/values.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"

namespace reporting {
base::CallbackListSubscription CrosReportingSettings::AddSettingsObserver(
    const std::string& path,
    base::RepeatingClosure callback) {
  return ash::CrosSettings::Get()->AddSettingsObserver(path,
                                                       std::move(callback));
}

bool CrosReportingSettings::PrepareTrustedValues(base::OnceClosure callback) {
  return ash::CrosSettings::Get()->PrepareTrustedValues(std::move(callback)) ==
         ash::CrosSettingsProvider::TRUSTED;
}

bool CrosReportingSettings::GetBoolean(const std::string& path,
                                       bool* out_value) const {
  return ash::CrosSettings::Get()->GetBoolean(path, out_value);
}

bool CrosReportingSettings::GetInteger(const std::string& path,
                                       int* out_value) const {
  return ash::CrosSettings::Get()->GetInteger(path, out_value);
}

bool CrosReportingSettings::GetList(const std::string& path,
                                    const base::Value::List** out_value) const {
  return ::ash::CrosSettings::Get()->GetList(path, out_value);
}

bool CrosReportingSettings::GetReportingEnabled(const std::string& path,
                                                bool* out_value) const {
  return GetBoolean(path, out_value);
}
}  // namespace reporting
