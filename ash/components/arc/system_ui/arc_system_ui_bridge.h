// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SYSTEM_UI_ARC_SYSTEM_UI_BRIDGE_H_
#define ASH_COMPONENTS_ARC_SYSTEM_UI_ARC_SYSTEM_UI_BRIDGE_H_

#include "ash/components/arc/mojom/system_ui.mojom-shared.h"
#include "ash/components/arc/mojom/system_ui.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/gtest_prod_util.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class notifies the Chrome OS side dark theme state to Android.
class ArcSystemUIBridge : public KeyedService,
                          public ConnectionObserver<mojom::SystemUiInstance>,
                          public ash::ColorModeObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcSystemUIBridge* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcSystemUIBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcSystemUIBridge(content::BrowserContext* context,
                    ArcBridgeService* bridge_service);
  ~ArcSystemUIBridge() override;

  ArcSystemUIBridge(const ArcSystemUIBridge&) = delete;
  ArcSystemUIBridge& operator=(const ArcSystemUIBridge&) = delete;

  // ConnectionObserver<mojom::SystemUiInstance> override:
  void OnConnectionReady() override;

  // ash::ColorModeObserver override:
  void OnColorModeChanged(bool dark_theme_status) override;

  // Sends the device overlay color and the {@link mojom::ThemeStyleType}.
  bool SendOverlayColor(uint32_t source_color,
                        mojom::ThemeStyleType theme_style);

  static void EnsureFactoryBuilt();

 private:
  FRIEND_TEST_ALL_PREFIXES(ArcSystemUIBridgeTest, SendOverlayColor);
  // Sends the device dark theme state to Android.
  bool SendDeviceDarkThemeState(bool dark_theme_status);

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SYSTEM_UI_ARC_SYSTEM_UI_BRIDGE_H_
