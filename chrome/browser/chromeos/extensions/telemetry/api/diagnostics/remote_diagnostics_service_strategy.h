// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_REMOTE_DIAGNOSTICS_SERVICE_STRATEGY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_REMOTE_DIAGNOSTICS_SERVICE_STRATEGY_H_

#include <memory>

#include "chromeos/crosapi/mojom/diagnostics_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// An interface for accessing a diagnostics service mojo remote. Allows for
// multiple implementations depending on whether this is running in Ash or
// LaCros.
class RemoteDiagnosticsServiceStrategy {
 public:
  static std::unique_ptr<RemoteDiagnosticsServiceStrategy> Create();

  RemoteDiagnosticsServiceStrategy(const RemoteDiagnosticsServiceStrategy&) =
      delete;
  RemoteDiagnosticsServiceStrategy& operator=(
      const RemoteDiagnosticsServiceStrategy&) = delete;
  virtual ~RemoteDiagnosticsServiceStrategy();

  virtual mojo::Remote<crosapi::mojom::DiagnosticsService>&
  GetRemoteService() = 0;

 protected:
  RemoteDiagnosticsServiceStrategy();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_DIAGNOSTICS_REMOTE_DIAGNOSTICS_SERVICE_STRATEGY_H_
