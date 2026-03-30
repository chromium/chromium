// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/system/fake_system_routine_controller_delegate.h"

#include "base/check.h"

namespace ash::diagnostics {

FakeSystemRoutineControllerDelegate::FakeSystemRoutineControllerDelegate() =
    default;

FakeSystemRoutineControllerDelegate::~FakeSystemRoutineControllerDelegate() =
    default;

void FakeSystemRoutineControllerDelegate::SetGoogleServicesConnectivityResult(
    chromeos::network_diagnostics::mojom::RoutineResultPtr result) {
  result_ = std::move(result);
}

void FakeSystemRoutineControllerDelegate::RunHeldCallback() {
  CHECK(held_callback_) << "No callback is held";
  std::move(held_callback_).Run(result_.Clone());
}

bool FakeSystemRoutineControllerDelegate::has_held_callback() const {
  return !held_callback_.is_null();
}

void FakeSystemRoutineControllerDelegate::RunGoogleServicesConnectivity(
    RunGoogleServicesConnectivityCallback callback) {
  if (hold_callback_) {
    held_callback_ = std::move(callback);
    return;
  }
  std::move(callback).Run(result_.Clone());
}

}  // namespace ash::diagnostics
