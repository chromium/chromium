// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/keyboard_shortcut/arc_keyboard_shortcut_bridge.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace arc {

namespace {

class ArcKeyboardShortcutBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcKeyboardShortcutBridge,
          ArcKeyboardShortcutBridgeFactory> {
 public:
  static constexpr const char* kName = "ArcKeyboardShortcutBridgeFactory";

  static ArcKeyboardShortcutBridgeFactory* GetInstance() {
    return base::Singleton<ArcKeyboardShortcutBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcKeyboardShortcutBridgeFactory>;
  ArcKeyboardShortcutBridgeFactory() = default;
  ~ArcKeyboardShortcutBridgeFactory() override = default;
};

}  // namespace

// static
BrowserContextKeyedServiceFactory* ArcKeyboardShortcutBridge::GetFactory() {
  return ArcKeyboardShortcutBridgeFactory::GetInstance();
}

// static
ArcKeyboardShortcutBridge* ArcKeyboardShortcutBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcKeyboardShortcutBridgeFactory::GetForBrowserContext(context);
}

// static
ArcKeyboardShortcutBridge*
ArcKeyboardShortcutBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcKeyboardShortcutBridgeFactory::GetForBrowserContextForTesting(
      context);
}

ArcKeyboardShortcutBridge::ArcKeyboardShortcutBridge(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  if (base::FeatureList::IsEnabled(
          arc::kKeyboardShortcutHelperIntegrationFeature)) {
    arc_bridge_service_->keyboard_shortcut()->SetHost(this);
  }
}

ArcKeyboardShortcutBridge::~ArcKeyboardShortcutBridge() {
  if (base::FeatureList::IsEnabled(
          arc::kKeyboardShortcutHelperIntegrationFeature)) {
    arc_bridge_service_->keyboard_shortcut()->SetHost(nullptr);
  }
}

void ArcKeyboardShortcutBridge::ShowKeyboardShortcutViewer() {
  // TODO(tetsui): Implement when AcceleratorConfigurationProvider becomes
  // available in M96.
  NOTIMPLEMENTED();
}

void ArcKeyboardShortcutBridge::HideKeyboardShortcutViewer() {
  // TODO(tetsui): Implement when AcceleratorConfigurationProvider becomes
  // available in M96.
  NOTIMPLEMENTED();
}

}  // namespace arc
