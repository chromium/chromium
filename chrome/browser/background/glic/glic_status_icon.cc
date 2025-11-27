// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_status_icon.h"

#include <memory>
#include <optional>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/background/glic/glic_controller.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/glic_settings_util.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/resources/glic_resources.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "glic_status_icon.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/task/sequenced_task_runner.h"
#include "base/win/registry.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"
#endif

namespace {

int GetTooltipMessageId(bool panel_showing) {
  // If GlicMultiInstance is enabled, show a single menu item and corresponding
  // tooltip for toggling the UI.
  bool multi_instance_enabled = glic::GlicEnabling::IsMultiInstanceEnabled();

  switch (chrome::GetChannel()) {
    case version_info::Channel::CANARY: {
      if (multi_instance_enabled) {
        return IDS_GLIC_STATUS_ICON_TOOLTIP_TOGGLE_CANARY;
      }
      return panel_showing ? IDS_GLIC_STATUS_ICON_TOOLTIP_CLOSE_CANARY
                           : IDS_GLIC_STATUS_ICON_TOOLTIP_CANARY;
    }
    case version_info::Channel::DEV: {
      if (multi_instance_enabled) {
        return IDS_GLIC_STATUS_ICON_TOOLTIP_TOGGLE_DEV;
      }
      return panel_showing ? IDS_GLIC_STATUS_ICON_TOOLTIP_CLOSE_DEV
                           : IDS_GLIC_STATUS_ICON_TOOLTIP_DEV;
    }
    case version_info::Channel::BETA: {
      if (multi_instance_enabled) {
        return IDS_GLIC_STATUS_ICON_TOOLTIP_TOGGLE_BETA;
      }
      return panel_showing ? IDS_GLIC_STATUS_ICON_TOOLTIP_CLOSE_BETA
                           : IDS_GLIC_STATUS_ICON_TOOLTIP_BETA;
    }
    default: {
      if (multi_instance_enabled) {
        return IDS_GLIC_STATUS_ICON_TOOLTIP_TOGGLE;
      }
      return panel_showing ? IDS_GLIC_STATUS_ICON_TOOLTIP_CLOSE
                           : IDS_GLIC_STATUS_ICON_TOOLTIP;
    }
  }
}

}  // namespace

namespace glic {

GlicStatusIcon::GlicStatusIcon(GlicController* controller,
                               StatusTray* status_tray)
    : controller_(controller), status_tray_(status_tray) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(features::kGlicShowStatusTrayIcon)) {
    return;
  }
#endif

  status_icon_ = status_tray_->CreateStatusIcon(
      StatusTray::GLIC_ICON, GetIcon(),
      l10n_util::GetStringUTF16(GetTooltipMessageId(controller_->IsShowing())));

  // If the StatusIcon cannot be created, don't configure it.
  if (!status_icon_) {
    return;
  }

#if BUILDFLAG(IS_LINUX)
  // Set a vector icon for proper theming on Linux.
  status_icon_->SetIcon(
      GlicVectorIconManager::GetVectorIcon(IDR_GLIC_BUTTON_VECTOR_ICON));
#else
  // Linux doesn't activate icon on click so no need to observe.
  status_icon_->AddObserver(this);
#endif

#if BUILDFLAG(IS_MAC)
  if (features::kGlicStatusIconOpenMenuWithSecondaryClick.Get()) {
    status_icon_->SetOpenMenuWithSecondaryClick(true);
  }
  // This sets the NSImage template property which makes the icon light/dark
  // based on contrast with the wallpaper.
  status_icon_->SetImageTemplate(true);
#endif

#if BUILDFLAG(IS_WIN)
  if (hkcu_themes_regkey_.Open(
          HKEY_CURRENT_USER,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
          KEY_READ | KEY_NOTIFY) == ERROR_SUCCESS) {
    UpdateForThemesRegkey();
    // If there's no sequenced task runner handle, we can't be called back for
    // registry changes. This generally happens in tests.
    if (base::SequencedTaskRunner::HasCurrentDefault()) {
      RegisterThemesRegkeyObserver();
    }
  } else {
    // Fall back to the native theme's preferred color scheme.
    native_theme_observer_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  }
#endif

  std::unique_ptr<StatusIconMenuModel> menu = CreateStatusIconMenu();
  context_menu_ = menu.get();
  status_icon_->SetContextMenu(std::move(menu));

  BrowserList::AddObserver(this);
  UpdateVisibilityOfExitInContextMenu();
  UpdateVisibilityOfShowAndCloseInContextMenu();

  GlicProfileManager* manager = GlicProfileManager::GetInstance();
  profile_observer_.Observe(manager);
  if (GlicKeyedService* service = manager->GetLastActiveGlic()) {
    panel_state_observer_.Observe(&service->window_controller());
  }
}

GlicStatusIcon::~GlicStatusIcon() {
  BrowserList::RemoveObserver(this);

  context_menu_ = nullptr;
  if (status_icon_) {
#if !BUILDFLAG(IS_LINUX)
    status_icon_->RemoveObserver(this);
#endif
    std::unique_ptr<StatusIcon> removed_icon =
        status_tray_->RemoveStatusIcon(status_icon_);
    status_icon_ = nullptr;
    removed_icon.reset();
  }
  status_tray_ = nullptr;
}

void GlicStatusIcon::OnStatusIconClicked() {
  controller_->Toggle(mojom::InvocationSource::kOsButton);
}

void GlicStatusIcon::ExecuteCommand(int command_id, int event_flags) {
  auto* profile = GlicProfileManager::GetInstance()->GetProfileForLaunch();
  switch (command_id) {
    case IDC_GLIC_STATUS_ICON_MENU_SHOW: {
      controller_->Show(mojom::InvocationSource::kOsButtonMenu);
      base::RecordAction(base::UserMetricsAction(
          "GlicOsEntrypoint.ContextMenuSelection.OpenGlic"));
      break;
    }
    case IDC_GLIC_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT: {
      OpenGlicKeyboardShortcutSetting(profile);
      base::RecordAction(base::UserMetricsAction(
          "GlicOsEntrypoint.ContextMenuSelection.OpenHotkeySettings"));
      break;
    }
    case IDC_GLIC_STATUS_ICON_MENU_REMOVE_ICON: {
      OpenGlicOsToggleSetting(profile);
      base::RecordAction(base::UserMetricsAction(
          "GlicOsEntrypoint.ContextMenuSelection.RemoveIcon"));
      break;
    }
    case IDC_GLIC_STATUS_ICON_MENU_SETTINGS: {
      OpenGlicSettingsPage(profile);
      base::RecordAction(base::UserMetricsAction(
          "GlicOsEntrypoint.ContextMenuSelection.OpenSettings"));
      break;
    }
    case IDC_GLIC_STATUS_ICON_MENU_EXIT: {
      chrome::CloseAllBrowsers();
      base::RecordAction(base::UserMetricsAction("Exit"));
      base::RecordAction(base::UserMetricsAction(
          "GlicOsEntrypoint.ContextMenuSelection.Exit"));
      break;
    }
    case IDC_GLIC_STATUS_ICON_MENU_CLOSE: {
      controller_->Close();
      base::RecordAction(base::UserMetricsAction(
          "GlicOsEntrypoint.ContextMenuSelection.CloseGlic"));
      break;
    }
    case IDC_GLIC_STATUS_ICON_MENU_TOGGLE: {
      controller_->Toggle(mojom::InvocationSource::kOsButtonMenu);
      base::RecordAction(base::UserMetricsAction(
          "GlicOsEntrypoint.ContextMenuSelection.ToggleGlic"));
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}

#if BUILDFLAG(IS_WIN)
void GlicStatusIcon::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  CHECK(!hkcu_themes_regkey_.Valid());
  in_dark_mode_ = observed_theme->preferred_color_scheme() ==
                  ui::NativeTheme::PreferredColorScheme::kDark;
  status_icon_->SetImage(GetIcon());
}
#endif

void GlicStatusIcon::OnBrowserAdded(Browser* browser) {
  UpdateVisibilityOfExitInContextMenu();
}

void GlicStatusIcon::OnBrowserRemoved(Browser* browser) {
  UpdateVisibilityOfExitInContextMenu();
}

void GlicStatusIcon::OnLastActiveGlicProfileChanged(Profile* profile) {
  panel_state_observer_.Reset();
  if (profile && !profile->ShutdownStarted()) {
    auto* service = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
    panel_state_observer_.Observe(&service->window_controller());
  }
  UpdateVisibilityOfShowAndCloseInContextMenu();
}

void GlicStatusIcon::PanelStateChanged(
    const mojom::PanelState& panel_state,
    const GlicWindowController::PanelStateContext& context) {
  // If GlicMultiInstance is enabled, show a single menu item for toggling the
  // UI and thus don't update based on state changes.
  if (GlicEnabling::IsMultiInstanceEnabled()) {
    return;
  }
  UpdateVisibilityOfShowAndCloseInContextMenu();
  status_icon_->SetToolTip(
      l10n_util::GetStringUTF16(GetTooltipMessageId(controller_->IsShowing())));
}

void GlicStatusIcon::UpdateHotkey(const ui::Accelerator& hotkey) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!context_menu_) {
    // TODO(crbug.com/454734385): Implement StatusTray functionality on
    // ChromeOS.
    return;
  }
#endif

  CHECK(context_menu_);
  context_menu_->SetAcceleratorForCommandId(IDC_GLIC_STATUS_ICON_MENU_SHOW,
                                            &hotkey);
  std::optional<size_t> show_menu_item_index =
      context_menu_->GetIndexOfCommandId(IDC_GLIC_STATUS_ICON_MENU_SHOW);
  CHECK(show_menu_item_index);
  context_menu_->SetForceShowAcceleratorForItemAt(show_menu_item_index.value(),
                                                  !hotkey.IsEmpty());
  context_menu_->SetAcceleratorForCommandId(IDC_GLIC_STATUS_ICON_MENU_CLOSE,
                                            &hotkey);
  std::optional<size_t> close_menu_item_index =
      context_menu_->GetIndexOfCommandId(IDC_GLIC_STATUS_ICON_MENU_CLOSE);
  CHECK(close_menu_item_index);
  context_menu_->SetForceShowAcceleratorForItemAt(close_menu_item_index.value(),
                                                  !hotkey.IsEmpty());
  std::optional<size_t> toggle_menu_item_index =
      context_menu_->GetIndexOfCommandId(IDC_GLIC_STATUS_ICON_MENU_TOGGLE);
  CHECK(toggle_menu_item_index);
  context_menu_->SetForceShowAcceleratorForItemAt(
      toggle_menu_item_index.value(), !hotkey.IsEmpty());
}

void GlicStatusIcon::UpdateVisibilityOfExitInContextMenu() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  if (context_menu_) {
    const bool is_visible = BrowserList::GetInstance()->empty();
    const std::optional<size_t> index =
        context_menu_->GetIndexOfCommandId(IDC_GLIC_STATUS_ICON_MENU_EXIT);
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

    context_menu_->SetCommandIdVisible(IDC_GLIC_STATUS_ICON_MENU_EXIT,
                                       is_visible);
  }
#endif
}

void GlicStatusIcon::UpdateVisibilityOfShowAndCloseInContextMenu() {
  // If GlicMultiInstance is enabled, always show a single menu item for
  // toggling the UI. Otherwise, show either the "Close" or "Show" menu item
  // accordingly.
  if (GlicEnabling::IsMultiInstanceEnabled()) {
    context_menu_->SetCommandIdVisible(IDC_GLIC_STATUS_ICON_MENU_TOGGLE, true);
    context_menu_->SetCommandIdVisible(IDC_GLIC_STATUS_ICON_MENU_CLOSE, false);
    context_menu_->SetCommandIdVisible(IDC_GLIC_STATUS_ICON_MENU_SHOW, false);
    return;
  }
  if (context_menu_) {
    context_menu_->SetCommandIdVisible(IDC_GLIC_STATUS_ICON_MENU_TOGGLE, false);
    const bool showing = controller_->IsShowing();
    context_menu_->SetCommandIdVisible(IDC_GLIC_STATUS_ICON_MENU_CLOSE,
                                       showing);
    context_menu_->SetCommandIdVisible(IDC_GLIC_STATUS_ICON_MENU_SHOW,
                                       !showing);
  }
}

gfx::ImageSkia GlicStatusIcon::GetIcon() const {
#if BUILDFLAG(IS_WIN)
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      glic::GetResourceID(in_dark_mode_ ? IDR_GLIC_STATUS_ICON_DARK
                                        : IDR_GLIC_STATUS_ICON_LIGHT));
#else
  // On Mac and Linux, theming is handled by the system and does not require
  // different images for light/dark mode.
  const auto& icon =
      glic::GlicVectorIconManager::GetVectorIcon(IDR_GLIC_STATUS_ICON);
  return gfx::CreateVectorIcon(icon, SK_ColorWHITE);
#endif
}

std::unique_ptr<StatusIconMenuModel> GlicStatusIcon::CreateStatusIconMenu() {
  std::unique_ptr<StatusIconMenuModel> menu =
      std::make_unique<StatusIconMenuModel>(this);

  menu->AddItem(IDC_GLIC_STATUS_ICON_MENU_TOGGLE,
                l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_MENU_TOGGLE));
  menu->AddItem(IDC_GLIC_STATUS_ICON_MENU_CLOSE,
                l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_MENU_CLOSE));
  menu->AddItem(IDC_GLIC_STATUS_ICON_MENU_SHOW,
                l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_MENU_SHOW));

  menu->AddSeparator(ui::NORMAL_SEPARATOR);

  menu->AddItem(IDC_GLIC_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT,
                l10n_util::GetStringUTF16(
                    IDS_GLIC_STATUS_ICON_MENU_CUSTOMIZE_KEYBOARD_SHORTCUT));
  menu->AddItem(IDC_GLIC_STATUS_ICON_MENU_SETTINGS,
                l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_MENU_SETTINGS));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  menu->AddSeparator(ui::NORMAL_SEPARATOR);
  menu->AddItem(IDC_GLIC_STATUS_ICON_MENU_EXIT,
                l10n_util::GetStringUTF16(IDS_GLIC_STATUS_ICON_MENU_EXIT));
#endif
  return menu;
}

#if BUILDFLAG(IS_WIN)
void GlicStatusIcon::RegisterThemesRegkeyObserver() {
  CHECK(hkcu_themes_regkey_.Valid());
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  hkcu_themes_regkey_.StartWatching(base::BindOnce(
      [](GlicStatusIcon* icon) {
        icon->UpdateForThemesRegkey();
        // `StartWatching()`'s callback is one-shot and must be re-registered
        // for future notifications.
        icon->RegisterThemesRegkeyObserver();
      },
      base::Unretained(this)));
}

void GlicStatusIcon::UpdateForThemesRegkey() {
  CHECK(hkcu_themes_regkey_.Valid());
  DWORD system_uses_light_theme = 1;
  hkcu_themes_regkey_.ReadValueDW(L"SystemUsesLightTheme",
                                  &system_uses_light_theme);
  in_dark_mode_ = !system_uses_light_theme;
  status_icon_->SetImage(GetIcon());
}
#endif

}  // namespace glic
