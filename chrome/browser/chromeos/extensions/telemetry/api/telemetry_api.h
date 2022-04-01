// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_API_H_

#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "ash/webui/telemetry_extension_ui/services/probe_service.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_api_guard_function.h"
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

  mojo::Remote<ash::health::mojom::ProbeService> remote_probe_service_;

 private:
  ash::ProbeService probe_service_;
};

class OsTelemetryGetVpdInfoFunction : public TelemetryApiFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getVpdInfo", OS_TELEMETRY_GETVPDINFO)

  OsTelemetryGetVpdInfoFunction();
  OsTelemetryGetVpdInfoFunction(const OsTelemetryGetVpdInfoFunction&) = delete;
  OsTelemetryGetVpdInfoFunction& operator=(
      const OsTelemetryGetVpdInfoFunction&) = delete;

 private:
  ~OsTelemetryGetVpdInfoFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(ash::health::mojom::TelemetryInfoPtr ptr);
};

class OsTelemetryGetOemDataFunction : public TelemetryApiFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getOemData", OS_TELEMETRY_GETOEMDATA)

  OsTelemetryGetOemDataFunction();
  OsTelemetryGetOemDataFunction(const OsTelemetryGetOemDataFunction&) = delete;
  OsTelemetryGetOemDataFunction& operator=(
      const OsTelemetryGetOemDataFunction&) = delete;

 private:
  ~OsTelemetryGetOemDataFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(ash::health::mojom::OemDataPtr ptr);
};

class OsTelemetryGetMemoryInfoFunction : public TelemetryApiFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getMemoryInfo",
                             OS_TELEMETRY_GETMEMORYINFO)

  OsTelemetryGetMemoryInfoFunction();
  OsTelemetryGetMemoryInfoFunction(const OsTelemetryGetMemoryInfoFunction&) =
      delete;
  OsTelemetryGetMemoryInfoFunction& operator=(
      const OsTelemetryGetMemoryInfoFunction&) = delete;

 private:
  ~OsTelemetryGetMemoryInfoFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(ash::health::mojom::TelemetryInfoPtr ptr);
};

class OsTelemetryGetCpuInfoFunction : public TelemetryApiFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getCpuInfo", OS_TELEMETRY_GETCPUINFO)

  OsTelemetryGetCpuInfoFunction();
  OsTelemetryGetCpuInfoFunction(const OsTelemetryGetCpuInfoFunction&) = delete;
  OsTelemetryGetCpuInfoFunction& operator=(
      const OsTelemetryGetCpuInfoFunction&) = delete;

 private:
  ~OsTelemetryGetCpuInfoFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(ash::health::mojom::TelemetryInfoPtr ptr);
};

class OsTelemetryGetBatteryInfoFunction : public TelemetryApiFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("os.telemetry.getBatteryInfo",
                             OS_TELEMETRY_GETBATTERYINFO)

  OsTelemetryGetBatteryInfoFunction();
  OsTelemetryGetBatteryInfoFunction(const OsTelemetryGetBatteryInfoFunction&) =
      delete;
  OsTelemetryGetBatteryInfoFunction& operator=(
      const OsTelemetryGetBatteryInfoFunction&) = delete;

 private:
  ~OsTelemetryGetBatteryInfoFunction() override;

  // BaseTelemetryExtensionApiGuardFunction:
  void RunIfAllowed() override;

  void OnResult(ash::health::mojom::TelemetryInfoPtr ptr);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_API_H_
