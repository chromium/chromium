// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_KEYMINT_ARC_KEYMINT_BRIDGE_H_
#define CHROME_BROWSER_ASH_ARC_KEYMINT_ARC_KEYMINT_BRIDGE_H_

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/mojom/keymint.mojom.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/keymint/cert_store_bridge_keymint.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class is responsible for providing a KeyMintServer proxy by
// bootstrapping a mojo connection with the arc-keymintd daemon. The mojo
// connection is bootstrapped lazily during the first call to GetServer. Chrome
// has no further involvement once the KeyMintServer proxy has been forwarded
// to the KeyMintInstance in ARC.
class ArcKeyMintBridge : public KeyedService,
                         public mojom::keymint::KeyMintHost {
 public:
  using mojom::keymint::KeyMintHost::GetServerCallback;
  using UpdatePlaceholderKeysCallback = base::OnceCallback<void(bool)>;

  // Returns singleton instance for the given BrowserContext, or nullptr if the
  // browser |context| is not allowed to use ARC.
  static ArcKeyMintBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcKeyMintBridge(content::BrowserContext* context,
                   ArcBridgeService* bridge_service);

  ArcKeyMintBridge(const ArcKeyMintBridge&) = delete;
  ArcKeyMintBridge& operator=(const ArcKeyMintBridge&) = delete;

  ~ArcKeyMintBridge() override;

  // Return the factory instance for this class.
  static BrowserContextKeyedServiceFactory* GetFactory();

  // Update the list of placeholder keys to be installed in arc-keymintd.
  //
  // Made virtual for override in tests.
  virtual void UpdatePlaceholderKeys(
      std::vector<keymint::mojom::ChromeOsKeyPtr> keys,
      UpdatePlaceholderKeysCallback callback);
  void UpdatePlaceholderKeysAfterBootstrap(
      std::vector<keymint::mojom::ChromeOsKeyPtr> keys,
      UpdatePlaceholderKeysCallback callback,
      bool bootstrapResult);
  // KeyMintHost mojo interface.
  void GetServer(GetServerCallback callback) override;

  static void EnsureFactoryBuilt();

 private:
  using BootstrapMojoConnectionCallback = base::OnceCallback<void(bool)>;

  void BootstrapMojoConnection(BootstrapMojoConnectionCallback callback);
  void OnBootstrapMojoConnection(BootstrapMojoConnectionCallback callback,
                                 bool bootstrapResult);
  void GetServerAfterBootstrap(GetServerCallback callback,
                               bool bootstrapResult);

  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.
                            //
  // Points to a proxy bound to the implementation in arc-keymintd.
  mojo::Remote<mojom::keymint::KeyMintServer> keymint_server_proxy_;

  // Points to the host implementation in Chrome, used to interact with the
  // arc-keymintd daemon.
  std::unique_ptr<keymint::CertStoreBridgeKeyMint> cert_store_bridge_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcKeyMintBridge> weak_factory_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_KEYMINT_ARC_KEYMINT_BRIDGE_H_
