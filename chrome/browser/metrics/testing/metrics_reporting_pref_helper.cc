// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/testing/metrics_reporting_pref_helper.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/device_settings_cache.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace {

void SetMetricsReportingEnabledChromeOS(bool is_enabled,
                                        base::Value::Dict& local_state_dict) {
  namespace em = enterprise_management;
  em::ChromeDeviceSettingsProto device_settings_proto;
  device_settings_proto.mutable_metrics_enabled()->set_metrics_enabled(
      is_enabled);
  em::PolicyData policy_data;
  policy_data.set_policy_type("google/chromeos/device");
  policy_data.set_policy_value(device_settings_proto.SerializeAsString());
  local_state_dict.Set(
      prefs::kDeviceSettingsCache,
      ash::device_settings_cache::PolicyDataToString(policy_data));
}

}  // namespace
#endif

namespace metrics {

base::FilePath SetUpUserDataDirectoryForTesting(bool is_enabled) {
  base::Value::Dict local_state_dict;
  local_state_dict.SetByDottedPath(metrics::prefs::kMetricsReportingEnabled,
                                   is_enabled);

  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return base::FilePath();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS checks a separate place for reporting enabled.
  SetMetricsReportingEnabledChromeOS(is_enabled, local_state_dict);
#endif

  base::FilePath local_state_path =
      user_data_dir.Append(chrome::kLocalStateFilename);
  if (!JSONFileValueSerializer(local_state_path).Serialize(local_state_dict))
    return base::FilePath();
  return local_state_path;
}

}  // namespace metrics
