// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/remote_probe_service_strategy.h"

#include <memory>

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chromeos/ash/components/telemetry_extension/telemetry/probe_service_ash.h"
#include "chromeos/crosapi/mojom/probe_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {

RemoteProbeServiceStrategy* g_instance = nullptr;
}

RemoteProbeServiceStrategy::RemoteProbeServiceStrategy() = default;
RemoteProbeServiceStrategy::~RemoteProbeServiceStrategy() = default;

RemoteProbeServiceStrategy* RemoteProbeServiceStrategy::Get() {
  if (!g_instance) {
    g_instance = new RemoteProbeServiceStrategy();
  }
  return g_instance;
}

// Returns the probe service currently enabled in this platform.
crosapi::mojom::TelemetryProbeService*
RemoteProbeServiceStrategy::GetRemoteProbeService() {
  if (test_service_) {
    return test_service_.get();
  }
  return crosapi::CrosapiManager::Get()->crosapi_ash()->probe_service_ash();
}

// Override platform service to use a fake service, should only be called in
// test. We cannot override Ash service since the ash crosapi manager does not
// exist in test.
void RemoteProbeServiceStrategy::SetServiceForTesting(
    mojo::PendingRemote<crosapi::mojom::TelemetryProbeService> test_service) {
  if (test_service_.is_bound()) {
    test_service_.reset();
  }
  test_service_.Bind(std::move(test_service));
}

}  // namespace chromeos
