// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_OEMCRYPTO_ARC_OEMCRYPTO_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_OEMCRYPTO_ARC_OEMCRYPTO_BRIDGE_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "components/arc/mojom/oemcrypto.mojom.h"
#include "components/arc/mojom/protected_buffer_manager.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcOemCryptoBridge : public KeyedService,
                           public mojom::OemCryptoHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcOemCryptoBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcOemCryptoBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcOemCryptoBridge() override;

  // OemCrypto Mojo host interface
  void Connect(
      mojo::PendingReceiver<mojom::OemCryptoService> receiver) override;

 private:
  void ConnectToDaemon(
      mojo::PendingReceiver<mojom::OemCryptoService> receiver,
      mojo::PendingRemote<mojom::ProtectedBufferManager> gpu_buffer_manager);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcOemCryptoBridge> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcOemCryptoBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_OEMCRYPTO_ARC_OEMCRYPTO_BRIDGE_H_
