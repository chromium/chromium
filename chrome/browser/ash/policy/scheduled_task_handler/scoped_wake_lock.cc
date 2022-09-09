// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/scoped_wake_lock.h"

#include "base/no_destructor.h"
#include "content/public/browser/device_service.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace policy {

namespace {

ScopedWakeLock::WakeLockProviderBinder& GetBinderOverride() {
  static base::NoDestructor<ScopedWakeLock::WakeLockProviderBinder> binder;
  return *binder;
}

}  // namespace

ScopedWakeLock::ScopedWakeLock(device::mojom::WakeLockType type,
                               const std::string& reason) {
  mojo::Remote<device::mojom::WakeLockProvider> provider;
  auto receiver = provider.BindNewPipeAndPassReceiver();
  const auto& binder = GetBinderOverride();
  if (binder)
    binder.Run(std::move(receiver));
  else
    content::GetDeviceService().BindWakeLockProvider(std::move(receiver));
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

// static
void ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
    WakeLockProviderBinder binder) {
  GetBinderOverride() = std::move(binder);
}

}  // namespace policy
