// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_REMOTE_PROBE_SERVICE_STRATEGY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_REMOTE_PROBE_SERVICE_STRATEGY_H_

#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// A class that provides an interface for accessing a probe service mojo remote.
// Allows for multiple implementations depending on whether this is running in
// Ash or LaCros.
class RemoteProbeServiceStrategy {
 public:
  static RemoteProbeServiceStrategy* Get();
  RemoteProbeServiceStrategy(const RemoteProbeServiceStrategy&) = delete;
  RemoteProbeServiceStrategy& operator=(const RemoteProbeServiceStrategy&) =
      delete;
  virtual ~RemoteProbeServiceStrategy();

  // Returns the probe service currently enabled in this platform.
  crosapi::mojom::TelemetryProbeService* GetRemoteProbeService();

  // Override platform service to use a fake service, should only be called in
  // test.
  void SetServiceForTesting(
      mojo::PendingRemote<crosapi::mojom::TelemetryProbeService> test_service);

 private:
  RemoteProbeServiceStrategy();

  // Store the test service remote for Ash.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  mojo::Remote<crosapi::mojom::TelemetryProbeService> test_service_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_COMMON_REMOTE_PROBE_SERVICE_STRATEGY_H_
