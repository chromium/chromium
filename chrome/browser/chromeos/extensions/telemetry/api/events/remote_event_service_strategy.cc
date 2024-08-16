// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/events/remote_event_service_strategy.h"

#include <memory>

#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "chromeos/crosapi/mojom/telemetry_event_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/telemetry_extension/events/telemetry_event_service_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG (IS_CHROMEOS_LACROS)

namespace chromeos {

namespace {

namespace crosapi = ::crosapi::mojom;

#if BUILDFLAG(IS_CHROMEOS_ASH)
class RemoteEventServiceStrategyAsh : public RemoteEventServiceStrategy {
 public:
  RemoteEventServiceStrategyAsh()
      : event_service_(ash::TelemetryEventServiceAsh::Factory::Create(
            remote_event_service_.BindNewPipeAndPassReceiver())) {}

  ~RemoteEventServiceStrategyAsh() override = default;

  // RemoteEventServiceStrategy:
  mojo::Remote<crosapi::TelemetryEventService>& GetRemoteService() override {
    return remote_event_service_;
  }

 private:
  mojo::Remote<crosapi::TelemetryEventService> remote_event_service_;

  std::unique_ptr<crosapi::TelemetryEventService> event_service_;
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class RemoteEventServiceStrategyLacros : public RemoteEventServiceStrategy {
 public:
  RemoteEventServiceStrategyLacros() = default;

  ~RemoteEventServiceStrategyLacros() override = default;

  // RemoteEventServiceStrategy:
  mojo::Remote<crosapi::TelemetryEventService>& GetRemoteService() override {
    return LacrosService::Get()->GetRemote<crosapi::TelemetryEventService>();
  }
};
#endif  // BUILDFLAG (IS_CHROMEOS_LACROS)

}  // namespace

// static
std::unique_ptr<RemoteEventServiceStrategy>
RemoteEventServiceStrategy::Create() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<RemoteEventServiceStrategyAsh>();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!LacrosService::Get()->IsAvailable<crosapi::TelemetryEventService>()) {
    return nullptr;
  }
  return std::make_unique<RemoteEventServiceStrategyLacros>();
#else  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  NOTREACHED();
#endif
}

RemoteEventServiceStrategy::RemoteEventServiceStrategy() = default;
RemoteEventServiceStrategy::~RemoteEventServiceStrategy() = default;

}  // namespace chromeos
