// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_KEYBOARD_SHORTCUT_ARC_KEYBOARD_SHORTCUT_BRIDGE_H_
#define ASH_COMPONENTS_ARC_KEYBOARD_SHORTCUT_ARC_KEYBOARD_SHORTCUT_BRIDGE_H_

#include "ash/components/arc/mojom/keyboard_shortcut.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

class BrowserContextKeyedServiceFactory;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcKeyboardShortcutBridge : public KeyedService,
                                  public mojom::KeyboardShortcutHost {
 public:
  static BrowserContextKeyedServiceFactory* GetFactory();

  static ArcKeyboardShortcutBridge* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcKeyboardShortcutBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcKeyboardShortcutBridge(content::BrowserContext* context,
                            ArcBridgeService* bridge_service);
  ~ArcKeyboardShortcutBridge() override;

  // mojom::KeyboardShortcutHost:
  void ShowKeyboardShortcutViewer() override;
  void HideKeyboardShortcutViewer() override;

  static void EnsureFactoryBuilt();

 private:
  ArcBridgeService* const arc_bridge_service_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_KEYBOARD_SHORTCUT_ARC_KEYBOARD_SHORTCUT_BRIDGE_H_
