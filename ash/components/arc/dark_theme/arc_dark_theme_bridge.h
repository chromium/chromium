// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_DARK_THEME_ARC_DARK_THEME_BRIDGE_H_
#define ASH_COMPONENTS_ARC_DARK_THEME_ARC_DARK_THEME_BRIDGE_H_

#include "ash/components/arc/mojom/dark_theme.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class notifies the Chrome OS side dark theme state to Android.
class ArcDarkThemeBridge : public KeyedService,
                           public ConnectionObserver<mojom::DarkThemeInstance>,
                           public ash::ColorModeObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcDarkThemeBridge* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcDarkThemeBridge* GetForBrowserContextForTesting(
      content::BrowserContext* context);

  ArcDarkThemeBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);
  ~ArcDarkThemeBridge() override;

  ArcDarkThemeBridge(const ArcDarkThemeBridge&) = delete;
  ArcDarkThemeBridge& operator=(const ArcDarkThemeBridge&) = delete;

  // ConnectionObserver<mojom::DarkThemeInstance> overrides:
  void OnConnectionReady() override;

  // ash::ColorModeObserver overrides.
  void OnColorModeChanged(bool dark_theme_status) override;

  bool SendDeviceDarkThemeStateForTesting(bool dark_theme_status);

 private:
  // Sends the device dark theme state to Android.
  bool SendDeviceDarkThemeState(bool dark_theme_status);

  THREAD_CHECKER(thread_checker_);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_DARK_THEME_ARC_DARK_THEME_BRIDGE_H_
