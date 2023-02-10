// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/keymaster/arc_keymaster_bridge.h"

#include <cstdint>
#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/process/process_handle.h"
#include "chromeos/ash/components/dbus/arc/arc_keymaster_client.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"

namespace arc {
namespace {

// Singleton factory for ArcKeymasterBridge
class ArcKeymasterBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcKeymasterBridge,
          ArcKeymasterBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcKeymasterBridgeFactory";

  static ArcKeymasterBridgeFactory* GetInstance() {
    return base::Singleton<ArcKeymasterBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcKeymasterBridgeFactory>;
  ArcKeymasterBridgeFactory() = default;
  ~ArcKeymasterBridgeFactory() override = default;
};

}  // namespace

// static
BrowserContextKeyedServiceFactory* ArcKeymasterBridge::GetFactory() {
  return ArcKeymasterBridgeFactory::GetInstance();
}

// static
ArcKeymasterBridge* ArcKeymasterBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcKeymasterBridgeFactory::GetForBrowserContext(context);
}

ArcKeymasterBridge::ArcKeymasterBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      cert_store_bridge_(std::make_unique<keymaster::CertStoreBridge>(context)),
      weak_factory_(this) {
  if (arc_bridge_service_)
    arc_bridge_service_->keymaster()->SetHost(this);
}

ArcKeymasterBridge::~ArcKeymasterBridge() {
  if (arc_bridge_service_)
    arc_bridge_service_->keymaster()->SetHost(nullptr);
}

void ArcKeymasterBridge::UpdatePlaceholderKeys(
    std::vector<keymaster::mojom::ChromeOsKeyPtr> keys,
    UpdatePlaceholderKeysCallback callback) {
  if (cert_store_bridge_->IsProxyBound()) {
    cert_store_bridge_->UpdatePlaceholderKeysInKeymaster(std::move(keys),
                                                         std::move(callback));
  } else {
    BootstrapMojoConnection(base::BindOnce(
        &ArcKeymasterBridge::UpdatePlaceholderKeysAfterBootstrap,
        weak_factory_.GetWeakPtr(), std::move(keys), std::move(callback)));
  }
}

void ArcKeymasterBridge::UpdatePlaceholderKeysAfterBootstrap(
    std::vector<keymaster::mojom::ChromeOsKeyPtr> keys,
    UpdatePlaceholderKeysCallback callback,
    bool bootstrapResult) {
  if (bootstrapResult) {
    cert_store_bridge_->UpdatePlaceholderKeysInKeymaster(std::move(keys),
                                                         std::move(callback));
  } else {
    std::move(callback).Run(/*success=*/false);
  }
}

void ArcKeymasterBridge::GetServer(GetServerCallback callback) {
  if (keymaster_server_proxy_.is_bound()) {
    std::move(callback).Run(keymaster_server_proxy_.Unbind());
  } else {
    BootstrapMojoConnection(
        base::BindOnce(&ArcKeymasterBridge::GetServerAfterBootstrap,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void ArcKeymasterBridge::GetServerAfterBootstrap(GetServerCallback callback,
                                                 bool bootstrapResult) {
  if (bootstrapResult)
    std::move(callback).Run(keymaster_server_proxy_.Unbind());
  else
    std::move(callback).Run(mojo::NullRemote());
}

void ArcKeymasterBridge::OnBootstrapMojoConnection(
    BootstrapMojoConnectionCallback callback,
    bool result) {
  if (result) {
    DVLOG(1) << "Success bootstrapping Mojo in arc-keymasterd.";
  } else {
    LOG(ERROR) << "Error bootstrapping Mojo in arc-keymasterd.";
    keymaster_server_proxy_.reset();
  }
  std::move(callback).Run(result);
}

void ArcKeymasterBridge::BootstrapMojoConnection(
    BootstrapMojoConnectionCallback callback) {
  DVLOG(1) << "Bootstrapping arc-keymasterd Mojo connection via D-Bus.";

  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe;
  if (mojo::core::IsMojoIpczEnabled()) {
    constexpr uint64_t kKeymasterPipeAttachment = 0;
    server_pipe = invitation.AttachMessagePipe(kKeymasterPipeAttachment);
  } else {
    server_pipe = invitation.AttachMessagePipe("arc-keymaster-pipe");
  }
  if (!server_pipe.is_valid()) {
    LOG(ERROR) << "ArcKeymasterBridge could not bind to invitation";
    std::move(callback).Run(false);
    return;
  }

  // Bootstrap cert_store channel attached to the same invitation.
  cert_store_bridge_->BindToInvitation(&invitation);

  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());

  keymaster_server_proxy_.Bind(
      mojo::PendingRemote<mojom::KeymasterServer>(std::move(server_pipe), 0u));
  DVLOG(1) << "Bound remote KeymasterServer interface to pipe.";
  keymaster_server_proxy_.set_disconnect_handler(
      base::BindOnce(&mojo::Remote<mojom::KeymasterServer>::reset,
                     base::Unretained(&keymaster_server_proxy_)));
  ash::ArcKeymasterClient::Get()->BootstrapMojoConnection(
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
      base::BindOnce(&ArcKeymasterBridge::OnBootstrapMojoConnection,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

// static
void ArcKeymasterBridge::EnsureFactoryBuilt() {
  ArcKeymasterBridgeFactory::GetInstance();
}

}  // namespace arc
