// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_TELEMETRY_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_TELEMETRY_API_H_

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/base_telemetry_extension_api_guard_function.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/remote_probe_service_strategy.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class TelemetryApiFunctionBase : public BaseTelemetryExtensionApiGuardFunction {
 public:
  TelemetryApiFunctionBase();

  TelemetryApiFunctionBase(const TelemetryApiFunctionBase&) = delete;
  TelemetryApiFunctionBase& operator=(const TelemetryApiFunctionBase&) = delete;

 protected:
  ~TelemetryApiFunctionBase() override;

  crosapi::mojom::TelemetryProbeService* GetRemoteService();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsCrosApiAvailable() override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

class OsTelemetryGetAudioInfoFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getAudioInfo",
                             OS_TELEMETRY_GETAUDIOINFO)
  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);

 private:
  ~OsTelemetryGetAudioInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsTelemetryGetBatteryInfoFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getBatteryInfo",
                             OS_TELEMETRY_GETBATTERYINFO)
  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);

 private:
  ~OsTelemetryGetBatteryInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;
};

class OsTelemetryGetNonRemovableBlockDevicesInfoFunction
    : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getNonRemovableBlockDevicesInfo",
                             OS_TELEMETRY_GETNONREMOVABLEBLOCKDEVICESINFO)

 private:
  ~OsTelemetryGetNonRemovableBlockDevicesInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

class OsTelemetryGetCpuInfoFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getCpuInfo", OS_TELEMETRY_GETCPUINFO)

 private:
  ~OsTelemetryGetCpuInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

class OsTelemetryGetDisplayInfoFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getDisplayInfo",
                             OS_TELEMETRY_GETDISPLAYINFO)

 private:
  ~OsTelemetryGetDisplayInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

class OsTelemetryGetInternetConnectivityInfoFunction
    : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getInternetConnectivityInfo",
                             OS_TELEMETRY_GETINTERNETCONNECTIVITYINFO)

 private:
  ~OsTelemetryGetInternetConnectivityInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

class OsTelemetryGetMarketingInfoFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getMarketingInfo",
                             OS_TELEMETRY_GETMARKETINGINFO)

 private:
  ~OsTelemetryGetMarketingInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

class OsTelemetryGetMemoryInfoFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getMemoryInfo",
                             OS_TELEMETRY_GETMEMORYINFO)

 private:
  ~OsTelemetryGetMemoryInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

class OsTelemetryGetOemDataFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getOemData", OS_TELEMETRY_GETOEMDATA)

 private:
  ~OsTelemetryGetOemDataFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeOemDataPtr ptr);
};

class OsTelemetryGetOsVersionInfoFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getOsVersionInfo",
                             OS_TELEMETRY_GETOSVERSIONINFO)

 private:
  ~OsTelemetryGetOsVersionInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

class OsTelemetryGetStatefulPartitionInfoFunction
    : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getStatefulPartitionInfo",
                             OS_TELEMETRY_GETSTATEFULPARTITIONINFO)

 private:
  ~OsTelemetryGetStatefulPartitionInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

class OsTelemetryGetThermalInfoFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getThermalInfo",
                             OS_TELEMETRY_GETTHERMALINFO)

 private:
  ~OsTelemetryGetThermalInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

class OsTelemetryGetTpmInfoFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getTpmInfo", OS_TELEMETRY_GETTPMINFO)

 private:
  ~OsTelemetryGetTpmInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

class OsTelemetryGetUsbBusInfoFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getUsbBusInfo",
                             OS_TELEMETRY_GETUSBBUSINFO)

 private:
  ~OsTelemetryGetUsbBusInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

class OsTelemetryGetVpdInfoFunction : public TelemetryApiFunctionBase {
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getVpdInfo", OS_TELEMETRY_GETVPDINFO)

 private:
  ~OsTelemetryGetVpdInfoFunction() override = default;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(crosapi::mojom::ProbeTelemetryInfoPtr ptr);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_TELEMETRY_API_H_
