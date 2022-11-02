// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/keymaster/cert_store_bridge.h"

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/platform/platform_channel.h"

namespace arc {
namespace keymaster {

CertStoreBridge::CertStoreBridge(content::BrowserContext* context)
    : weak_ptr_factory_(this) {
  VLOG(2) << "CertStoreBridge::CertStoreBridge";
}

CertStoreBridge::~CertStoreBridge() {
  VLOG(2) << "CertStoreBridge::~CertStoreBridge";
}

void CertStoreBridge::UpdatePlaceholderKeysInKeymaster(
    std::vector<mojom::ChromeOsKeyPtr> keys,
    mojom::CertStoreInstance::UpdatePlaceholderKeysCallback callback) {
  VLOG(2) << "CertStoreBridge::UpdatePlaceholderKeysInKeymaster";
  if (cert_store_proxy_.is_bound()) {
    cert_store_proxy_->UpdatePlaceholderKeys(std::move(keys),
                                             std::move(callback));
  } else {
    LOG(ERROR) << "Tried to update placeholders but cert store is not bound";
    std::move(callback).Run(/*success=*/false);
  }
}

void CertStoreBridge::GetSecurityTokenOperation(
    mojo::PendingReceiver<mojom::SecurityTokenOperation> operation_receiver,
    GetSecurityTokenOperationCallback callback) {
  VLOG(2) << "CertStoreBridge::GetSecurityTokenOperation";
  std::move(callback).Run();
}

void CertStoreBridge::BindToInvitation(mojo::OutgoingInvitation* invitation) {
  VLOG(2) << "CertStoreBridge::BootstrapMojoConnection";

  mojo::ScopedMessagePipeHandle pipe;
  if (mojo::core::IsMojoIpczEnabled()) {
    constexpr uint64_t kCertStorePipeAttachment = 1;
    pipe = invitation->AttachMessagePipe(kCertStorePipeAttachment);
  } else {
    pipe = invitation->AttachMessagePipe("arc-cert-store-pipe");
  }

  if (!pipe.is_valid()) {
    LOG(ERROR) << "CertStoreBridge could not bind to invitation";
    return;
  }

  cert_store_proxy_.Bind(
      mojo::PendingRemote<keymaster::mojom::CertStoreInstance>(std::move(pipe),
                                                               0u));
  VLOG(2) << "Bound remote CertStoreInstance interface to pipe.";
  cert_store_proxy_.set_disconnect_handler(
      base::BindOnce(&mojo::Remote<keymaster::mojom::CertStoreInstance>::reset,
                     base::Unretained(&cert_store_proxy_)));
}

void CertStoreBridge::OnBootstrapMojoConnection(bool result) {
  if (!result) {
    cert_store_proxy_.reset();
    return;
  }

  auto receiver =
      std::make_unique<mojo::Receiver<keymaster::mojom::CertStoreHost>>(this);
  mojo::PendingRemote<keymaster::mojom::CertStoreHost> host_proxy;
  receiver->Bind(host_proxy.InitWithNewPipeAndPassReceiver());

  cert_store_proxy_->Init(
      std::move(host_proxy),
      base::BindOnce(&CertStoreBridge::OnConnectionReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(receiver)));
}

void CertStoreBridge::OnConnectionReady(
    std::unique_ptr<mojo::Receiver<mojom::CertStoreHost>> receiver) {
  VLOG(2) << "CertStoreBridge::OnConnectionReady";
  DCHECK(!receiver_);
  receiver->set_disconnect_handler(base::BindOnce(
      &CertStoreBridge::OnConnectionClosed, base::Unretained(this)));
  receiver_ = std::move(receiver);
}

void CertStoreBridge::OnConnectionClosed() {
  VLOG(2) << "CertStoreBridge::OnConnectionClosed";
  DCHECK(receiver_);
  receiver_.reset();
}

}  // namespace keymaster
}  // namespace arc
