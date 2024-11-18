// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/keymint/arc_keymint_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ash/arc/keymint/cert_store_bridge_keymint.h"
#include "chromeos/ash/components/dbus/arc/arc_keymint_client.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/platform/platform_channel.h"

namespace arc {

namespace {

// Singleton factory for ArcKeyMintBridge
class ArcKeyMintBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcKeyMintBridge,
          ArcKeyMintBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcKeyMintBridgeFactory";

  static ArcKeyMintBridgeFactory* GetInstance() {
    return base::Singleton<ArcKeyMintBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcKeyMintBridgeFactory>;
  ArcKeyMintBridgeFactory() = default;
  ~ArcKeyMintBridgeFactory() override = default;
};

}  // namespace

// static
BrowserContextKeyedServiceFactory* ArcKeyMintBridge::GetFactory() {
  return ArcKeyMintBridgeFactory::GetInstance();
}

// static
ArcKeyMintBridge* ArcKeyMintBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcKeyMintBridgeFactory::GetForBrowserContext(context);
}

ArcKeyMintBridge::ArcKeyMintBridge(content::BrowserContext* context,
                                   ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      cert_store_bridge_(
          std::make_unique<keymint::CertStoreBridgeKeyMint>(context)),
      weak_factory_(this) {
  if (arc_bridge_service_) {
    arc_bridge_service_->keymint()->SetHost(this);
  }
}

ArcKeyMintBridge::~ArcKeyMintBridge() {
  if (arc_bridge_service_) {
    arc_bridge_service_->keymint()->SetHost(nullptr);
  }
}

void ArcKeyMintBridge::UpdatePlaceholderKeys(
    std::vector<keymint::mojom::ChromeOsKeyPtr> keys,
    UpdatePlaceholderKeysCallback callback) {
  if (cert_store_bridge_->IsProxyBound()) {
    cert_store_bridge_->UpdatePlaceholderKeysInKeyMint(std::move(keys),
                                                       std::move(callback));
  } else {
    BootstrapMojoConnection(base::BindOnce(
        &ArcKeyMintBridge::UpdatePlaceholderKeysAfterBootstrap,
        weak_factory_.GetWeakPtr(), std::move(keys), std::move(callback)));
  }
}

void ArcKeyMintBridge::UpdatePlaceholderKeysAfterBootstrap(
    std::vector<keymint::mojom::ChromeOsKeyPtr> keys,
    UpdatePlaceholderKeysCallback callback,
    bool bootstrapResult) {
  if (bootstrapResult) {
    cert_store_bridge_->UpdatePlaceholderKeysInKeyMint(std::move(keys),
                                                       std::move(callback));
  } else {
    std::move(callback).Run(/*success=*/false);
  }
}

void ArcKeyMintBridge::GetServer(GetServerCallback callback) {
  if (keymint_server_proxy_.is_bound()) {
    std::move(callback).Run(keymint_server_proxy_.Unbind());
  } else {
    BootstrapMojoConnection(
        base::BindOnce(&ArcKeyMintBridge::GetServerAfterBootstrap,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }
}

void ArcKeyMintBridge::GetServerAfterBootstrap(GetServerCallback callback,
                                               bool bootstrapResult) {
  if (bootstrapResult) {
    std::move(callback).Run(keymint_server_proxy_.Unbind());
  } else {
    std::move(callback).Run(mojo::NullRemote());
  }
}

void ArcKeyMintBridge::OnBootstrapMojoConnection(
    BootstrapMojoConnectionCallback callback,
    bool result) {
  if (result) {
    DVLOG(1) << "Success bootstrapping Mojo in arc-keymintd.";
  } else {
    LOG(ERROR) << "Error bootstrapping Mojo in arc-keymintd.";
    keymint_server_proxy_.reset();
  }
  std::move(callback).Run(result);
}

void ArcKeyMintBridge::BootstrapMojoConnection(
    BootstrapMojoConnectionCallback callback) {
  DVLOG(1) << "Bootstrapping arc-keymintd Mojo connection via D-Bus.";

  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe;
  if (mojo::core::IsMojoIpczEnabled()) {
    constexpr uint64_t kKeyMintPipeAttachment = 0;
    server_pipe = invitation.AttachMessagePipe(kKeyMintPipeAttachment);
  } else {
    server_pipe = invitation.AttachMessagePipe("arc-keymint-pipe");
  }
  if (!server_pipe.is_valid()) {
    LOG(ERROR) << "ArcKeyMintBridge could not bind to invitation";
    std::move(callback).Run(false);
    return;
  }

  // Bootstrap cert_store channel attached to the same invitation.
  cert_store_bridge_->BindToInvitation(&invitation);

  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());

  keymint_server_proxy_.Bind(mojo::PendingRemote<mojom::keymint::KeyMintServer>(
      std::move(server_pipe), 0u));
  DVLOG(1) << "Bound remote KeyMintServer interface to pipe.";
  keymint_server_proxy_.set_disconnect_handler(
      base::BindOnce(&mojo::Remote<mojom::keymint::KeyMintServer>::reset,
                     base::Unretained(&keymint_server_proxy_)));
  ash::ArcKeyMintClient::Get()->BootstrapMojoConnection(
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD(),
      base::BindOnce(&ArcKeyMintBridge::OnBootstrapMojoConnection,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

// static
void ArcKeyMintBridge::EnsureFactoryBuilt() {
  ArcKeyMintBridgeFactory::GetInstance();
}

}  // namespace arc
