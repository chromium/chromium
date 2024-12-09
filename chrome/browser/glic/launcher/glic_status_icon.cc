// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_status_icon.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/launcher/glic_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "glic_status_icon.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

GlicStatusIcon::GlicStatusIcon(GlicController* controller,
                               StatusTray* status_tray)
    : controller_(controller), status_tray_(status_tray) {
  // TODO(https://crbug.com/382287104): Use correct icon.
  gfx::ImageSkia status_tray_icon = gfx::CreateVectorIcon(
      vector_icons::kProductRefreshIcon, gfx::kPlaceholderColor);

  status_icon_ = status_tray_->CreateStatusIcon(
      StatusTray::GLIC_ICON, status_tray_icon,
      l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_TOOLTIP));
  status_icon_->AddObserver(this);

  std::unique_ptr<StatusIconMenuModel> menu = CreateStatusIconMenu();
  context_menu_ = menu.get();
  status_icon_->SetContextMenu(std::move(menu));
}

std::unique_ptr<StatusIconMenuModel> GlicStatusIcon::CreateStatusIconMenu() {
  std::unique_ptr<StatusIconMenuModel> menu(new StatusIconMenuModel(this));
  menu->AddItem(IDC_GLIC_STATUS_ICON_MENU_SHOW,
                l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_MENU_SHOW));
  menu->AddSeparator(ui::NORMAL_SEPARATOR);

  menu->AddItem(IDC_GLIC_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT,
                l10n_util::GetStringUTF16(
                    IDS_GLIC_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT));
  menu->AddItem(IDC_GLIC_STATUS_ICON_MENU_SETTINGS,
                l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_MENU_SETTINGS));
  return menu;
}

GlicStatusIcon::~GlicStatusIcon() {
  status_icon_->RemoveObserver(this);
  status_tray_->RemoveStatusIcon(status_icon_);
}

void GlicStatusIcon::OnStatusIconClicked() {
  controller_->Show();
}

void GlicStatusIcon::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDC_GLIC_STATUS_ICON_MENU_SHOW:
      controller_->Show();
      break;
    case IDC_GLIC_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT:
      // TODO(https://crbug.com/378143781): Use correct settings subpage and
      // show help bubble on the appropriate setting.
      chrome::ShowSettingsSubPageForProfile(
          glic::GlicProfileManager::GetInstance()->GetProfileForLaunch(),
          std::string());
      break;
    case IDC_GLIC_STATUS_ICON_MENU_SETTINGS:
      // TODO(https://crbug.com/378143780): Use the correct settings subpage.
      chrome::ShowSettingsSubPageForProfile(
          glic::GlicProfileManager::GetInstance()->GetProfileForLaunch(),
          std::string());
      break;
    default:
      NOTREACHED();
  }
}
