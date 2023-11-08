// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_

#include <vector>

#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Public interface for Kiosk.
class KioskController {
 public:
  static KioskController& Get();

  KioskController(WebKioskAppManager& web_app_manager,
                  KioskChromeAppManager& chrome_app_manager,
                  ArcKioskAppManager& arc_app_manager);
  KioskController(const KioskController&) = delete;
  KioskController& operator=(const KioskController&) = delete;
  ~KioskController();

  std::vector<KioskApp> GetApps() const;
  absl::optional<KioskApp> GetAppById(const KioskAppId& app_id) const;
  absl::optional<KioskApp> GetAutoLaunchApp() const;

 private:
  raw_ref<WebKioskAppManager> web_app_manager_;
  raw_ref<KioskChromeAppManager> chrome_app_manager_;
  raw_ref<ArcKioskAppManager> arc_app_manager_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_CONTROLLER_H_
