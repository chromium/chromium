// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_REMOTE_EVENT_SERVICE_STRATEGY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_REMOTE_EVENT_SERVICE_STRATEGY_H_

#include <memory>

#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

// An interface for accessing an events service mojo remote. Allows for
// multiple implementations depending on whether this is running in Ash or
// LaCros.
class RemoteEventServiceStrategy {
 public:
  static std::unique_ptr<RemoteEventServiceStrategy> Create();

  RemoteEventServiceStrategy(const RemoteEventServiceStrategy&) = delete;
  RemoteEventServiceStrategy& operator=(const RemoteEventServiceStrategy&) =
      delete;
  virtual ~RemoteEventServiceStrategy();

  virtual mojo::Remote<crosapi::mojom::TelemetryEventService>&
  GetRemoteService() = 0;

 protected:
  RemoteEventServiceStrategy();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_EVENTS_REMOTE_EVENT_SERVICE_STRATEGY_H_
