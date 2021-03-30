// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/sharesheet/arc_sharesheet_bridge.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"

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

}  // namespace arc
