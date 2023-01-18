// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/keymaster/cert_store_bridge.h"

#include <cstdint>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

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

}  // namespace keymaster
}  // namespace arc
