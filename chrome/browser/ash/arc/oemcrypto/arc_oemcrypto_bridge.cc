// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/oemcrypto/arc_oemcrypto_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/mojom/protected_buffer_manager.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
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

mojo::PendingRemote<mojom::ProtectedBufferManager>
GetGpuBufferManagerOnIOThread() {
  // Get the Mojo interface from the GPU for dealing with secure buffers and
  // pass that to the daemon as well in our ConnectToDaemon call.
  mojo::PendingRemote<mojom::ProtectedBufferManager> gpu_buffer_manager;
  content::BindInterfaceInGpuProcess(
      gpu_buffer_manager.InitWithNewPipeAndPassReceiver());
  return gpu_buffer_manager;
}

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
          chromeos::kAttestationForContentProtectionEnabled,
          &attestation_enabled)) {
    LOG(ERROR) << "Failed to get attestation device setting";
    return;
  }
  if (!attestation_enabled) {
    DVLOG(1) << "OEMCrypto L1 DRM denied because Verified Access is disabled "
                "for this device.";
    return;
  }

  // We need to get the GPU interface on the IO thread, then after that is
  // done it will run the Mojo call on our thread. This call may have come back
  // on our mojo thread or the proxy's mojo thread, either one is safe to invoke
  // the OemCrypto call because the proxy will repost it to the proper thread.
  base::PostTaskAndReplyWithResult(
      content::GetIOThreadTaskRunner({}).get(), FROM_HERE,
      base::BindOnce(&GetGpuBufferManagerOnIOThread),
      base::BindOnce(&ArcOemCryptoBridge::ConnectToDaemon,
                     weak_factory_.GetWeakPtr(), std::move(receiver)));
}

void ArcOemCryptoBridge::ConnectToDaemon(
    mojo::PendingReceiver<mojom::OemCryptoService> receiver,
    mojo::PendingRemote<mojom::ProtectedBufferManager> gpu_buffer_manager) {
  // Create the OutputProtection interface to pass to the CDM.
  mojo::PendingRemote<chromeos::cdm::mojom::OutputProtection> output_protection;
  chromeos::CdmFactoryDaemonProxy::GetInstance().GetOutputProtection(
      output_protection.InitWithNewPipeAndPassReceiver());

  chromeos::CdmFactoryDaemonProxy::GetInstance().ConnectOemCrypto(
      std::move(receiver), std::move(gpu_buffer_manager),
      std::move(output_protection));
}

}  // namespace arc
