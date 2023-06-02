// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/chrome_feature_flags/arc_chrome_feature_flags_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_features.h"
#include "base/memory/singleton.h"

namespace arc {

namespace {

// Singleton factory for ArcChromeFeatureFlagsBridge.
class ArcChromeFeatureFlagsBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcChromeFeatureFlagsBridge,
          ArcChromeFeatureFlagsBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcChromeFeatureFlagsBridgeFactory";

  static ArcChromeFeatureFlagsBridgeFactory* GetInstance() {
    return base::Singleton<ArcChromeFeatureFlagsBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcChromeFeatureFlagsBridgeFactory>;
  ArcChromeFeatureFlagsBridgeFactory() = default;
  ~ArcChromeFeatureFlagsBridgeFactory() override = default;
};

}  // namespace

// static
ArcChromeFeatureFlagsBridge* ArcChromeFeatureFlagsBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcChromeFeatureFlagsBridgeFactory::GetForBrowserContext(context);
}

// static
ArcChromeFeatureFlagsBridge*
ArcChromeFeatureFlagsBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcChromeFeatureFlagsBridgeFactory::GetForBrowserContextForTesting(
      context);
}

ArcChromeFeatureFlagsBridge::ArcChromeFeatureFlagsBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->chrome_feature_flags()->AddObserver(this);
}

ArcChromeFeatureFlagsBridge::~ArcChromeFeatureFlagsBridge() {
  arc_bridge_service_->chrome_feature_flags()->RemoveObserver(this);
}

void ArcChromeFeatureFlagsBridge::OnConnectionReady() {
  NotifyQsRevamp();
}

void ArcChromeFeatureFlagsBridge::NotifyQsRevamp() {
  mojom::ChromeFeatureFlagsInstance* chrome_feature_flags_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->chrome_feature_flags(),
                                  NotifyQsRevamp);
  if (!chrome_feature_flags_instance) {
    return;
  }
  chrome_feature_flags_instance->NotifyQsRevamp(
      ash::features::IsQsRevampEnabled());
}

// static
void ArcChromeFeatureFlagsBridge::EnsureFactoryBuilt() {
  ArcChromeFeatureFlagsBridgeFactory::GetInstance();
}

}  // namespace arc
