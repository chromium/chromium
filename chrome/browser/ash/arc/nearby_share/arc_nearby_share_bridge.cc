// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/arc_nearby_share_bridge.h"

#include <utility>

#include "ash/public/cpp/app_types_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/nearby_share/nearby_share_session_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_util.h"
#include "components/arc/intent_helper/custom_tab.h"
#include "components/arc/mojom/nearby_share.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"

namespace arc {

namespace {

// Singleton factory for ArcNearbyShareBridgeFactory.
class ArcNearbyShareBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcNearbyShareBridge,
          ArcNearbyShareBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcNearbyShareBridgeFactory";

  static ArcNearbyShareBridgeFactory* GetInstance() {
    return base::Singleton<ArcNearbyShareBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcNearbyShareBridgeFactory>;
  ArcNearbyShareBridgeFactory() = default;
  ~ArcNearbyShareBridgeFactory() override = default;
};

}  // namespace

// static
ArcNearbyShareBridge* ArcNearbyShareBridge::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ArcNearbyShareBridgeFactory::GetForBrowserContext(browser_context);
}

// static
ArcNearbyShareBridge* ArcNearbyShareBridge::GetForBrowserContextForTesting(
    content::BrowserContext* browser_context) {
  return ArcNearbyShareBridgeFactory::GetForBrowserContextForTesting(
      browser_context);
}

ArcNearbyShareBridge::ArcNearbyShareBridge(
    content::BrowserContext* browser_context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      profile_(Profile::FromBrowserContext(browser_context)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->nearby_share()->SetHost(this);
}

ArcNearbyShareBridge::~ArcNearbyShareBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->nearby_share()->SetHost(nullptr);
  session_map_.clear();
}

void ArcNearbyShareBridge::OnNearbyShareSessionFinished(int32_t task_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!session_map_.erase(task_id)) {
    VLOG(1) << "No share session found for " << task_id;
  }
}

void ArcNearbyShareBridge::StartNearbyShare(
    uint32_t task_id,
    mojom::ShareIntentInfoPtr share_info,
    mojo::PendingRemote<mojom::NearbyShareSessionInstance> session_instance,
    StartNearbyShareCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VLOG(1) << "Creating Nearby Share session";
  if (!session_instance) {
    LOG(ERROR) << "instance is null. Unable to create NearbyShareSessionImpl";
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  if (session_map_.find(task_id) != session_map_.end()) {
    LOG(ERROR) << "Unable to create NearbyShareSessionImpl since one already "
               << "exists for " << task_id;
    std::move(callback).Run(mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<mojom::NearbyShareSessionHost> remote;
  // The NearbyShareSessionImpl instance will be deleted when the instance calls
  // |SessionFinishedCallback|. This indicates the session is no longer needed.
  session_map_.emplace(
      task_id,
      std::make_unique<NearbyShareSessionImpl>(
          profile_, task_id, std::move(share_info), std::move(session_instance),
          remote.InitWithNewPipeAndPassReceiver(),
          base::BindOnce(&ArcNearbyShareBridge::OnNearbyShareSessionFinished,
                         weak_ptr_factory_.GetWeakPtr())));
  std::move(callback).Run(std::move(remote));
}

}  // namespace arc
