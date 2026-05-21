// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_MANAGEMENT_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_MANAGEMENT_API_H_

#include <optional>

#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_api_guard_function.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace chromeos {

class ManagementApiFunctionBase
    : public BaseTelemetryExtensionApiGuardFunction {
 public:
  ManagementApiFunctionBase();

  ManagementApiFunctionBase(const ManagementApiFunctionBase&) = delete;
  ManagementApiFunctionBase& operator=(const ManagementApiFunctionBase&) =
      delete;

 protected:
  ~ManagementApiFunctionBase() override;

  // Gets the parameters passed to the JavaScript call and tries to convert it
  // to the `Params` type. If the `Params` can't be created, this resolves the
  // corresponding JavaScript call with an error and returns `nullptr`.
  template <class Params>
  std::optional<Params> GetParams();
};

class OsManagementSetAudioGainFunction : public ManagementApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.management.setAudioGain",
                             OS_MANAGEMENT_SETAUDIOGAIN)

 private:
  ~OsManagementSetAudioGainFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsManagementSetAudioVolumeFunction : public ManagementApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.management.setAudioVolume",
                             OS_MANAGEMENT_SETAUDIOVOLUME)

 private:
  ~OsManagementSetAudioVolumeFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_MANAGEMENT_API_H_
