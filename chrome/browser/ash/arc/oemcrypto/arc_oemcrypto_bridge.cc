// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/oemcrypto/arc_oemcrypto_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/mojom/protected_buffer_manager.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy_ash.h"
#include "content/public/browser/gpu_service_registry.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace arc {
namespace {

// Singleton factory for ArcOemCryptoBridge
class ArcOemCryptoBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcOemCryptoBridge,
          ArcOemCryptoBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcOemCryptoBridgeFactory";

  static ArcOemCryptoBridgeFactory* GetInstance() {
    return base::Singleton<ArcOemCryptoBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcOemCryptoBridgeFactory>;
  ArcOemCryptoBridgeFactory() = default;
  ~ArcOemCryptoBridgeFactory() override = default;
};

}  // namespace

// static
ArcOemCryptoBridge* ArcOemCryptoBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcOemCryptoBridgeFactory::GetForBrowserContext(context);
}

ArcOemCryptoBridge::ArcOemCryptoBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->oemcrypto()->SetHost(this);
}

ArcOemCryptoBridge::~ArcOemCryptoBridge() {
  arc_bridge_service_->oemcrypto()->SetHost(nullptr);
}

void ArcOemCryptoBridge::Connect(
    mojo::PendingReceiver<mojom::OemCryptoService> receiver) {
  DVLOG(1) << "ArcOemCryptoBridge::Connect called";

  // Check that the user has Attestation for Content Protection enabled in
  // their Chrome settings and if they do not then block this connection since
  // OEMCrypto utilizes Attestation as the root of trust for its DRM
  // implementation.
  bool attestation_enabled = false;
  if (!ash::CrosSettings::Get()->GetBoolean(
          ash::kAttestationForContentProtectionEnabled, &attestation_enabled)) {
    LOG(ERROR) << "Failed to get attestation device setting";
    return;
  }
  if (!attestation_enabled) {
    DVLOG(1) << "OEMCrypto L1 DRM denied because Verified Access is disabled "
                "for this device.";
    return;
  }

  // Get the Mojo interface from the GPU for dealing with secure buffers and
  // pass that to the daemon as well in our ConnectToDaemon call.
  mojo::PendingRemote<mojom::ProtectedBufferManager> gpu_buffer_manager;
  content::BindInterfaceInGpuProcess(
      gpu_buffer_manager.InitWithNewPipeAndPassReceiver());

  // Create the OutputProtection interface to pass to the CDM.
  mojo::PendingRemote<chromeos::cdm::mojom::OutputProtection> output_protection;
  chromeos::CdmFactoryDaemonProxyAsh::GetInstance().GetOutputProtection(
      output_protection.InitWithNewPipeAndPassReceiver());

  chromeos::CdmFactoryDaemonProxyAsh::GetInstance().ConnectOemCrypto(
      std::move(receiver), std::move(gpu_buffer_manager),
      std::move(output_protection));
}

// static
void ArcOemCryptoBridge::EnsureFactoryBuilt() {
  ArcOemCryptoBridgeFactory::GetInstance();
}

}  // namespace arc
