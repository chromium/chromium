// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_MANAGEMENT_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_MANAGEMENT_API_H_

#include <memory>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_api_guard_function.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/management/remote_telemetry_management_service_strategy.h"
#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "mojo/public/cpp/bindings/remote.h"

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

  mojo::Remote<crosapi::mojom::TelemetryManagementService>& GetRemoteService();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsCrosApiAvailable() override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Gets the parameters passed to the JavaScript call and tries to convert it
  // to the `Params` type. If the `Params` can't be created, this resolves the
  // corresponding JavaScript call with an error and returns `nullptr`.
  template <class Params>
  std::optional<Params> GetParams();

 private:
  std::unique_ptr<RemoteTelemetryManagementServiceStrategy>
      remote_telemetry_management_service_strategy_;
};

class OsManagementSetAudioGainFunction : public ManagementApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.management.setAudioGain",
                             OS_MANAGEMENT_SETAUDIOGAIN)

 private:
  ~OsManagementSetAudioGainFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(bool is_success);
};

class OsManagementSetAudioVolumeFunction : public ManagementApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.management.setAudioVolume",
                             OS_MANAGEMENT_SETAUDIOVOLUME)

 private:
  ~OsManagementSetAudioVolumeFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(bool is_success);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_MANAGEMENT_API_H_
