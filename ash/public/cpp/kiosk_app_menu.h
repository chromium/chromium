// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_KIOSK_APP_MENU_H_
#define ASH_PUBLIC_CPP_KIOSK_APP_MENU_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "components/account_id/account_id.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Metadata used to populate the Kiosk apps menu in the login screen shelf.
struct ASH_PUBLIC_EXPORT KioskAppMenuEntry {
  // Mirrors `KioskAppType`.
  enum class AppType { kChromeApp, kWebApp, kIsolatedWebApp };

  KioskAppMenuEntry(AppType type,
                    const AccountId& account_id,
                    const std::optional<std::string>& chrome_app_id,
                    std::u16string name,
                    gfx::ImageSkia icon);
  KioskAppMenuEntry(const KioskAppMenuEntry& other);
  KioskAppMenuEntry(KioskAppMenuEntry&& other);

  KioskAppMenuEntry& operator=(KioskAppMenuEntry&& other);
  KioskAppMenuEntry& operator=(const KioskAppMenuEntry& other);

  ~KioskAppMenuEntry();

  AppType type;

  AccountId account_id;

  // Only present in Chrome apps, `nullopt` in other types.
  std::optional<std::string> chrome_app_id;

  std::u16string name;

  gfx::ImageSkia icon;
};

// An interface implemented by Ash to allow Chrome to control the kiosk app
// menu, which appears in the login shelf.
class ASH_PUBLIC_EXPORT KioskAppMenu {
 public:
  // Returns the singleton instance.
  static KioskAppMenu* Get();

  // Update the kiosk app data.
  virtual void SetKioskApps(
      const std::vector<KioskAppMenuEntry>& kiosk_apps) = 0;

  // Configure the kiosk callbacks. |launch_app| will be called if the user
  // selects an item (app) from the menu. |on_show_menu| will be called when the
  // menu will be displayed.
  virtual void ConfigureKioskCallbacks(
      const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app,
      const base::RepeatingClosure& on_show_menu) = 0;

 protected:
  KioskAppMenu();
  virtual ~KioskAppMenu();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_KIOSK_APP_MENU_H_
