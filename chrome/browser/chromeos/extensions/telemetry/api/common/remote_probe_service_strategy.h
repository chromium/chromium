// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_REMOTE_PROBE_SERVICE_STRATEGY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_REMOTE_PROBE_SERVICE_STRATEGY_H_

#include <memory>

#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// A class that provides an interface for accessing a probe service mojo remote.
// Allows for multiple implementations depending on whether this is running in
// Ash or LaCros.
class RemoteProbeServiceStrategy {
 public:
  static std::unique_ptr<RemoteProbeServiceStrategy> Create();

  RemoteProbeServiceStrategy();
  RemoteProbeServiceStrategy(const RemoteProbeServiceStrategy&) = delete;
  RemoteProbeServiceStrategy& operator=(const RemoteProbeServiceStrategy&) =
      delete;
  virtual ~RemoteProbeServiceStrategy();

  virtual mojo::Remote<crosapi::mojom::TelemetryProbeService>&
  GetRemoteService() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_REMOTE_PROBE_SERVICE_STRATEGY_H_
