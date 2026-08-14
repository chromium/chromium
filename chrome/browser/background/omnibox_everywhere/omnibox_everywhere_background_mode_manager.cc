// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/omnibox_everywhere/omnibox_everywhere_background_mode_manager.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

namespace omnibox_everywhere {

OmniboxEverywhereBackgroundModeManager::OmniboxEverywhereBackgroundModeManager(
    ShowUICallback show_ui_callback)
    : show_ui_callback_(std::move(show_ui_callback)) {
  CHECK(base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere));
  CHECK(g_browser_process && g_browser_process->local_state());

  background_mode_pref_member_.Init(
      prefs::kOmniboxEverywhereBackgroundMode, g_browser_process->local_state(),
      base::BindRepeating(
          &OmniboxEverywhereBackgroundModeManager::OnPrefChanged,
          base::Unretained(this)));
  OnPrefChanged();
}

OmniboxEverywhereBackgroundModeManager::
    ~OmniboxEverywhereBackgroundModeManager() {
  Reset();
}

void OmniboxEverywhereBackgroundModeManager::SetProfile(Profile* profile) {
  if (profile_ == profile) {
    return;
  }
  profile_ = profile;
  UpdateProfileKeepAlive();
}

void OmniboxEverywhereBackgroundModeManager::Reset() {
  profile_keep_alive_.reset();
  keep_alive_.reset();
  HideStatusIcon();
}

void OmniboxEverywhereBackgroundModeManager::ExitBackgroundMode() {
  Reset();
}

void OmniboxEverywhereBackgroundModeManager::OnPrefChanged() {
  if (!background_mode_pref_member_.GetValue()) {
    Reset();
    return;
  }

  if (!status_icon_) {
    ShowStatusIcon();
  }

  if (!keep_alive_) {
    KeepAliveRegistry* const keep_alive_registry =
        KeepAliveRegistry::GetInstance();

    if (keep_alive_registry && !keep_alive_registry->IsShuttingDown()) {
      keep_alive_ = std::make_unique<ScopedKeepAlive>(
          KeepAliveOrigin::OMNIBOX_EVERYWHERE, KeepAliveRestartOption::ENABLED);
    }
  }

  UpdateProfileKeepAlive();
}

void OmniboxEverywhereBackgroundModeManager::UpdateProfileKeepAlive() {
  if (background_mode_pref_member_.GetValue() && profile_ &&
      !profile_->IsOffTheRecord()) {
    if (!profile_keep_alive_ || profile_keep_alive_->profile() != profile_) {
      profile_keep_alive_ = ScopedProfileKeepAlive::TryAcquire(
          profile_, ProfileKeepAliveOrigin::kOmniboxEverywhere);
    }
  } else {
    profile_keep_alive_.reset();
  }
}

void OmniboxEverywhereBackgroundModeManager::ShowStatusIcon() {
  StatusTray* status_tray =
      g_browser_process ? g_browser_process->status_tray() : nullptr;
  if (!status_tray) {
    return;
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const gfx::VectorIcon& icon_vector = vector_icons::kGoogleGLogoIcon;
#else
  const gfx::VectorIcon& icon_vector = vector_icons::kSearchIcon;
#endif

  gfx::ImageSkia icon_image =
      gfx::CreateVectorIcon(icon_vector, 16, SK_ColorBLACK);

  std::u16string tooltip =
      l10n_util::GetStringUTF16(IDS_OMNIBOX_EVERYWHERE_STATUS_ICON_TOOLTIP);
  status_icon_ = status_tray->CreateStatusIcon(
      StatusTray::OMNIBOX_EVERYWHERE_ICON, icon_image, tooltip);
  if (status_icon_) {
    status_icon_->AddObserver(this);
#if BUILDFLAG(IS_MAC)
    status_icon_->SetOpenMenuWithSecondaryClick(true);
#endif
    UpdateStatusIconContextMenu();
  }
}

void OmniboxEverywhereBackgroundModeManager::HideStatusIcon() {
  if (!status_icon_) {
    return;
  }

  status_icon_->RemoveObserver(this);

  StatusTray* status_tray =
      g_browser_process ? g_browser_process->status_tray() : nullptr;
  if (status_tray) {
    // Avoids dangling the `status_icon_` raw_ptr while `RemoveStatusIcon` is
    // called.
    StatusIcon* status_icon = status_icon_;
    status_icon_ = nullptr;
    status_tray->RemoveStatusIcon(status_icon);
  } else {
    status_icon_ = nullptr;
  }
}

void OmniboxEverywhereBackgroundModeManager::OnStatusIconClicked() {
  if (show_ui_callback_) {
    show_ui_callback_.Run();
  }
}

void OmniboxEverywhereBackgroundModeManager::ExecuteCommand(int command_id,
                                                            int event_flags) {
  switch (command_id) {
    case IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE:
      if (show_ui_callback_) {
        show_ui_callback_.Run();
      }
      break;
    case IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT:
    case IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_SETTINGS:
      // Placeholders for now.
      break;
    default:
      NOTREACHED();
  }
}

void OmniboxEverywhereBackgroundModeManager::UpdateStatusIconContextMenu() {
  if (!status_icon_) {
    return;
  }

  auto menu = std::make_unique<StatusIconMenuModel>(this);

  ui::Accelerator hotkey = GetHotkey();
  menu->AddItem(IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE,
                l10n_util::GetStringUTF16(
                    IDS_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE));
  menu->SetAcceleratorForCommandId(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_TOGGLE, &hotkey);
  menu->SetForceShowAcceleratorForItemAt(0, true);

  menu->AddItem(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT,
      l10n_util::GetStringUTF16(
          IDS_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT));

  menu->AddItem(IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_SETTINGS,
                l10n_util::GetStringUTF16(
                    IDS_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_SETTINGS));

  status_icon_->SetContextMenu(std::move(menu));
}

}  // namespace omnibox_everywhere
