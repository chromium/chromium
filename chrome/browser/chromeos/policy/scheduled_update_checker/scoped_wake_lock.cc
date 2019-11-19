// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/scheduled_update_checker/scoped_wake_lock.h"

#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace policy {

ScopedWakeLock::ScopedWakeLock(service_manager::Connector* connector,
                               device::mojom::WakeLockType type,
                               const std::string& reason) {
  mojo::Remote<device::mojom::WakeLockProvider> provider;
  connector->Connect(device::mojom::kServiceName,
                     provider.BindNewPipeAndPassReceiver());
  provider->GetWakeLockWithoutContext(
      type, device::mojom::WakeLockReason::kOther, reason,
      wake_lock_.BindNewPipeAndPassReceiver());
  // This would violate |GetWakeLockWithoutContext|'s API contract.
  DCHECK(wake_lock_);
  wake_lock_->RequestWakeLock();
}

ScopedWakeLock::~ScopedWakeLock() {
  // |wake_lock_| could have been moved.
  if (wake_lock_)
    wake_lock_->CancelWakeLock();
}

ScopedWakeLock::ScopedWakeLock(ScopedWakeLock&& other) = default;

ScopedWakeLock& ScopedWakeLock::operator=(ScopedWakeLock&& other) = default;

}  // namespace policy
