// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_REMOTE_TELEMETRY_MANAGEMENT_SERVICE_STRATEGY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_REMOTE_TELEMETRY_MANAGEMENT_SERVICE_STRATEGY_H_

#include <memory>

#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// A class that provides an interface for accessing a management service mojo
// remote. Allows for multiple implementations depending on whether this is
// running in Ash or LaCros.
class RemoteTelemetryManagementServiceStrategy {
 public:
  static std::unique_ptr<RemoteTelemetryManagementServiceStrategy> Create();

  RemoteTelemetryManagementServiceStrategy(
      const RemoteTelemetryManagementServiceStrategy&) = delete;
  RemoteTelemetryManagementServiceStrategy& operator=(
      const RemoteTelemetryManagementServiceStrategy&) = delete;
  virtual ~RemoteTelemetryManagementServiceStrategy();

  virtual mojo::Remote<crosapi::mojom::TelemetryManagementService>&
  GetRemoteService() = 0;

 protected:
  RemoteTelemetryManagementServiceStrategy();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_MANAGEMENT_REMOTE_TELEMETRY_MANAGEMENT_SERVICE_STRATEGY_H_
