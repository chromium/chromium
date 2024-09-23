// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/kiosk_apps_button.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/kiosk_app_menu.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/login_shelf_button.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

class KioskAppsButton::KioskAppsMenuModel
    : public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate {
 public:
  KioskAppsMenuModel() : ui::SimpleMenuModel(this) {}

  KioskAppsMenuModel(const KioskAppsMenuModel&) = delete;
  KioskAppsMenuModel& operator=(const KioskAppsMenuModel&) = delete;

  ~KioskAppsMenuModel() override = default;

  bool IsLaunchEnabled() const { return is_launch_enabled_; }
  void SetLaunchEnabled(bool enabled) { is_launch_enabled_ = enabled; }

  bool LaunchApp(const std::string& chrome_app_id) {
    for (size_t i = 0; i < kiosk_apps_.size(); ++i) {
      if (kiosk_apps_[i].chrome_app_id == chrome_app_id) {
        ExecuteCommand(/*command_id=*/i, /*event_flags=*/0);
        return true;
      }
    }
    return false;
  }

  bool LaunchApp(const AccountId& account_id) {
    for (size_t i = 0; i < kiosk_apps_.size(); ++i) {
      if (kiosk_apps_[i].account_id == account_id) {
        ExecuteCommand(/*command_id=*/i, /*event_flags=*/0);
        return true;
      }
    }
    return false;
  }

  void SetApps(const std::vector<KioskAppMenuEntry>& kiosk_apps) {
    kiosk_apps_ = kiosk_apps;
    Clear();
    const gfx::Size kAppIconSize(16, 16);
    for (size_t i = 0; i < kiosk_apps_.size(); ++i) {
      gfx::ImageSkia icon = gfx::ImageSkiaOperations::CreateResizedImage(
          kiosk_apps_[i].icon, skia::ImageOperations::RESIZE_GOOD,
          kAppIconSize);
      AddItemWithIcon(i, kiosk_apps_[i].name,
                      ui::ImageModel::FromImageSkia(icon));
    }
  }

  void OnMenuWillShow(SimpleMenuModel* source) override {
    is_menu_opened_ = true;
    on_show_menu_.Run();
  }

  void MenuClosed(SimpleMenuModel* source) override {
    on_close_menu_.Run();
    is_menu_opened_ = false;
  }

  bool IsMenuOpened() const { return is_menu_opened_; }

  void ExecuteCommand(int command_id, int event_flags) override {
    DCHECK(command_id >= 0 &&
           base::checked_cast<size_t>(command_id) < kiosk_apps_.size());
    // Once an app is clicked on, don't allow any additional clicks until
    // the state is reset (when login screen reappears).
    is_launch_enabled_ = false;
    launch_app_callback_.Run(kiosk_apps_[command_id]);
  }

  void ConfigureKioskCallbacks(
      base::RepeatingCallback<void(const KioskAppMenuEntry&)> launch_app,
      base::RepeatingClosure on_show_menu,
      base::RepeatingClosure on_close_menu) {
    launch_app_callback_ = std::move(launch_app);
    on_show_menu_ = std::move(on_show_menu);
    on_close_menu_ = std::move(on_close_menu);
  }

 private:
  base::RepeatingCallback<void(const KioskAppMenuEntry&)> launch_app_callback_;
  base::RepeatingClosure on_show_menu_;
  base::RepeatingClosure on_close_menu_;
  std::vector<KioskAppMenuEntry> kiosk_apps_;

  bool is_launch_enabled_ = true;
  bool is_menu_opened_ = false;
};

KioskAppsButton::KioskAppsButton()
    : LoginShelfButton(PressedCallback(),
                       IDS_ASH_SHELF_APPS_BUTTON,
                       kShelfAppsButtonIcon) {
  menu_model_ = std::make_unique<KioskAppsMenuModel>();
  std::unique_ptr<views::MenuButtonController> menu_button_controller =
      std::make_unique<views::MenuButtonController>(
          this,
          base::BindRepeating(
              [](KioskAppsButton* button) {
                if (button->menu_model_->IsLaunchEnabled()) {
                  button->DisplayMenu();
                }
              },
              this),
          std::make_unique<Button::DefaultButtonControllerDelegate>(this));
  menu_button_controller_ = menu_button_controller.get();
  SetButtonController(std::move(menu_button_controller));

  set_suppress_default_focus_handling();
}

KioskAppsButton::~KioskAppsButton() = default;

bool KioskAppsButton::LaunchAppForTesting(const std::string& chrome_app_id) {
  return menu_model_->LaunchApp(chrome_app_id);
}

bool KioskAppsButton::LaunchAppForTesting(const AccountId& account_id) {
  return menu_model_->LaunchApp(account_id);
}

void KioskAppsButton::SetApps(
    const std::vector<KioskAppMenuEntry>& kiosk_apps) {
  menu_model_->SetApps(kiosk_apps);
  // If the menu is being shown, update it.
  if (menu_runner_ && menu_runner_->IsRunning()) {
    DisplayMenu();
  }
}

void KioskAppsButton::ConfigureKioskCallbacks(
    base::RepeatingCallback<void(const KioskAppMenuEntry&)> launch_app,
    base::RepeatingClosure on_show_menu,
    base::RepeatingClosure on_close_menu) {
  menu_model_->ConfigureKioskCallbacks(
      std::move(launch_app), std::move(on_show_menu), std::move(on_close_menu));
}

bool KioskAppsButton::HasApps() const {
  return menu_model_->GetItemCount() > 0;
}

void KioskAppsButton::SetVisible(bool visible) {
  LoginShelfButton::SetVisible(visible);
  if (visible) {
    menu_model_->SetLaunchEnabled(true);
  }
}

void KioskAppsButton::DisplayMenu() {
  const gfx::Point point = GetMenuPosition();
  const gfx::Point origin(point.x() - width(), point.y() - height());
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(
      GetWidget()->GetTopLevelWidget(), menu_button_controller_,
      gfx::Rect(origin, gfx::Size()),
      views::MenuAnchorPosition::kBubbleBottomLeft, ui::MENU_SOURCE_NONE);
}

bool KioskAppsButton::IsMenuOpened() const {
  return menu_model_->IsMenuOpened();
}

void KioskAppsButton::SetCallback(PressedCallback callback) {
  menu_button_controller_->SetCallback(std::move(callback));
}

void KioskAppsButton::NotifyClick(const ui::Event& event) {
  // Run pressed callback via MenuButtonController, instead of directly.
  menu_button_controller_->Activate(&event);
}

BEGIN_METADATA(KioskAppsButton)
END_METADATA

}  // namespace ash
