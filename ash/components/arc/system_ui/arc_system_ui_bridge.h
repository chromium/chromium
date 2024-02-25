// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SYSTEM_UI_ARC_SYSTEM_UI_BRIDGE_H_
#define ASH_COMPONENTS_ARC_SYSTEM_UI_ARC_SYSTEM_UI_BRIDGE_H_

#include "ash/components/arc/mojom/system_ui.mojom-shared.h"
#include "ash/components/arc/mojom/system_ui.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "ash/shell_observer.h"
#include "ash/style/color_palette_controller.h"
#include "base/check_is_test.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ash {
class Shell;
}

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class notifies the Chrome OS side dark theme state to Android.
class ArcSystemUIBridge : public KeyedService,
                          public ConnectionObserver<mojom::SystemUiInstance>,
                          public ash::ColorPaletteController::Observer,
                          public ash::ShellObserver {
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

  // ash::ColorPaletteController::Observer override:
  void OnColorPaletteChanging(const ash::ColorPaletteSeed& seed) override;

  // ash::ShellObserver override:
  void OnShellDestroying() override;

  // Sends the device overlay color and the {@link mojom::ThemeStyleType}.
  bool SendOverlayColor(uint32_t source_color,
                        mojom::ThemeStyleType theme_style);

  void SetColorPaletteControllerForTesting(
      ash::ColorPaletteController* controller) {
    CHECK_IS_TEST();
    color_palette_controller_ = controller;
  }

  static void EnsureFactoryBuilt();

 private:
  FRIEND_TEST_ALL_PREFIXES(ArcSystemUIBridgeTest, SendOverlayColor);
  // Sends the device dark theme state to Android.
  bool SendDeviceDarkThemeState(bool dark_theme_status);

  // Set `color_palette_controller_` from `ash::Shell` if it's not already set
  // and attach the necessary observers.
  void AttachColorPaletteController();

  // Removes `this` as an observer of the `ColorPaletteController` and sets it
  // to `nullptr`.
  void DetachColorPaletteController();

  THREAD_CHECKER(thread_checker_);

  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};

  // The most recent seed sent to ARC.
  std::optional<const ash::ColorPaletteSeed> previous_seed_;
  raw_ptr<ash::ColorPaletteController> color_palette_controller_ =
      nullptr;  // Owned by Shell.
  const raw_ptr<ArcBridgeService>
      arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SYSTEM_UI_ARC_SYSTEM_UI_BRIDGE_H_
