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
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/resources/glic_resources.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "glic_status_icon.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/background/glic/os_icon_provider_mac.h"
#include "chrome/browser/browser_process.h"
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif                                                // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/background/glic/glic_status_icon_win.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/background/glic/glic_status_icon_chromeos.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

int GetTooltipMessageId(bool panel_showing) {
  switch (chrome::GetChannel()) {
    case version_info::Channel::CANARY:
      return IDS_GLIC_STATUS_ICON_TOOLTIP_TOGGLE_CANARY;
    case version_info::Channel::DEV:
      return IDS_GLIC_STATUS_ICON_TOOLTIP_TOGGLE_DEV;
    case version_info::Channel::BETA:
      return IDS_GLIC_STATUS_ICON_TOOLTIP_TOGGLE_BETA;
    default:
      return IDS_GLIC_STATUS_ICON_TOOLTIP_TOGGLE;
  }
}

}  // namespace

namespace glic {

// static
std::unique_ptr<GlicStatusIcon> GlicStatusIcon::Create(
    GlicController* controller,
    StatusTray* status_tray) {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<GlicStatusIconWin>(controller, status_tray);
#elif BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<GlicStatusIconChromeOS>(controller, status_tray);
#else
  return std::make_unique<GlicStatusIcon>(controller, status_tray);
#endif
}

GlicStatusIcon::GlicStatusIcon(GlicController* controller,
                               StatusTray* status_tray)
    : controller_(controller),
      status_tray_(status_tray)
#if BUILDFLAG(IS_MAC)
      ,
      os_icon_provider_mac_(*g_browser_process->local_state(), *this)
#endif
{
}

GlicStatusIcon::~GlicStatusIcon() {
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

void GlicStatusIcon::Init() {
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
#endif  // BUILDFLAG(IS_MAC)
  std::unique_ptr<StatusIconMenuModel> menu = CreateStatusIconMenu();
  context_menu_ = menu.get();
  status_icon_->SetContextMenu(std::move(menu));

  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
  UpdateVisibilityOfExitInContextMenu();
}

void GlicStatusIcon::OnStatusIconClicked() {
  controller_->Toggle(mojom::InvocationSource::kOsButton);
}

void GlicStatusIcon::ExecuteCommand(int command_id, int event_flags) {
  auto* profile = GlicProfileManager::GetInstance()->GetProfileForLaunch();
  switch (command_id) {
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

void GlicStatusIcon::OnBrowserCreated(BrowserWindowInterface* browser) {
  UpdateVisibilityOfExitInContextMenu();
}

void GlicStatusIcon::OnBrowserClosed(BrowserWindowInterface* browser) {
  UpdateVisibilityOfExitInContextMenu();
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
  std::optional<size_t> toggle_menu_item_index =
      context_menu_->GetIndexOfCommandId(IDC_GLIC_STATUS_ICON_MENU_TOGGLE);
  CHECK(toggle_menu_item_index);
  context_menu_->SetAcceleratorForCommandId(IDC_GLIC_STATUS_ICON_MENU_TOGGLE,
                                            &hotkey);
  context_menu_->SetForceShowAcceleratorForItemAt(
      toggle_menu_item_index.value(), !hotkey.IsEmpty());
}

void GlicStatusIcon::UpdateVisibilityOfExitInContextMenu() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  if (context_menu_) {
    const bool is_visible = GlobalBrowserCollection::GetInstance()->IsEmpty();
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

gfx::ImageSkia GlicStatusIcon::GetIcon() const {
  // On Mac and Linux, theming is handled by the system,. whereas ChromeOS and
  // Win need theme aware icons. (See GetIcon() implementations of
  // GlicStatusIconWin and GlicStatusIconChromeOS)
#if BUILDFLAG(IS_MAC)
  return os_icon_provider_mac_.GetIcon();
#else
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

void GlicStatusIcon::SetIcon(const gfx::ImageSkia& icon) {
  if (status_icon_) {
    status_icon_->SetImage(icon);
  }
}

}  // namespace glic
