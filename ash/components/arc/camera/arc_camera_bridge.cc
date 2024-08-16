// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/camera/arc_camera_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/dbus/arc/arc_camera_client.h"
#include "crypto/random.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Singleton factory for ArcCameraBridge.
class ArcCameraBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcCameraBridge,
          ArcCameraBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcCameraBridgeFactory";

  static ArcCameraBridgeFactory* GetInstance() {
    return base::Singleton<ArcCameraBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcCameraBridgeFactory>;
  ArcCameraBridgeFactory() = default;
  ~ArcCameraBridgeFactory() override = default;
};

}  // namespace

// Runs the callback after verifying the connection to the service.
class ArcCameraBridge::PendingStartCameraServiceResult {
 public:
  PendingStartCameraServiceResult(
      ArcCameraBridge* owner,
      mojo::ScopedMessagePipeHandle pipe,
      ArcCameraBridge::StartCameraServiceCallback callback)
      : owner_(owner),
        service_(
            mojo::PendingRemote<mojom::CameraService>(std::move(pipe), 0u)),
        callback_(std::move(callback)) {
    service_.set_disconnect_handler(
        base::BindOnce(&PendingStartCameraServiceResult::OnError,
                       weak_ptr_factory_.GetWeakPtr()));
    service_.QueryVersion(
        base::BindOnce(&PendingStartCameraServiceResult::OnVersionReady,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  PendingStartCameraServiceResult(const PendingStartCameraServiceResult&) =
      delete;
  PendingStartCameraServiceResult& operator=(
      const PendingStartCameraServiceResult&) = delete;

  ~PendingStartCameraServiceResult() = default;

 private:
  void OnVersionReady(uint32_t version) { Finish(); }

  void OnError() {
    LOG(ERROR) << "Failed to query the camera service version.";
    // Run the callback anyways. The same error will be delivered to the Android
    // side error handler.
    Finish();
  }

  // Runs the callback and removes this object from the owner.
  void Finish() {
    DCHECK(callback_);
    std::move(callback_).Run(service_.Unbind());
    // Destructs |this|.
    owner_->pending_start_camera_service_results_.erase(this);
  }

  const raw_ptr<ArcCameraBridge> owner_;
  mojo::Remote<mojom::CameraService> service_;
  ArcCameraBridge::StartCameraServiceCallback callback_;
  base::WeakPtrFactory<PendingStartCameraServiceResult> weak_ptr_factory_{this};
};

// static
ArcCameraBridge* ArcCameraBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcCameraBridgeFactory::GetForBrowserContext(context);
}

// static
ArcCameraBridge* ArcCameraBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcCameraBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcCameraBridge::ArcCameraBridge(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->camera()->SetHost(this);
}

ArcCameraBridge::~ArcCameraBridge() {
  arc_bridge_service_->camera()->SetHost(nullptr);
}

void ArcCameraBridge::StartCameraService(StartCameraServiceCallback callback) {
  uint8_t random_bytes[16];
  crypto::RandBytes(random_bytes);
  std::string token = base::HexEncode(random_bytes);

  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe =
      invitation.AttachMessagePipe(token);

  // Run the callback after verifying the connection to the service process.
  auto pending_result = std::make_unique<PendingStartCameraServiceResult>(
      this, std::move(server_pipe), std::move(callback));
  auto* pending_result_ptr = pending_result.get();
  pending_start_camera_service_results_[pending_result_ptr] =
      std::move(pending_result);

  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());
  base::ScopedFD fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();

  ash::ArcCameraClient::Get()->StartService(
      fd.get(), token, base::BindOnce([](bool success) {}));
}

void ArcCameraBridge::RegisterCameraHalClientLegacy(
    mojo::PendingRemote<cros::mojom::CameraHalClient> client) {
  NOTREACHED() << "ArcCameraBridge::RegisterCameraHalClientLegacy is "
                  "deprecated. CameraHalClient will not be registered.";
}

void ArcCameraBridge::RegisterCameraHalClient(
    mojo::PendingRemote<cros::mojom::CameraHalClient> client,
    RegisterCameraHalClientCallback callback) {
  auto* dispatcher = media::CameraHalDispatcherImpl::GetInstance();
  auto type = cros::mojom::CameraClientType::ANDROID;
  dispatcher->RegisterClientWithToken(
      std::move(client), type, dispatcher->GetTokenForTrustedClient(type),
      std::move(callback));
}

// static
void ArcCameraBridge::EnsureFactoryBuilt() {
  ArcCameraBridgeFactory::GetInstance();
}

}  // namespace arc
