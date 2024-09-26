// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_context_menu.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/ash/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/extension_app_utils.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/standalone_browser_extension_app_context_menu.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ash/settings/app_management/app_management_uma.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/gfx/vector_icon_types.h"

namespace {
bool MenuItemHasLauncherContext(const extensions::MenuItem* item) {
  return item->contexts().Contains(extensions::MenuItem::LAUNCHER);
}

apps::WindowMode ConvertUseLaunchTypeCommandToWindowMode(int command_id) {
  DCHECK(command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
         command_id < ash::USE_LAUNCH_TYPE_COMMAND_END);
  switch (command_id) {
    case ash::USE_LAUNCH_TYPE_REGULAR:
      return apps::WindowMode::kBrowser;
    case ash::USE_LAUNCH_TYPE_WINDOW:
      return apps::WindowMode::kWindow;
    case ash::USE_LAUNCH_TYPE_TABBED_WINDOW:
      return apps::WindowMode::kTabbedWindow;
    case ash::DEPRECATED_USE_LAUNCH_TYPE_PINNED:
    case ash::DEPRECATED_USE_LAUNCH_TYPE_FULLSCREEN:
      [[fallthrough]];
    default:
      NOTREACHED_IN_MIGRATION();
      return apps::WindowMode::kUnknown;
  }
}

void CreateNewWindow(bool incognito, bool post_task) {
  if (post_task) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(CreateNewWindow, incognito, /*post_task=*/false));
    return;
  }

  ash::NewWindowDelegate::GetInstance()->NewWindow(
      incognito, /*should_trigger_session_restore=*/false);
}

void ShowOptionsPage(AppListControllerDelegate* controller,
                     Profile* profile,
                     const std::string& app_id,
                     bool post_task) {
  DCHECK(controller);
  DCHECK(profile);

  if (post_task) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(ShowOptionsPage, controller, profile, app_id,
                                  /*post_task=*/false));
    return;
  }

  controller->ShowOptionsPage(profile, app_id);
}

void ExecuteLaunchCommand(app_list::AppContextMenuDelegate* delegate,
                          int event_flags,
                          bool post_task) {
  DCHECK(delegate);
  if (post_task) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(ExecuteLaunchCommand, delegate, event_flags,
                                  /*post_task=*/false));
    return;
  }

  delegate->ExecuteLaunchCommand(event_flags);
}

void MaybeCloseFullRestoreServiceNotification(Profile* profile) {
  if (auto* full_restore_service =
          ash::full_restore::FullRestoreServiceFactory::GetForProfile(
              profile)) {
    full_restore_service->MaybeCloseNotification();
  }
}

}  // namespace

AppServiceContextMenu::AppServiceContextMenu(
    app_list::AppContextMenuDelegate* delegate,
    Profile* profile,
    const std::string& app_id,
    AppListControllerDelegate* controller,
    ash::AppListItemContext item_context)
    : AppContextMenu(delegate, profile, app_id, controller, item_context),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(profile)) {
  proxy_->AppRegistryCache().ForOneApp(
      app_id, [this](const apps::AppUpdate& update) {
        app_type_ = apps_util::IsInstalled(update.Readiness())
                        ? update.AppType()
                        : apps::AppType::kUnknown;
        is_platform_app_ = update.IsPlatformApp().value_or(false);
      });
  // StandaloneBrowserExtension creates its own context menus for platform apps.
  if (app_type_ == apps::AppType::kStandaloneBrowserChromeApp &&
      is_platform_app_) {
    standalone_browser_extension_menu_ =
        std::make_unique<StandaloneBrowserExtensionAppContextMenu>(
            app_id, StandaloneBrowserExtensionAppContextMenu::Source::kAppList);
  }
}

AppServiceContextMenu::~AppServiceContextMenu() = default;

ui::ImageModel AppServiceContextMenu::GetIconForCommandId(
    int command_id) const {
  if (command_id == ash::LAUNCH_NEW) {
    const gfx::VectorIcon& icon =
        GetMenuItemVectorIcon(command_id, launch_new_string_id_);
    return ui::ImageModel::FromVectorIcon(
        icon, apps::GetColorIdForMenuItemIcon(), ash::kAppContextMenuIconSize);
  }
  return AppContextMenu::GetIconForCommandId(command_id);
}

std::u16string AppServiceContextMenu::GetLabelForCommandId(
    int command_id) const {
  if (command_id == ash::LAUNCH_NEW)
    return l10n_util::GetStringUTF16(launch_new_string_id_);

  return AppContextMenu::GetLabelForCommandId(command_id);
}

void AppServiceContextMenu::GetMenuModel(GetMenuModelCallback callback) {
  if (app_type_ == apps::AppType::kUnknown) {
    std::move(callback).Run(nullptr);
    return;
  }

  // StandaloneBrowserExtension handles its own context menus for platform apps.
  // Forward to that class.
  if (app_type_ == apps::AppType::kStandaloneBrowserChromeApp &&
      is_platform_app_) {
    standalone_browser_extension_menu_->GetMenuModel(std::move(callback));
    return;
  }

  proxy_->GetMenuModel(
      app_id(), apps::MenuType::kAppList, controller()->GetAppListDisplayId(),
      base::BindOnce(&AppServiceContextMenu::OnGetMenuModel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AppServiceContextMenu::ExecuteCommand(int command_id, int event_flags) {
  // StandaloneBrowserExtension handles its own context menus. Forward to that
  // class.
  if (standalone_browser_extension_menu_) {
    standalone_browser_extension_menu_->ExecuteCommand(command_id, event_flags);
    return;
  }

  // Place new windows on the same display as the context menu.
  display::ScopedDisplayForNewWindows scoped_display(
      controller()->GetAppListDisplayId());
  switch (command_id) {
    case ash::LAUNCH_NEW:
      ExecuteLaunchCommand(delegate(), event_flags, /*post_task=*/true);
      MaybeCloseFullRestoreServiceNotification(profile());
      break;

    case ash::SHOW_APP_INFO:
      ShowAppInfo();
      MaybeCloseFullRestoreServiceNotification(profile());
      break;

    case ash::OPTIONS:
      ShowOptionsPage(controller(), profile(), app_id(), /*post_task=*/true);
      MaybeCloseFullRestoreServiceNotification(profile());
      break;

    case ash::UNINSTALL:
      controller()->UninstallApp(profile(), app_id());
      break;

    case ash::SETTINGS:
      if (app_id() == guest_os::kTerminalSystemAppId) {
        guest_os::LaunchTerminalSettings(profile(),
                                         controller()->GetAppListDisplayId());
        MaybeCloseFullRestoreServiceNotification(profile());
      }
      break;

    case ash::APP_CONTEXT_MENU_NEW_WINDOW:
    case ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW: {
      const bool is_incognito =
          command_id == ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW;
      if (app_type_ == apps::AppType::kStandaloneBrowser) {
        crosapi::BrowserManager::Get()->NewWindow(
            is_incognito, /*should_trigger_session_restore=*/false);
      } else {
        // Create browser asynchronously to prevent this AppServiceContextMenu
        // object to be deleted when the browser window is shown.
        CreateNewWindow(is_incognito, /*post_task=*/true);
      }
      MaybeCloseFullRestoreServiceNotification(profile());
      break;
    }
    case ash::SHUTDOWN_GUEST_OS:
      if (app_id() == guest_os::kTerminalSystemAppId) {
        crostini::CrostiniManager::GetForProfile(profile())->StopRunningVms(
            base::DoNothing());
      } else if (app_id() == plugin_vm::kPluginVmShelfAppId) {
        plugin_vm::PluginVmManagerFactory::GetForProfile(profile())
            ->StopPluginVm(plugin_vm::kPluginVmName, /*force=*/false);
      } else {
        LOG(ERROR) << "App " << app_id()
                   << " should not have a shutdown guest OS command.";
      }
      break;
    default:
      if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
          command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
        launch_new_string_id_ =
            apps::StringIdForUseLaunchTypeCommand(command_id);

        if (app_type_ == apps::AppType::kWeb &&
            command_id == ash::USE_LAUNCH_TYPE_TABBED_WINDOW) {
          proxy_->SetWindowMode(app_id(), apps::WindowMode::kTabbedWindow);
          return;
        }

        SetLaunchType(command_id);
        return;
      }

      if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
              command_id)) {
        extension_menu_items_->ExecuteCommand(command_id, nullptr, nullptr,
                                              content::ContextMenuParams());
        return;
      }

      if (command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
          command_id <= ash::LAUNCH_APP_SHORTCUT_LAST) {
        ExecutePublisherContextMenuCommand(command_id);
        return;
      }

      AppContextMenu::ExecuteCommand(command_id, event_flags);
  }
}

bool AppServiceContextMenu::IsCommandIdChecked(int command_id) const {
  // StandaloneBrowserExtension handles its own context menus. Forward to that
  // class.
  if (standalone_browser_extension_menu_) {
    return standalone_browser_extension_menu_->IsCommandIdChecked(command_id);
  }

  switch (app_type_) {
    case apps::AppType::kWeb:
    case apps::AppType::kStandaloneBrowserChromeApp:  // hosted app
      if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
          command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
        auto user_window_mode = apps::WindowMode::kUnknown;
        proxy_->AppRegistryCache().ForOneApp(
            app_id(), [&user_window_mode](const apps::AppUpdate& update) {
              user_window_mode = update.WindowMode();
            });
        return user_window_mode != apps::WindowMode::kUnknown &&
               user_window_mode ==
                   ConvertUseLaunchTypeCommandToWindowMode(command_id);
      }
      return AppContextMenu::IsCommandIdChecked(command_id);

    case apps::AppType::kChromeApp:
      if (command_id >= ash::USE_LAUNCH_TYPE_COMMAND_START &&
          command_id < ash::USE_LAUNCH_TYPE_COMMAND_END) {
        return static_cast<int>(
                   controller()->GetExtensionLaunchType(profile(), app_id())) +
                   ash::USE_LAUNCH_TYPE_COMMAND_START ==
               command_id;
      } else if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(
                     command_id)) {
        return extension_menu_items_->IsCommandIdChecked(command_id);
      }
      return AppContextMenu::IsCommandIdChecked(command_id);

    case apps::AppType::kArc:
      [[fallthrough]];
    case apps::AppType::kCrostini:
      [[fallthrough]];
    case apps::AppType::kBuiltIn:
      [[fallthrough]];
    case apps::AppType::kPluginVm:
      [[fallthrough]];
    case apps::AppType::kBorealis:
      [[fallthrough]];
    default:
      return AppContextMenu::IsCommandIdChecked(command_id);
  }
}

bool AppServiceContextMenu::IsCommandIdEnabled(int command_id) const {
  // StandaloneBrowserExtension handles its own context menus. Forward to that
  // class.
  if (standalone_browser_extension_menu_) {
    return standalone_browser_extension_menu_->IsCommandIdEnabled(command_id);
  }

  if (extensions::ContextMenuMatcher::IsExtensionsCustomCommandId(command_id) &&
      extension_menu_items_) {
    return extension_menu_items_->IsCommandIdEnabled(command_id);
  }
  return AppContextMenu::IsCommandIdEnabled(command_id);
}

bool AppServiceContextMenu::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == ash::LAUNCH_NEW ||
         AppContextMenu::IsItemForCommandIdDynamic(command_id);
}

void AppServiceContextMenu::OnGetMenuModel(GetMenuModelCallback callback,
                                           apps::MenuItems menu_items) {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  submenu_ = std::make_unique<ui::SimpleMenuModel>(this);
  size_t index = 0;

  if (!menu_items.items.empty() &&
      menu_items.items[0]->command_id == ash::LAUNCH_NEW) {
    apps::PopulateLaunchNewItemFromMenuItem(menu_items.items[0],
                                            menu_model.get(), submenu_.get(),
                                            &launch_new_string_id_);
    ++index;
  }

  // The special rule to ensure that FilesManager's first menu item is "New
  // window".
  const bool build_extension_menu_before_default =
      (app_type_ == apps::AppType::kChromeApp &&
       app_id() == extension_misc::kFilesManagerAppId);

  if (build_extension_menu_before_default)
    BuildExtensionAppShortcutsMenu(menu_model.get());

  // Create default items for non-Remote apps.
  if (app_id() != app_constants::kChromeAppId &&
      app_id() != app_constants::kLacrosAppId &&
      app_type_ != apps::AppType::kUnknown &&
      app_type_ != apps::AppType::kRemote) {
    app_list::AppContextMenu::BuildMenu(menu_model.get());
  }

  if (!build_extension_menu_before_default)
    BuildExtensionAppShortcutsMenu(menu_model.get());

  app_shortcut_items_ = std::make_unique<apps::AppShortcutItems>();
  for (size_t i = index; i < menu_items.items.size(); i++) {
    if (menu_items.items[i]->command_id == ash::LAUNCH_NEW) {
      // Crostini apps have `LAUNCH_NEW` menu item at non-0 position.
      apps::PopulateLaunchNewItemFromMenuItem(menu_items.items[i],
                                              menu_model.get(), submenu_.get(),
                                              &launch_new_string_id_);
    } else if (menu_items.items[i]->type == apps::MenuItemType::kCommand) {
      AddContextMenuOption(
          menu_model.get(),
          static_cast<ash::CommandId>(menu_items.items[i]->command_id),
          menu_items.items[i]->string_id);
    } else {
      apps::PopulateItemFromMenuItem(menu_items.items[i], menu_model.get(),
                                     app_shortcut_items_.get());
    }
  }

  AddReorderMenuOption(menu_model.get());

  std::move(callback).Run(std::move(menu_model));
}

void AppServiceContextMenu::BuildExtensionAppShortcutsMenu(
    ui::SimpleMenuModel* menu_model) {
  extension_menu_items_ = std::make_unique<extensions::ContextMenuMatcher>(
      profile(), this, menu_model,
      base::BindRepeating(MenuItemHasLauncherContext));

  // Assign unique IDs to commands added by the app itself.
  int index = ash::USE_LAUNCH_TYPE_COMMAND_END;
  extension_menu_items_->AppendExtensionItems(
      extensions::MenuItem::ExtensionKey(app_id()), std::u16string(), &index,
      false /*is_action_menu*/);

  const int appended_count = index - ash::USE_LAUNCH_TYPE_COMMAND_END;
  app_list::AddMenuItemIconsForSystemApps(
      app_id(), menu_model, menu_model->GetItemCount() - appended_count,
      appended_count);
}

void AppServiceContextMenu::ShowAppInfo() {
  if (app_type_ == apps::AppType::kArc) {
    chrome::ShowAppManagementPage(
        profile(), app_id(),
        ash::settings::AppManagementEntryPoint::kAppListContextMenuAppInfoArc);
    return;
  }

  controller()->DoShowAppInfoFlow(profile(), app_id());
}

void AppServiceContextMenu::SetLaunchType(int command_id) {
  switch (app_type_) {
    case apps::AppType::kWeb:
    case apps::AppType::kStandaloneBrowserChromeApp: {
      // Web apps and standalone browser hosted apps can only toggle between
      // kWindow and kBrowser.
      apps::WindowMode user_window_mode =
          ConvertUseLaunchTypeCommandToWindowMode(command_id);
      if (user_window_mode != apps::WindowMode::kUnknown) {
        proxy_->SetWindowMode(app_id(), user_window_mode);
      }
      return;
    }
    case apps::AppType::kChromeApp: {
      // Hosted apps can only toggle between LAUNCH_TYPE_WINDOW and
      // LAUNCH_TYPE_REGULAR.
      extensions::LaunchType launch_type =
          (controller()->GetExtensionLaunchType(profile(), app_id()) ==
           extensions::LAUNCH_TYPE_WINDOW)
              ? extensions::LAUNCH_TYPE_REGULAR
              : extensions::LAUNCH_TYPE_WINDOW;
      controller()->SetExtensionLaunchType(profile(), app_id(), launch_type);
      return;
    }
    case apps::AppType::kArc:
      [[fallthrough]];
    case apps::AppType::kCrostini:
      [[fallthrough]];
    case apps::AppType::kBuiltIn:
      [[fallthrough]];
    case apps::AppType::kPluginVm:
      [[fallthrough]];
    case apps::AppType::kBorealis:
      [[fallthrough]];
    default:
      return;
  }
}

void AppServiceContextMenu::ExecutePublisherContextMenuCommand(int command_id) {
  DCHECK(command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
         command_id <= ash::LAUNCH_APP_SHORTCUT_LAST);
  const size_t index = command_id - ash::LAUNCH_APP_SHORTCUT_FIRST;
  DCHECK(app_shortcut_items_);
  DCHECK_LT(index, app_shortcut_items_->size());

  proxy_->ExecuteContextMenuCommand(app_id(), command_id,
                                    app_shortcut_items_->at(index).shortcut_id,
                                    controller()->GetAppListDisplayId());
}
