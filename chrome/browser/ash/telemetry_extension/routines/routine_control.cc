// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/telemetry_extension/routines/routine_control.h"

#include <utility>

#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_routines.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

namespace {

namespace crosapi = crosapi::mojom;
namespace healthd = cros_healthd::mojom;

}  // namespace

CrosHealthdRoutineControl::CrosHealthdRoutineControl(
    mojo::PendingRemote<healthd::RoutineControl> pending_remote)
    : remote_(std::move(pending_remote)) {}

CrosHealthdRoutineControl::~CrosHealthdRoutineControl() = default;

mojo::Remote<healthd::RoutineControl>& CrosHealthdRoutineControl::GetRemote() {
  return remote_;
}

}  // namespace ash
