// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_KEYMASTER_CERT_STORE_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_KEYMASTER_CERT_STORE_BRIDGE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/services/keymaster/public/mojom/cert_store.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/invitation.h"

namespace content {

class BrowserContext;

}  // namespace content

namespace arc::keymaster {

class CertStoreBridge {
 public:
  explicit CertStoreBridge(content::BrowserContext* context);
  CertStoreBridge(const CertStoreBridge&) = delete;
  CertStoreBridge& operator=(const CertStoreBridge&) = delete;
  ~CertStoreBridge();

  // Attaches a new message pipe to the invitation and binds it to the cert
  // store instance proxy.
  void BindToInvitation(mojo::OutgoingInvitation* invitation);

  bool IsProxyBound() const;

  // Updates the latest information about Chrome OS keys to arc-keymasterd.
  void UpdatePlaceholderKeysInKeymaster(
      std::vector<mojom::ChromeOsKeyPtr> keys,
      mojom::CertStoreInstance::UpdatePlaceholderKeysCallback callback);

 private:
  // Points to a proxy bound to the implementation in arc-keymasterd.
  mojo::Remote<keymaster::mojom::CertStoreInstance> cert_store_proxy_;

  base::WeakPtrFactory<CertStoreBridge> weak_ptr_factory_;
};

}  // namespace arc::keymaster

#endif  // CHROME_BROWSER_ASH_ARC_KEYMASTER_CERT_STORE_BRIDGE_H_
