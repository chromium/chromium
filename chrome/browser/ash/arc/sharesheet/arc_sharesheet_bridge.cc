// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/sharesheet/arc_sharesheet_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

// Singleton factory for ArcSharesheetBridgeFactory.
class ArcSharesheetBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcSharesheetBridge,
          ArcSharesheetBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcSharesheetBridgeFactory";

  static ArcSharesheetBridgeFactory* GetInstance() {
    return base::Singleton<ArcSharesheetBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcSharesheetBridgeFactory>;
  ArcSharesheetBridgeFactory() = default;
  ~ArcSharesheetBridgeFactory() override = default;
};

}  // namespace

// static
ArcSharesheetBridge* ArcSharesheetBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ArcSharesheetBridgeFactory::GetForBrowserContext(context);
}

// static
ArcSharesheetBridge* ArcSharesheetBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcSharesheetBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcSharesheetBridge::ArcSharesheetBridge(content::BrowserContext* context,
                                         ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      profile_(Profile::FromBrowserContext(context)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->sharesheet()->SetHost(this);
  VLOG(1) << "ArcSharesheetBridge created";
}

ArcSharesheetBridge::~ArcSharesheetBridge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_bridge_service_->sharesheet()->SetHost(nullptr);
}

// static
void ArcSharesheetBridge::EnsureFactoryBuilt() {
  ArcSharesheetBridgeFactory::GetInstance();
}

}  // namespace arc
