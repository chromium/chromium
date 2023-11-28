// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/appfuse/arc_appfuse_bridge.h"

#include <sys/epoll.h>

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "chromeos/ash/components/dbus/arc/arc_appfuse_provider_client.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Singleton factory for ArcAppfuseBridge.
class ArcAppfuseBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcAppfuseBridge,
          ArcAppfuseBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcAppfuseBridgeFactory";

  static ArcAppfuseBridgeFactory* GetInstance() {
    return base::Singleton<ArcAppfuseBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcAppfuseBridgeFactory>;
  ArcAppfuseBridgeFactory() = default;
  ~ArcAppfuseBridgeFactory() override = default;
};

void RunWithScopedHandle(base::OnceCallback<void(mojo::ScopedHandle)> callback,
                         std::optional<base::ScopedFD> fd) {
  if (!fd || !fd.value().is_valid()) {
    LOG(ERROR) << "Invalid FD: fd.has_value() = " << fd.has_value();
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  mojo::ScopedHandle wrapped_handle =
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(fd.value())));
  if (!wrapped_handle.is_valid()) {
    LOG(ERROR) << "Failed to wrap handle";
    std::move(callback).Run(mojo::ScopedHandle());
    return;
  }
  std::move(callback).Run(std::move(wrapped_handle));
}

}  // namespace

// static
ArcAppfuseBridge* ArcAppfuseBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcAppfuseBridgeFactory::GetForBrowserContext(context);
}

// static
ArcAppfuseBridge* ArcAppfuseBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcAppfuseBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcAppfuseBridge::ArcAppfuseBridge(content::BrowserContext* context,
                                   ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->appfuse()->SetHost(this);
}

ArcAppfuseBridge::~ArcAppfuseBridge() {
  arc_bridge_service_->appfuse()->SetHost(nullptr);
}

void ArcAppfuseBridge::Mount(uint32_t uid,
                             int32_t mount_id,
                             MountCallback callback) {
  // This is safe because ArcAppfuseProviderClient outlives ArcServiceLauncher.
  ash::ArcAppfuseProviderClient::Get()->Mount(
      uid, mount_id, base::BindOnce(&RunWithScopedHandle, std::move(callback)));
}

void ArcAppfuseBridge::Unmount(uint32_t uid,
                               int32_t mount_id,
                               UnmountCallback callback) {
  ash::ArcAppfuseProviderClient::Get()->Unmount(uid, mount_id,
                                                std::move(callback));
}

void ArcAppfuseBridge::OpenFile(uint32_t uid,
                                int32_t mount_id,
                                int32_t file_id,
                                int32_t flags,
                                OpenFileCallback callback) {
  ash::ArcAppfuseProviderClient::Get()->OpenFile(
      uid, mount_id, file_id, flags,
      base::BindOnce(&RunWithScopedHandle, std::move(callback)));
}

// static
void ArcAppfuseBridge::EnsureFactoryBuilt() {
  ArcAppfuseBridgeFactory::GetInstance();
}

}  // namespace arc
