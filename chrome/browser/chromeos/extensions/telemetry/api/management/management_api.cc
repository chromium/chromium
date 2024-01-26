// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/management/management_api.h"

#include <inttypes.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chromeos/extensions/api/management.h"
#include "chromeos/crosapi/mojom/telemetry_management_service.mojom.h"
#include "extensions/common/permissions/permissions_data.h"

namespace chromeos {

namespace {

namespace cx_manage = api::os_management;
namespace crosapi = ::crosapi::mojom;

}  // namespace

// ManagementApiFunctionBase ---------------------------------------------------

ManagementApiFunctionBase::ManagementApiFunctionBase()
    : remote_telemetry_management_service_strategy_(
          RemoteTelemetryManagementServiceStrategy::Create()) {}

ManagementApiFunctionBase::~ManagementApiFunctionBase() = default;

mojo::Remote<crosapi::TelemetryManagementService>&
ManagementApiFunctionBase::GetRemoteService() {
  DCHECK(remote_telemetry_management_service_strategy_);
  return remote_telemetry_management_service_strategy_->GetRemoteService();
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool ManagementApiFunctionBase::IsCrosApiAvailable() {
  return remote_telemetry_management_service_strategy_ != nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

template <class Params>
std::optional<Params> ManagementApiFunctionBase::GetParams() {
  auto params = Params::Create(args());
  if (!params) {
    SetBadMessage();
    Respond(BadMessage());
  }

  return params;
}

// OsManagementSetAudioGainFunction --------------------------------------------

void OsManagementSetAudioGainFunction::RunIfAllowed() {
  // Clamping |gain| to [0, 100] is done in Telemetry Extension Service
  // `TelemetryManagementServiceAsh::SetAudioGain`.
  const auto params = GetParams<cx_manage::SetAudioGain::Params>();
  if (!params) {
    return;
  }

  auto cb = base::BindOnce(&OsManagementSetAudioGainFunction::OnResult, this);
  GetRemoteService()->SetAudioGain(params.value().args.node_id,
                                   params.value().args.gain, std::move(cb));
}

void OsManagementSetAudioGainFunction::OnResult(bool is_success) {
  Respond(WithArguments(is_success));
}

// OsManagementSetAudioVolumeFunction ------------------------------------------

void OsManagementSetAudioVolumeFunction::RunIfAllowed() {
  // Clamping |volume| to [0, 100] is done in Telemetry Extension Service
  // `TelemetryManagementServiceAsh::SetAudioVolume`.
  const auto params = GetParams<cx_manage::SetAudioVolume::Params>();
  if (!params) {
    return;
  }

  auto cb = base::BindOnce(&OsManagementSetAudioVolumeFunction::OnResult, this);
  GetRemoteService()->SetAudioVolume(
      params.value().args.node_id, params.value().args.volume,
      params.value().args.is_muted, std::move(cb));
}

void OsManagementSetAudioVolumeFunction::OnResult(bool is_success) {
  Respond(WithArguments(is_success));
}

}  // namespace chromeos
