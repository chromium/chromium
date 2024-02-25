// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/wake_lock/arc_wake_lock_bridge.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "content/public/browser/device_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

namespace arc {

namespace {

constexpr char kWakeLockReason[] = "ARC";

// Singleton factory for ArcWakeLockBridge.
class ArcWakeLockBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcWakeLockBridge,
          ArcWakeLockBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcWakeLockBridgeFactory";

  static ArcWakeLockBridgeFactory* GetInstance() {
    return base::Singleton<ArcWakeLockBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcWakeLockBridgeFactory>;
  ArcWakeLockBridgeFactory() = default;
  ~ArcWakeLockBridgeFactory() override = default;
};

}  // namespace

// WakeLockRequester requests a wake lock from the device service in response
// to wake lock requests of a given type from Android. A count is kept of
// outstanding Android requests so that only a single actual wake lock is used.
class ArcWakeLockBridge::WakeLockRequester {
 public:
  WakeLockRequester(device::mojom::WakeLockType type,
                    device::mojom::WakeLockProvider* provider)
      : type_(type), provider_(provider) {}

  WakeLockRequester(const WakeLockRequester&) = delete;
  WakeLockRequester& operator=(const WakeLockRequester&) = delete;

  ~WakeLockRequester() = default;

  // Increments the number of outstanding requests from Android and requests a
  // wake lock from the device service if this is the only request.
  void AddRequest() {
    DCHECK_GE(wake_lock_count_, 0);
    wake_lock_count_++;
    if (wake_lock_count_ > 1) {
      DVLOG(1) << "Partial wake lock acquire. Count: " << wake_lock_count_;
      return;
    }

    // Initialize |wake_lock_| if this is the first time we're using it.
    DVLOG(1) << "Partial wake lock new acquire. Count: " << wake_lock_count_;
    if (!wake_lock_) {
      provider_->GetWakeLockWithoutContext(
          type_, device::mojom::WakeLockReason::kOther, kWakeLockReason,
          wake_lock_.BindNewPipeAndPassReceiver());
    }

    wake_lock_->RequestWakeLock();
  }

  // Decrements the number of outstanding Android requests. Cancels the device
  // service wake lock when the request count hits zero.
  void RemoveRequest() {
    DCHECK_GE(wake_lock_count_, 0);
    if (wake_lock_count_ == 0) {
      LOG(WARNING) << "Release without acquire. Count: " << wake_lock_count_;
      return;
    }

    wake_lock_count_--;
    if (wake_lock_count_ >= 1) {
      DVLOG(1) << "Partial wake release. Count: " << wake_lock_count_;
      return;
    }

    DCHECK(wake_lock_);
    DVLOG(1) << "Partial wake force release. Count: " << wake_lock_count_;
    wake_lock_->CancelWakeLock();
  }

  // Runs the message loop until replies have been received for all pending
  // requests on |wake_lock_|.
  void FlushForTesting() {
    if (wake_lock_)
      wake_lock_.FlushForTesting();
  }

 private:
  // Type of wake lock to request.
  const device::mojom::WakeLockType type_;

  // Used to get wake locks. Not owned.
  const raw_ptr<device::mojom::WakeLockProvider> provider_;

  // Number of outstanding Android requests.
  int64_t wake_lock_count_ = 0;

  // Lazily initialized in response to first request.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
};

// static
BrowserContextKeyedServiceFactory* ArcWakeLockBridge::GetFactory() {
  return ArcWakeLockBridgeFactory::GetInstance();
}

// static
ArcWakeLockBridge* ArcWakeLockBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcWakeLockBridgeFactory::GetForBrowserContext(context);
}

// static
ArcWakeLockBridge* ArcWakeLockBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcWakeLockBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcWakeLockBridge::ArcWakeLockBridge(content::BrowserContext* context,
                                     ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->wake_lock()->SetHost(this);
  arc_bridge_service_->wake_lock()->AddObserver(this);
}

ArcWakeLockBridge::~ArcWakeLockBridge() {
  arc_bridge_service_->wake_lock()->RemoveObserver(this);
  arc_bridge_service_->wake_lock()->SetHost(nullptr);
}

void ArcWakeLockBridge::OnConnectionClosed() {
  DVLOG(1) << "OnConnectionClosed";
  wake_lock_requesters_.clear();
}

void ArcWakeLockBridge::FlushWakeLocksForTesting() {
  for (const auto& it : wake_lock_requesters_)
    it.second->FlushForTesting();
}

void ArcWakeLockBridge::AcquirePartialWakeLock(
    AcquirePartialWakeLockCallback callback) {
  GetWakeLockRequester(device::mojom::WakeLockType::kPreventAppSuspension)
      ->AddRequest();
  std::move(callback).Run(true);
}

void ArcWakeLockBridge::ReleasePartialWakeLock(
    ReleasePartialWakeLockCallback callback) {
  GetWakeLockRequester(device::mojom::WakeLockType::kPreventAppSuspension)
      ->RemoveRequest();
  std::move(callback).Run(true);
}

ArcWakeLockBridge::WakeLockRequester* ArcWakeLockBridge::GetWakeLockRequester(
    device::mojom::WakeLockType type) {
  auto it = wake_lock_requesters_.find(type);
  if (it != wake_lock_requesters_.end())
    return it->second.get();

  if (!wake_lock_provider_) {
    content::GetDeviceService().BindWakeLockProvider(
        wake_lock_provider_.BindNewPipeAndPassReceiver());
  }

  it = wake_lock_requesters_
           .emplace(type, std::make_unique<WakeLockRequester>(
                              type, wake_lock_provider_.get()))
           .first;
  return it->second.get();
}

// static
void ArcWakeLockBridge::EnsureFactoryBuilt() {
  ArcWakeLockBridgeFactory::GetInstance();
}

}  // namespace arc
