// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_KIOSK_APPS_BUTTON_H_
#define ASH_SHELF_KIOSK_APPS_BUTTON_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/shelf/login_shelf_button.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

// The kiosk app button will only be created for the primary display's login
// shelf. The appearance of the button is managed by it's base class.
// The class manages the kiosk menu appearance and model.
class ASH_EXPORT KioskAppsButton : public LoginShelfButton {
  METADATA_HEADER(KioskAppsButton, LoginShelfButton)

 public:
  KioskAppsButton();
  KioskAppsButton(const KioskAppsButton&) = delete;
  KioskAppsButton& operator=(const KioskAppsButton&) = delete;
  ~KioskAppsButton() override;

  // Launch app based on the app_id.
  bool LaunchAppForTesting(const std::string& app_id);
  bool LaunchAppForTesting(const AccountId& account_id);

  // Replace the existing items list with a new list of kiosk app menu items.
  void SetApps(const std::vector<KioskAppMenuEntry>& kiosk_apps);

  // Setup the application launch, and menu show/close callbacks.
  void ConfigureKioskCallbacks(
      base::RepeatingCallback<void(const KioskAppMenuEntry&)> launch_app,
      base::RepeatingClosure on_show_menu,
      base::RepeatingClosure on_close_menu);

  bool HasApps() const;

  void SetVisible(bool visible) override;

  // Shows the kiosk menu.
  void DisplayMenu();

  // The opened state of the menu.
  bool IsMenuOpened() const;

  // Setup the button controller callback.
  void SetCallback(PressedCallback callback) override;

 protected:
  void NotifyClick(const ui::Event& event) final;

 private:
  class KioskAppsMenuModel;

  std::unique_ptr<views::MenuRunner> menu_runner_;
  raw_ptr<views::MenuButtonController> menu_button_controller_ = nullptr;
  std::unique_ptr<KioskAppsMenuModel> menu_model_;
};

}  // namespace ash

#endif  // ASH_SHELF_KIOSK_APPS_BUTTON_H_
