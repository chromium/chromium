// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_API_H_

#include "chromeos/components/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "chromeos/components/telemetry_extension_ui/services/probe_service.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class TelemetryApiFunctionBase : public ExtensionFunction {
 public:
  TelemetryApiFunctionBase();

  TelemetryApiFunctionBase(const TelemetryApiFunctionBase&) = delete;
  TelemetryApiFunctionBase& operator=(const TelemetryApiFunctionBase&) = delete;

 protected:
  ~TelemetryApiFunctionBase() override;

  mojo::Remote<health::mojom::ProbeService> remote_probe_service_;

 private:
  ProbeService probe_service_;
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

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnResult(health::mojom::TelemetryInfoPtr ptr);
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

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnResult(health::mojom::OemDataPtr ptr);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_TELEMETRY_API_H_
