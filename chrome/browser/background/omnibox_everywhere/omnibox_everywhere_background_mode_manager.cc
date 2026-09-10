// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/omnibox_everywhere/omnibox_everywhere_background_mode_manager.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_icon.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"

namespace omnibox_everywhere {

OmniboxEverywhereBackgroundModeManager::OmniboxEverywhereBackgroundModeManager(
    ShowUICallback show_ui_callback)
    : show_ui_callback_(std::move(show_ui_callback)) {
  CHECK(base::FeatureList::IsEnabled(omnibox::kOmniboxEverywhere));
  CHECK(g_browser_process && g_browser_process->local_state());

  enabled_pref_member_.Init(
      prefs::kOmniboxEverywhereEnabled, g_browser_process->local_state(),
      base::BindRepeating(
          &OmniboxEverywhereBackgroundModeManager::OnPrefChanged,
          base::Unretained(this)));
  background_mode_pref_member_.Init(
      prefs::kOmniboxEverywhereBackgroundMode, g_browser_process->local_state(),
      base::BindRepeating(
          &OmniboxEverywhereBackgroundModeManager::OnPrefChanged,
          base::Unretained(this)));
#if BUILDFLAG(IS_WIN)
  launch_on_startup_pref_member_.Init(
      prefs::kOmniboxEverywhereLaunchOnStartup,
      g_browser_process->local_state(),
      base::BindRepeating(
          &OmniboxEverywhereBackgroundModeManager::OnPrefChanged,
          base::Unretained(this)));
#endif  // BUILDFLAG(IS_WIN)
  hotkey_string_pref_member_.Init(
      prefs::kOmniboxEverywhereHotkey, g_browser_process->local_state(),
      base::BindRepeating(
          &OmniboxEverywhereBackgroundModeManager::UpdateStatusIconContextMenu,
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
  OnPrefChanged();
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
  if (!enabled_pref_member_.GetValue()) {
#if BUILDFLAG(IS_WIN)
    startup_launch_client_.SetLaunchOnStartup(false);
#endif
    Reset();
    return;
  }

  const bool background_mode_enabled = background_mode_pref_member_.GetValue();

#if BUILDFLAG(IS_WIN)
  const bool launch_on_startup_enabled =
      launch_on_startup_pref_member_.GetValue();
  const bool should_launch_on_startup =
      background_mode_enabled && launch_on_startup_enabled;
  // TODO(crbug.com/532190282): Load the persisted target profile on OS startup
  // launches.
  startup_launch_client_.SetLaunchOnStartup(should_launch_on_startup);
#endif

  if (!profile_) {
    Reset();
    return;
  }

  if (!status_icon_) {
    ShowStatusIcon();
  }

  if (background_mode_enabled) {
    if (!keep_alive_) {
      KeepAliveRegistry* const keep_alive_registry =
          KeepAliveRegistry::GetInstance();

      if (keep_alive_registry && !keep_alive_registry->IsShuttingDown()) {
        keep_alive_ = std::make_unique<ScopedKeepAlive>(
            KeepAliveOrigin::OMNIBOX_EVERYWHERE,
            KeepAliveRestartOption::ENABLED);
      }
    }
    UpdateProfileKeepAlive();
  } else {
    profile_keep_alive_.reset();
    keep_alive_.reset();
  }
}

void OmniboxEverywhereBackgroundModeManager::UpdateProfileKeepAlive() {
  if (enabled_pref_member_.GetValue() &&
      background_mode_pref_member_.GetValue() && profile_ &&
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

  gfx::ImageSkia icon_image = GetOmniboxEverywhereIcon();

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
  browser_collection_observation_.Reset();
  context_menu_ = nullptr;

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
      if (profile_) {
        chrome::ShowSettingsSubPageForProfile(profile_, chrome::kSearchSubPage);
      }
      break;
    case IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_EXIT:
      chrome::CloseAllBrowsers();
      base::RecordAction(base::UserMetricsAction("Exit"));
      break;
    default:
      NOTREACHED();
  }
}

void OmniboxEverywhereBackgroundModeManager::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  UpdateVisibilityOfExitInContextMenu();
}

void OmniboxEverywhereBackgroundModeManager::OnBrowserClosed(
    BrowserWindowInterface* browser) {
  UpdateVisibilityOfExitInContextMenu();
}

void OmniboxEverywhereBackgroundModeManager::
    UpdateVisibilityOfExitInContextMenu() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  if (context_menu_) {
    const bool is_visible = GlobalBrowserCollection::GetInstance()->IsEmpty();
    const std::optional<size_t> index = context_menu_->GetIndexOfCommandId(
        IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_EXIT);
    CHECK(index.has_value() && index.value() > 0);

    if (is_visible) {
      if (context_menu_->GetTypeAt(index.value() - 1) !=
          ui::MenuModel::TYPE_SEPARATOR) {
        context_menu_->InsertSeparatorAt(index.value(), ui::NORMAL_SEPARATOR);
      }
    } else {
      if (context_menu_->GetTypeAt(index.value() - 1) ==
          ui::MenuModel::TYPE_SEPARATOR) {
        context_menu_->RemoveItemAt(index.value() - 1);
      }
    }

    context_menu_->SetCommandIdVisible(
        IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_EXIT, is_visible);
  }
#endif
}

void OmniboxEverywhereBackgroundModeManager::UpdateStatusIconContextMenu() {
  if (!status_icon_) {
    return;
  }

  auto menu = std::make_unique<StatusIconMenuModel>(this);

  PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : nullptr;
  ui::Accelerator hotkey = prefs::GetOmniboxEverywhereHotkey(local_state);
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  menu->AddSeparator(ui::NORMAL_SEPARATOR);
  menu->AddItem(
      IDC_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_EXIT,
      l10n_util::GetStringUTF16(IDS_OMNIBOX_EVERYWHERE_STATUS_ICON_MENU_EXIT));
#endif

  context_menu_ = menu.get();
  status_icon_->SetContextMenu(std::move(menu));

  if (!browser_collection_observation_.IsObserving()) {
    browser_collection_observation_.Observe(
        GlobalBrowserCollection::GetInstance());
  }
  UpdateVisibilityOfExitInContextMenu();
}

}  // namespace omnibox_everywhere
