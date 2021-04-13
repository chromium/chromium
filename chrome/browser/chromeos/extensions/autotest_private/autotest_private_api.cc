// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/autotest_private/autotest_private_api.h"

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/accessibility_controller.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/assistant/assistant_client.h"
#include "ash/public/cpp/autotest_ambient_api.h"
#include "ash/public/cpp/autotest_desks_api.h"
#include "ash/public/cpp/autotest_private_api_utils.h"
#include "ash/public/cpp/desks_helper.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shelf_ui_info.h"
#include "ash/public/cpp/split_view_test_api.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/shell.h"
#include "ash/wm/wm_event.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/scoped_observer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_installer.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/crostini/crostini_uninstaller_view.h"
#include "chrome/browser/ui/views/plugin_vm/plugin_vm_installer_view.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_dialog.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_ui.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/extensions/api/autotest_private.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/policy_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/user_manager/user_manager.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/filename_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/compositor/throughput_tracker_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

using extensions::mojom::ManifestLocation;

namespace extensions {
namespace {

using chromeos::PrinterClass;

constexpr char kCrostiniNotAvailableForCurrentUserError[] =
    "Crostini is not available for the current user";

int AccessArray(const volatile int arr[], const volatile int* index) {
  return arr[*index];
}

std::unique_ptr<base::ListValue> GetHostPermissions(const Extension* ext,
                                                    bool effective_perm) {
  const PermissionsData* permissions_data = ext->permissions_data();

  const URLPatternSet* pattern_set = nullptr;
  URLPatternSet effective_hosts;
  if (effective_perm) {
    effective_hosts = permissions_data->GetEffectiveHostPermissions();
    pattern_set = &effective_hosts;
  } else {
    pattern_set = &permissions_data->active_permissions().explicit_hosts();
  }

  auto permissions = std::make_unique<base::ListValue>();
  for (const auto& perm : *pattern_set)
    permissions->AppendString(perm.GetAsString());

  return permissions;
}

std::unique_ptr<base::ListValue> GetAPIPermissions(const Extension* ext) {
  auto permissions = std::make_unique<base::ListValue>();
  std::set<std::string> perm_list =
      ext->permissions_data()->active_permissions().GetAPIsAsStrings();
  for (std::set<std::string>::const_iterator perm = perm_list.begin();
       perm != perm_list.end(); ++perm) {
    permissions->AppendString(*perm);
  }
  return permissions;
}

bool IsTestMode(content::BrowserContext* context) {
  return AutotestPrivateAPI::GetFactoryInstance()->Get(context)->test_mode();
}

std::string ConvertToString(message_center::NotificationType type) {
  switch (type) {
    case message_center::NOTIFICATION_TYPE_SIMPLE:
      return "simple";
    case message_center::NOTIFICATION_TYPE_BASE_FORMAT:
      return "base_format";
    case message_center::NOTIFICATION_TYPE_IMAGE:
      return "image";
    case message_center::NOTIFICATION_TYPE_MULTIPLE:
      return "multiple";
    case message_center::NOTIFICATION_TYPE_PROGRESS:
      return "progress";
    case message_center::NOTIFICATION_TYPE_CUSTOM:
      return "custom";
  }
  return "unknown";
}

std::unique_ptr<base::DictionaryValue> MakeDictionaryFromNotification(
    const message_center::Notification& notification) {
  auto result = std::make_unique<base::DictionaryValue>();
  result->SetString("id", notification.id());
  result->SetString("type", ConvertToString(notification.type()));
  result->SetString("title", notification.title());
  result->SetString("message", notification.message());
  result->SetInteger("priority", notification.priority());
  result->SetInteger("progress", notification.progress());
  return result;
}

std::string GetPrinterType(PrinterClass type) {
  switch (type) {
    case PrinterClass::kSaved:
      return "configured";
    case PrinterClass::kEnterprise:
      return "enterprise";
    case PrinterClass::kAutomatic:
      return "automatic";
    case PrinterClass::kDiscovered:
      return "discovered";
    default:
      return "unknown";
  }
}

api::autotest_private::ShelfItemType GetShelfItemType(ash::ShelfItemType type) {
  switch (type) {
    case ash::TYPE_PINNED_APP:
      return api::autotest_private::ShelfItemType::SHELF_ITEM_TYPE_PINNEDAPP;
    case ash::TYPE_BROWSER_SHORTCUT:
      return api::autotest_private::ShelfItemType::
          SHELF_ITEM_TYPE_BROWSERSHORTCUT;
    case ash::TYPE_APP:
      return api::autotest_private::ShelfItemType::SHELF_ITEM_TYPE_APP;
    case ash::TYPE_UNPINNED_BROWSER_SHORTCUT:
      return api::autotest_private::ShelfItemType::
          SHELF_ITEM_TYPE_UNPINNEDBROWSERSHORTCUT;
    case ash::TYPE_DIALOG:
      return api::autotest_private::ShelfItemType::SHELF_ITEM_TYPE_DIALOG;
    case ash::TYPE_UNDEFINED:
      return api::autotest_private::ShelfItemType::SHELF_ITEM_TYPE_NONE;
  }
  NOTREACHED();
  return api::autotest_private::ShelfItemType::SHELF_ITEM_TYPE_NONE;
}

api::autotest_private::ShelfItemStatus GetShelfItemStatus(
    ash::ShelfItemStatus status) {
  switch (status) {
    case ash::STATUS_CLOSED:
      return api::autotest_private::ShelfItemStatus::SHELF_ITEM_STATUS_CLOSED;
    case ash::STATUS_RUNNING:
      return api::autotest_private::ShelfItemStatus::SHELF_ITEM_STATUS_RUNNING;
    case ash::STATUS_ATTENTION:
      return api::autotest_private::ShelfItemStatus::
          SHELF_ITEM_STATUS_ATTENTION;
  }
  NOTREACHED();
  return api::autotest_private::ShelfItemStatus::SHELF_ITEM_STATUS_NONE;
}

api::autotest_private::AppType GetAppType(apps::mojom::AppType type) {
  switch (type) {
    case apps::mojom::AppType::kArc:
      return api::autotest_private::AppType::APP_TYPE_ARC;
    case apps::mojom::AppType::kBuiltIn:
      return api::autotest_private::AppType::APP_TYPE_BUILTIN;
    case apps::mojom::AppType::kCrostini:
      return api::autotest_private::AppType::APP_TYPE_CROSTINI;
    case apps::mojom::AppType::kExtension:
      return api::autotest_private::AppType::APP_TYPE_EXTENSION;
    case apps::mojom::AppType::kPluginVm:
      return api::autotest_private::AppType::APP_TYPE_PLUGINVM;
    case apps::mojom::AppType::kWeb:
    case apps::mojom::AppType::kSystemWeb:
      return api::autotest_private::AppType::APP_TYPE_WEB;
    case apps::mojom::AppType::kUnknown:
      return api::autotest_private::AppType::APP_TYPE_NONE;
    case apps::mojom::AppType::kMacOs:
      return api::autotest_private::AppType::APP_TYPE_MACOS;
    case apps::mojom::AppType::kLacros:
      return api::autotest_private::AppType::APP_TYPE_LACROS;
    case apps::mojom::AppType::kRemote:
      return api::autotest_private::AppType::APP_TYPE_REMOTE;
    case apps::mojom::AppType::kBorealis:
      return api::autotest_private::AppType::APP_TYPE_BOREALIS;
  }
  NOTREACHED();
  return api::autotest_private::AppType::APP_TYPE_NONE;
}

api::autotest_private::AppInstallSource GetAppInstallSource(
    apps::mojom::InstallSource source) {
  switch (source) {
    case apps::mojom::InstallSource::kUnknown:
      return api::autotest_private::AppInstallSource::
          APP_INSTALL_SOURCE_UNKNOWN;
    case apps::mojom::InstallSource::kSystem:
      return api::autotest_private::AppInstallSource::APP_INSTALL_SOURCE_SYSTEM;
    case apps::mojom::InstallSource::kPolicy:
      return api::autotest_private::AppInstallSource::APP_INSTALL_SOURCE_POLICY;
    case apps::mojom::InstallSource::kOem:
      return api::autotest_private::AppInstallSource::APP_INSTALL_SOURCE_OEM;
    case apps::mojom::InstallSource::kDefault:
      return api::autotest_private::AppInstallSource::
          APP_INSTALL_SOURCE_DEFAULT;
    case apps::mojom::InstallSource::kSync:
      return api::autotest_private::AppInstallSource::APP_INSTALL_SOURCE_SYNC;
    case apps::mojom::InstallSource::kUser:
      return api::autotest_private::AppInstallSource::APP_INSTALL_SOURCE_USER;
  }
  NOTREACHED();
  return api::autotest_private::AppInstallSource::APP_INSTALL_SOURCE_NONE;
}

api::autotest_private::AppWindowType GetAppWindowType(ash::AppType type) {
  switch (type) {
    case ash::AppType::ARC_APP:
      return api::autotest_private::AppWindowType::APP_WINDOW_TYPE_ARCAPP;
    case ash::AppType::SYSTEM_APP:
      return api::autotest_private::AppWindowType::APP_WINDOW_TYPE_SYSTEMAPP;
    case ash::AppType::CROSTINI_APP:
      return api::autotest_private::AppWindowType::APP_WINDOW_TYPE_CROSTINIAPP;
    case ash::AppType::CHROME_APP:
      return api::autotest_private::AppWindowType::APP_WINDOW_TYPE_EXTENSIONAPP;
    case ash::AppType::BROWSER:
      return api::autotest_private::AppWindowType::APP_WINDOW_TYPE_BROWSER;
    case ash::AppType::LACROS:
      return api::autotest_private::AppWindowType::APP_WINDOW_TYPE_LACROS;
    case ash::AppType::NON_APP:
      return api::autotest_private::AppWindowType::APP_WINDOW_TYPE_NONE;
      // TODO(oshima): Investigate if we want to have "extension" type.
  }
  NOTREACHED();
  return api::autotest_private::AppWindowType::APP_WINDOW_TYPE_NONE;
}

api::autotest_private::AppReadiness GetAppReadiness(
    apps::mojom::Readiness readiness) {
  switch (readiness) {
    case apps::mojom::Readiness::kReady:
      return api::autotest_private::AppReadiness::APP_READINESS_READY;
    case apps::mojom::Readiness::kDisabledByBlocklist:
      return api::autotest_private::AppReadiness::
          APP_READINESS_DISABLEDBYBLACKLIST;
    case apps::mojom::Readiness::kDisabledByPolicy:
      return api::autotest_private::AppReadiness::
          APP_READINESS_DISABLEDBYPOLICY;
    case apps::mojom::Readiness::kDisabledByUser:
      return api::autotest_private::AppReadiness::APP_READINESS_DISABLEDBYUSER;
    case apps::mojom::Readiness::kTerminated:
      return api::autotest_private::AppReadiness::APP_READINESS_TERMINATED;
    case apps::mojom::Readiness::kUninstalledByUser:
      return api::autotest_private::AppReadiness::
          APP_READINESS_UNINSTALLEDBYUSER;
    case apps::mojom::Readiness::kRemoved:
      return api::autotest_private::AppReadiness::APP_READINESS_REMOVED;
    case apps::mojom::Readiness::kUnknown:
      return api::autotest_private::AppReadiness::APP_READINESS_NONE;
  }
  NOTREACHED();
  return api::autotest_private::AppReadiness::APP_READINESS_NONE;
}

api::autotest_private::HotseatState GetHotseatState(
    ash::HotseatState hotseat_state) {
  switch (hotseat_state) {
    case ash::HotseatState::kNone:
      return api::autotest_private::HotseatState::HOTSEAT_STATE_NONE;
    case ash::HotseatState::kHidden:
      return api::autotest_private::HotseatState::HOTSEAT_STATE_HIDDEN;
    case ash::HotseatState::kShownClamshell:
      return api::autotest_private::HotseatState::HOTSEAT_STATE_SHOWNCLAMSHELL;
    case ash::HotseatState::kShownHomeLauncher:
      return api::autotest_private::HotseatState::
          HOTSEAT_STATE_SHOWNHOMELAUNCHER;
    case ash::HotseatState::kExtended:
      return api::autotest_private::HotseatState::HOTSEAT_STATE_EXTENDED;
  }

  NOTREACHED();
}

std::unique_ptr<bool> ConvertMojomOptionalBool(
    apps::mojom::OptionalBool optional) {
  switch (optional) {
    case apps::mojom::OptionalBool::kTrue:
      return std::make_unique<bool>(true);
    case apps::mojom::OptionalBool::kFalse:
      return std::make_unique<bool>(false);
    case apps::mojom::OptionalBool::kUnknown:
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

// Helper function to set whitelisted user pref based on |pref_name| with any
// specific pref validations. Returns error messages if any.
std::string SetWhitelistedPref(Profile* profile,
                               const std::string& pref_name,
                               const base::Value& value) {
  // Special case for the preference that is stored in the "Local State"
  // profile.
  if (pref_name == prefs::kEnableAdbSideloadingRequested) {
    DCHECK(value.is_bool());
    g_browser_process->local_state()->Set(pref_name, value);
    return std::string();
  }

  if (pref_name == chromeos::assistant::prefs::kAssistantEnabled) {
    if (!value.is_bool())
      return "Invalid value type.";
    // Validate the Assistant service allowed state.
    chromeos::assistant::AssistantAllowedState allowed_state =
        assistant::IsAssistantAllowedForProfile(profile);
    if (allowed_state != chromeos::assistant::AssistantAllowedState::ALLOWED) {
      return base::StringPrintf("Assistant not allowed - state: %d",
                                allowed_state);
    }
  } else if (pref_name == chromeos::assistant::prefs::kAssistantConsentStatus) {
    if (!value.is_int())
      return "Invalid value type.";
    if (!profile->GetPrefs()->GetBoolean(
            chromeos::assistant::prefs::kAssistantEnabled)) {
      return "Unable to set the pref because Assistant has not been enabled.";
    }
  } else if (pref_name ==
                 chromeos::assistant::prefs::kAssistantContextEnabled ||
             pref_name ==
                 chromeos::assistant::prefs::kAssistantHotwordEnabled) {
    if (!value.is_bool())
      return "Invalid value type.";
    // Assistant service must be enabled first for those prefs to take effect.
    if (!profile->GetPrefs()->GetBoolean(
            chromeos::assistant::prefs::kAssistantEnabled)) {
      return std::string(
          "Unable to set the pref because Assistant has not been enabled.");
    }
  } else if (pref_name ==
             ash::prefs::kAssistantNumSessionsWhereOnboardingShown) {
    if (!value.is_int())
      return "Invalid value type.";
  } else if (pref_name == ash::prefs::kAccessibilityVirtualKeyboardEnabled) {
    DCHECK(value.is_bool());
  } else if (pref_name == prefs::kLanguagePreloadEngines) {
    DCHECK(value.is_string());
  } else if (pref_name == plugin_vm::prefs::kPluginVmCameraAllowed) {
    DCHECK(value.is_bool());
  } else if (pref_name == plugin_vm::prefs::kPluginVmMicAllowed) {
    DCHECK(value.is_bool());
  } else {
    return "The pref " + pref_name + " is not whitelisted.";
  }

  // Set value for the specified user pref after validation.
  profile->GetPrefs()->Set(pref_name, value);

  return std::string();
}

// Returns the ARC app window that associates with |package_name|. Note there
// might be more than 1 windows that have the same package name. This function
// just returns the first window it finds.
aura::Window* GetArcAppWindow(const std::string& package_name) {
  for (auto* window : ChromeLauncherController::instance()->GetArcWindows()) {
    std::string* pkg_name = window->GetProperty(ash::kArcPackageNameKey);
    if (pkg_name && *pkg_name == package_name)
      return window;
  }
  return nullptr;
}

// Gets expected window state type according to |event_type|.
chromeos::WindowStateType GetExpectedWindowState(
    api::autotest_private::WMEventType event_type) {
  switch (event_type) {
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTNORMAL:
      return chromeos::WindowStateType::kNormal;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTMAXIMIZE:
      return chromeos::WindowStateType::kMaximized;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTMINIMIZE:
      return chromeos::WindowStateType::kMinimized;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTFULLSCREEN:
      return chromeos::WindowStateType::kFullscreen;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTSNAPLEFT:
      return chromeos::WindowStateType::kLeftSnapped;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTSNAPRIGHT:
      return chromeos::WindowStateType::kRightSnapped;
    default:
      NOTREACHED();
      return chromeos::WindowStateType::kNormal;
  }
}

ash::WMEventType ToWMEventType(api::autotest_private::WMEventType event_type) {
  switch (event_type) {
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTNORMAL:
      return ash::WMEventType::WM_EVENT_NORMAL;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTMAXIMIZE:
      return ash::WMEventType::WM_EVENT_MAXIMIZE;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTMINIMIZE:
      return ash::WMEventType::WM_EVENT_MINIMIZE;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTFULLSCREEN:
      return ash::WMEventType::WM_EVENT_FULLSCREEN;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTSNAPLEFT:
      return ash::WMEventType::WM_EVENT_SNAP_LEFT;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTSNAPRIGHT:
      return ash::WMEventType::WM_EVENT_SNAP_RIGHT;
    default:
      NOTREACHED();
      return ash::WMEventType::WM_EVENT_NORMAL;
  }
}

api::autotest_private::WindowStateType ToWindowStateType(
    chromeos::WindowStateType state_type) {
  switch (state_type) {
    // Consider adding DEFAULT type to idl.
    case chromeos::WindowStateType::kDefault:
    case chromeos::WindowStateType::kNormal:
      return api::autotest_private::WindowStateType::WINDOW_STATE_TYPE_NORMAL;
    case chromeos::WindowStateType::kMinimized:
      return api::autotest_private::WindowStateType::
          WINDOW_STATE_TYPE_MINIMIZED;
    case chromeos::WindowStateType::kMaximized:
      return api::autotest_private::WindowStateType::
          WINDOW_STATE_TYPE_MAXIMIZED;
    case chromeos::WindowStateType::kFullscreen:
      return api::autotest_private::WindowStateType::
          WINDOW_STATE_TYPE_FULLSCREEN;
    case chromeos::WindowStateType::kLeftSnapped:
      return api::autotest_private::WindowStateType::
          WINDOW_STATE_TYPE_LEFTSNAPPED;
    case chromeos::WindowStateType::kRightSnapped:
      return api::autotest_private::WindowStateType::
          WINDOW_STATE_TYPE_RIGHTSNAPPED;
    case chromeos::WindowStateType::kPip:
      return api::autotest_private::WindowStateType::WINDOW_STATE_TYPE_PIP;
    default:
      NOTREACHED();
      return api::autotest_private::WindowStateType::WINDOW_STATE_TYPE_NONE;
  }
}

std::string GetPngDataAsString(scoped_refptr<base::RefCountedMemory> png_data) {
  // Base64 encode the result so we can return it as a string.
  std::string base64Png(png_data->front(),
                        png_data->front() + png_data->size());
  base::Base64Encode(base64Png, &base64Png);
  return base64Png;
}

display::Display::Rotation ToRotation(
    api::autotest_private::RotationType rotation) {
  switch (rotation) {
    case api::autotest_private::RotationType::ROTATION_TYPE_ROTATE0:
      return display::Display::ROTATE_0;
    case api::autotest_private::RotationType::ROTATION_TYPE_ROTATE90:
      return display::Display::ROTATE_90;
    case api::autotest_private::RotationType::ROTATION_TYPE_ROTATE180:
      return display::Display::ROTATE_180;
    case api::autotest_private::RotationType::ROTATION_TYPE_ROTATE270:
      return display::Display::ROTATE_270;
    case api::autotest_private::RotationType::ROTATION_TYPE_ROTATEANY:
    case api::autotest_private::RotationType::ROTATION_TYPE_NONE:
      break;
  }
  NOTREACHED();
  return display::Display::ROTATE_0;
}

api::autotest_private::Bounds ToBoundsDictionary(const gfx::Rect& bounds) {
  api::autotest_private::Bounds result;
  result.left = bounds.x();
  result.top = bounds.y();
  result.width = bounds.width();
  result.height = bounds.height();
  return result;
}

gfx::Rect ToRect(const api::autotest_private::Bounds& result) {
  return gfx::Rect(result.left, result.top, result.width, result.height);
}

std::vector<api::autotest_private::Bounds> ToBoundsDictionaryList(
    const std::vector<gfx::Rect>& items_bounds) {
  std::vector<api::autotest_private::Bounds> bounds_list;
  for (const gfx::Rect& bounds : items_bounds)
    bounds_list.push_back(ToBoundsDictionary(bounds));
  return bounds_list;
}

api::autotest_private::Location ToLocationDictionary(const gfx::Point& point) {
  api::autotest_private::Location result;
  result.x = point.x();
  result.y = point.y();
  return result;
}

aura::Window* FindAppWindowById(const int64_t id) {
  auto list = ash::GetAppWindowList();
  auto iter =
      std::find_if(list.begin(), list.end(),
                   [id](aura::Window* window) { return window->id() == id; });
  if (iter == list.end())
    return nullptr;
  return *iter;
}

// Returns the first available Browser that is not a web app.
Browser* GetFirstRegularBrowser() {
  const BrowserList* list = BrowserList::GetInstance();
  auto iter = std::find_if(list->begin(), list->end(), [](Browser* browser) {
    return browser->app_controller() == nullptr;
  });
  if (iter == list->end())
    return nullptr;
  return *iter;
}

ash::AppListViewState ToAppListViewState(
    api::autotest_private::LauncherStateType state) {
  switch (state) {
    case api::autotest_private::LauncherStateType::LAUNCHER_STATE_TYPE_CLOSED:
      return ash::AppListViewState::kClosed;
    case api::autotest_private::LauncherStateType::LAUNCHER_STATE_TYPE_PEEKING:
      return ash::AppListViewState::kPeeking;
    case api::autotest_private::LauncherStateType::LAUNCHER_STATE_TYPE_HALF:
      return ash::AppListViewState::kHalf;
    case api::autotest_private::LauncherStateType::
        LAUNCHER_STATE_TYPE_FULLSCREENALLAPPS:
      return ash::AppListViewState::kFullscreenAllApps;
    case api::autotest_private::LauncherStateType::
        LAUNCHER_STATE_TYPE_FULLSCREENSEARCH:
      return ash::AppListViewState::kFullscreenSearch;
    case api::autotest_private::LauncherStateType::LAUNCHER_STATE_TYPE_NONE:
      break;
  }
  return ash::AppListViewState::kClosed;
}

ash::OverviewAnimationState ToOverviewAnimationState(
    api::autotest_private::OverviewStateType state) {
  switch (state) {
    case api::autotest_private::OverviewStateType::OVERVIEW_STATE_TYPE_SHOWN:
      return ash::OverviewAnimationState::kEnterAnimationComplete;
    case api::autotest_private::OverviewStateType::OVERVIEW_STATE_TYPE_HIDDEN:
      return ash::OverviewAnimationState::kExitAnimationComplete;
    case api::autotest_private::OverviewStateType::OVERVIEW_STATE_TYPE_NONE:
      break;
  }
  NOTREACHED();
  return ash::OverviewAnimationState::kExitAnimationComplete;
}

ui::KeyboardCode StringToKeyCode(const std::string& str) {
  constexpr struct Map {
    const char* str;
    ui::KeyboardCode key_code;
  } map[] = {
      {"search", ui::VKEY_LWIN},
      {"assistant", ui::VKEY_ASSISTANT},
  };
  DCHECK(base::IsStringASCII(str));
  if (str.length() == 1) {
    char c = str[0];
    if (c >= 'a' && c <= 'z') {
      return static_cast<ui::KeyboardCode>(static_cast<int>(ui::VKEY_A) +
                                           (c - 'a'));
    }
    if (c >= '0' && c <= '9') {
      return static_cast<ui::KeyboardCode>(static_cast<int>(ui::VKEY_0) +
                                           (c - '0'));
    }
  } else {
    for (auto& entry : map) {
      if (str == entry.str)
        return entry.key_code;
    }
  }
  NOTREACHED();
  return ui::VKEY_A;
}

aura::Window* GetActiveWindow() {
  std::vector<aura::Window*> list = ash::GetAppWindowList();
  if (!list.size())
    return nullptr;
  return wm::GetActivationClient(list[0]->GetRootWindow())->GetActiveWindow();
}

bool IsFrameVisible(views::Widget* widget) {
  views::NonClientFrameView* frame_view =
      widget->non_client_view() ? widget->non_client_view()->frame_view()
                                : nullptr;
  return frame_view && frame_view->GetEnabled() && frame_view->GetVisible();
}

void ConvertPointToHost(aura::Window* root_window, gfx::PointF* location) {
  gfx::Point3F transformed_location_in_root(*location);
  root_window->GetHost()->GetRootTransform().TransformPoint(
      &transformed_location_in_root);
  *location = transformed_location_in_root.AsPointF();
}

int GetMouseEventFlags(api::autotest_private::MouseButton button) {
  switch (button) {
    case api::autotest_private::MOUSE_BUTTON_LEFT:
      return ui::EF_LEFT_MOUSE_BUTTON;
    case api::autotest_private::MOUSE_BUTTON_RIGHT:
      return ui::EF_RIGHT_MOUSE_BUTTON;
    case api::autotest_private::MOUSE_BUTTON_MIDDLE:
      return ui::EF_MIDDLE_MOUSE_BUTTON;
    default:
      NOTREACHED();
  }
  return ui::EF_NONE;
}

// Gets display id out of an optional DOMString display id argument. Returns
// false if optional display id is given but in bad format. Otherwise returns
// true and fills |display_id| with either the primary display id when the
// optional arg is not given or the parsed display id out of the arg
bool GetDisplayIdFromOptionalArg(const std::unique_ptr<std::string>& arg,
                                 int64_t* display_id) {
  if (arg.get() && !arg->empty()) {
    return base::StringToInt64(*arg, display_id);
  }

  *display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  return true;
}

struct SmoothnessTrackerInfo {
  base::Optional<ui::ThroughputTracker> tracker;
  ui::ThroughputTrackerHost::ReportCallback callback;
};
using DisplaySmoothnessTrackerInfos = std::map<int64_t, SmoothnessTrackerInfo>;
DisplaySmoothnessTrackerInfos* GetDisplaySmoothnessTrackerInfos() {
  static base::NoDestructor<DisplaySmoothnessTrackerInfos> trackers;
  return trackers.get();
}

// Forwards frame rate data to the callback for |display_id| and resets.
void ForwardFrameRateDataAndReset(
    int64_t display_id,
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  auto* infos = GetDisplaySmoothnessTrackerInfos();
  auto it = infos->find(display_id);
  DCHECK(it != infos->end());
  DCHECK(it->second.callback);

  // Moves the callback out and erases the mapping first to allow new tracking
  // for |display_id| to start before |callback| run returns.
  // See https://crbug.com/1098886.
  auto callback = std::move(it->second.callback);
  infos->erase(it);
  std::move(callback).Run(data);
}

std::string ResolutionToString(
    chromeos::assistant::AssistantInteractionResolution resolution) {
  using chromeos::assistant::AssistantInteractionResolution;
  switch (resolution) {
    case AssistantInteractionResolution::kNormal:
      return "kNormal";
    case AssistantInteractionResolution::kError:
      return "kError";
    case AssistantInteractionResolution::kInterruption:
      return "kInterruption";
    case AssistantInteractionResolution::kMicTimeout:
      return "kMicTimeout";
    case AssistantInteractionResolution::kMultiDeviceHotwordLoss:
      return "kMultiDeviceHotwordLoss";
  }

  // Not reachable here.
  DCHECK(false);
}

}  // namespace

class WindowStateChangeObserver : public aura::WindowObserver {
 public:
  WindowStateChangeObserver(aura::Window* window,
                            chromeos::WindowStateType expected_type,
                            base::OnceCallback<void(bool)> callback)
      : expected_type_(expected_type), callback_(std::move(callback)) {
    DCHECK_NE(window->GetProperty(chromeos::kWindowStateTypeKey),
              expected_type_);
    scoped_observer_.Add(window);
  }
  ~WindowStateChangeObserver() override {}

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK(scoped_observer_.IsObserving(window));
    if (key == chromeos::kWindowStateTypeKey &&
        window->GetProperty(chromeos::kWindowStateTypeKey) == expected_type_) {
      scoped_observer_.RemoveAll();
      std::move(callback_).Run(/*success=*/true);
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(scoped_observer_.IsObserving(window));
    scoped_observer_.RemoveAll();
    std::move(callback_).Run(/*success=*/false);
  }

 private:
  chromeos::WindowStateType expected_type_;
  ScopedObserver<aura::Window, aura::WindowObserver> scoped_observer_{this};
  base::OnceCallback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(WindowStateChangeObserver);
};

class WindowBoundsChangeObserver : public aura::WindowObserver {
 public:
  WindowBoundsChangeObserver(
      aura::Window* window,
      const gfx::Rect& to_bounds,
      int64_t display_id,
      base::OnceCallback<void(const gfx::Rect&, int64_t, bool)> callback)
      : callback_(std::move(callback)) {
    auto* state = ash::WindowState::Get(window);
    DCHECK(state);
    wait_for_bounds_change_ = window->GetBoundsInRootWindow() != to_bounds;
    wait_for_display_change_ = state->GetDisplay().id() != display_id;
    DCHECK(wait_for_bounds_change_ || wait_for_display_change_);
    scoped_observer_.Add(window);
  }
  ~WindowBoundsChangeObserver() override = default;

  WindowBoundsChangeObserver(const WindowBoundsChangeObserver&) = delete;
  WindowBoundsChangeObserver& operator=(const WindowBoundsChangeObserver&) =
      delete;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    wait_for_bounds_change_ = false;
    MaybeFinishObserving(window, /*success=*/true);
  }

  void OnWindowAddedToRootWindow(aura::Window* window) override {
    wait_for_display_change_ = false;
    MaybeFinishObserving(window, /*success=*/true);
  }

  void OnWindowDestroying(aura::Window* window) override {
    wait_for_display_change_ = false;
    wait_for_bounds_change_ = false;
    MaybeFinishObserving(window, /*success=*/false);
  }

 private:
  void MaybeFinishObserving(aura::Window* window, bool success) {
    DCHECK(scoped_observer_.IsObserving(window));
    if (!wait_for_bounds_change_ && !wait_for_display_change_) {
      scoped_observer_.RemoveAll();
      std::move(callback_).Run(window->GetBoundsInRootWindow(),
                               ash::WindowState::Get(window)->GetDisplay().id(),
                               success);
    }
  }

  ScopedObserver<aura::Window, aura::WindowObserver> scoped_observer_{this};
  bool wait_for_bounds_change_ = false;
  bool wait_for_display_change_ = false;
  base::OnceCallback<void(const gfx::Rect&, int64_t, bool)> callback_;
};

class EventGenerator {
 public:
  EventGenerator(aura::WindowTreeHost* host, base::OnceClosure closure)
      : input_injector_(
            ui::OzonePlatform::GetInstance()->CreateSystemInputInjector()),
        host_(host),
        interval_(base::TimeDelta::FromSeconds(1) /
                  std::max(host->compositor()->refresh_rate(), 60.0f)),
        closure_(std::move(closure)),
        weak_ptr_factory_(this) {
    LOG_IF(ERROR, host->compositor()->refresh_rate() < 60.0f)
        << "Refresh rate (" << host->compositor()->refresh_rate()
        << ") is too low.";
  }
  ~EventGenerator() = default;

  void ScheduleMouseEvent(ui::EventType type,
                          gfx::PointF location_in_host,
                          int flags) {
    if (flags == 0 &&
        (type == ui::ET_MOUSE_PRESSED || type == ui::ET_MOUSE_RELEASED)) {
      LOG(ERROR) << "No flags specified for mouse button changes";
    }
    tasks_.push_back(Task(type, location_in_host, flags));
  }

  void Run() {
    next_event_timestamp_ = base::TimeTicks::Now();
    SendEvent();
  }

  const base::TimeDelta& interval() const { return interval_; }

 private:
  struct Task {
    enum Status {
      kNotScheduled,
      kScheduled,
    };

    const ui::EventType type;
    const gfx::PointF location_in_host;
    const int flags;
    Status status = kNotScheduled;

    Task(ui::EventType type, gfx::PointF location_in_host, int flags)
        : type(type), location_in_host(location_in_host), flags(flags) {}
  };

  void SendEvent() {
    if (tasks_.empty()) {
      std::move(closure_).Run();
      return;
    }
    Task* task = &tasks_.front();
    DCHECK_EQ(task->status, Task::kNotScheduled);
    // A task can be processed asynchronously; the next task will be scheduled
    // after the control returns to the message pump, assuming that implies the
    // processing of the current task has finished.
    // WindowEventDispatcherObserver was used but the way it works does not
    // support nested loop in window move/resize or drag-n-drop. In such
    // cases, the mouse move event triggers the nested loop does not finish
    // until the nested loop quits. But this blocks future mouse events. Hence
    // the operation does not finish and the nested loop does not quit.
    task->status = Task::kScheduled;
    switch (task->type) {
      case ui::ET_MOUSE_PRESSED:
      case ui::ET_MOUSE_RELEASED: {
        bool pressed = (task->type == ui::ET_MOUSE_PRESSED);
        if (task->flags & ui::EF_LEFT_MOUSE_BUTTON)
          input_injector_->InjectMouseButton(ui::EF_LEFT_MOUSE_BUTTON, pressed);
        if (task->flags & ui::EF_MIDDLE_MOUSE_BUTTON) {
          input_injector_->InjectMouseButton(ui::EF_MIDDLE_MOUSE_BUTTON,
                                             pressed);
        }
        if (task->flags & ui::EF_RIGHT_MOUSE_BUTTON) {
          input_injector_->InjectMouseButton(ui::EF_RIGHT_MOUSE_BUTTON,
                                             pressed);
        }
        break;
      }
      case ui::ET_MOUSE_MOVED:
      case ui::ET_MOUSE_DRAGGED:
        // The location should be offset by the origin of the root-window since
        // ui::SystemInputInjector expects so.
        input_injector_->MoveCursorTo(
            task->location_in_host +
            host_->GetBoundsInPixels().OffsetFromOrigin());
        break;
      default:
        NOTREACHED();
    }

    // Post a task after scheduling the event and assumes that when the task
    // runs, it implies that the processing of the scheduled event is finished.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&EventGenerator::OnFinishedProcessingEvent,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void OnFinishedProcessingEvent() {
    if (tasks_.empty())
      return;

    DCHECK_EQ(tasks_.front().status, Task::kScheduled);
    tasks_.pop_front();
    auto runner = base::SequencedTaskRunnerHandle::Get();
    auto closure = base::BindOnce(&EventGenerator::SendEvent,
                                  weak_ptr_factory_.GetWeakPtr());
    // Non moving tasks can be done immediately.
    if (tasks_.empty() || tasks_.front().type == ui::ET_MOUSE_PRESSED ||
        tasks_.front().type == ui::ET_MOUSE_RELEASED) {
      runner->PostTask(FROM_HERE, std::move(closure));
      return;
    }
    next_event_timestamp_ += interval_;
    auto now = base::TimeTicks::Now();
    base::TimeDelta interval = next_event_timestamp_ - now;
    if (interval <= base::TimeDelta()) {
      // Looks like event handling could take too long time -- still generate
      // the next event with resetting the interval.
      LOG(ERROR) << "The handling of the event spent long time and there is "
                 << "no time to delay. The next event is supposed to happen at "
                 << next_event_timestamp_ << " but now at " << now << ". "
                 << "Posting the next event immediately.";
      next_event_timestamp_ = now;
      runner->PostTask(FROM_HERE, std::move(closure));
    } else {
      runner->PostDelayedTask(FROM_HERE, std::move(closure), interval);
    }
  }

  std::unique_ptr<ui::SystemInputInjector> input_injector_;
  aura::WindowTreeHost* host_;
  base::TimeTicks next_event_timestamp_;
  const base::TimeDelta interval_;
  base::OnceClosure closure_;
  std::deque<Task> tasks_;

  base::WeakPtrFactory<EventGenerator> weak_ptr_factory_;
};

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateInitializeEventsFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateInitializeEventsFunction::
    ~AutotestPrivateInitializeEventsFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateInitializeEventsFunction::Run() {
  // AutotestPrivateAPI is lazily initialized, but needs to be created before
  // any of its events can be fired, so we get the instance here and return.
  AutotestPrivateAPI::GetFactoryInstance()->Get(browser_context());
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateLogoutFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateLogoutFunction::~AutotestPrivateLogoutFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateLogoutFunction::Run() {
  DVLOG(1) << "AutotestPrivateLogoutFunction";
  if (!IsTestMode(browser_context()))
    chrome::AttemptUserExit();
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRestartFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRestartFunction::~AutotestPrivateRestartFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateRestartFunction::Run() {
  DVLOG(1) << "AutotestPrivateRestartFunction";
  if (!IsTestMode(browser_context()))
    chrome::AttemptRestart();
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateShutdownFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateShutdownFunction::~AutotestPrivateShutdownFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateShutdownFunction::Run() {
  std::unique_ptr<api::autotest_private::Shutdown::Params> params(
      api::autotest_private::Shutdown::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DVLOG(1) << "AutotestPrivateShutdownFunction " << params->force;

  if (!IsTestMode(browser_context()))
    chrome::AttemptExit();
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateLoginStatusFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateLoginStatusFunction::~AutotestPrivateLoginStatusFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateLoginStatusFunction::Run() {
  DVLOG(1) << "AutotestPrivateLoginStatusFunction";
  auto result = std::make_unique<base::DictionaryValue>();
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();

  // default_screen_locker()->locked() is set when the UI is ready, so this
  // tells us both views based lockscreen UI and screenlocker are ready.
  const bool is_screen_locked =
      !!chromeos::ScreenLocker::default_screen_locker() &&
      chromeos::ScreenLocker::default_screen_locker()->locked();

  if (user_manager) {
    result->SetBoolean("isLoggedIn", user_manager->IsUserLoggedIn());
    result->SetBoolean("isOwner", user_manager->IsCurrentUserOwner());
    result->SetBoolean("isScreenLocked", is_screen_locked);
    result->SetBoolean("isReadyForPassword",
                       ash::LoginScreen::Get()->IsReadyForPassword());
    if (user_manager->IsUserLoggedIn()) {
      result->SetBoolean("isRegularUser",
                         user_manager->IsLoggedInAsUserWithGaiaAccount());
      result->SetBoolean("isGuest", user_manager->IsLoggedInAsGuest());
      result->SetBoolean("isKiosk", user_manager->IsLoggedInAsKioskApp());

      const user_manager::User* user = user_manager->GetActiveUser();
      result->SetString("email", user->GetAccountId().GetUserEmail());
      result->SetString("displayEmail", user->display_email());

      std::string user_image;
      switch (user->image_index()) {
        case user_manager::User::USER_IMAGE_EXTERNAL:
          user_image = "file";
          break;

        case user_manager::User::USER_IMAGE_PROFILE:
          user_image = "profile";
          break;

        default:
          user_image = base::NumberToString(user->image_index());
          break;
      }
      result->SetString("userImage", user_image);
    }
  }
  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(std::move(result))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateLockScreenFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateLockScreenFunction::~AutotestPrivateLockScreenFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateLockScreenFunction::Run() {
  DVLOG(1) << "AutotestPrivateLockScreenFunction";

  chromeos::SessionManagerClient::Get()->RequestLockScreen();
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetAllEnterprisePoliciesFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetAllEnterprisePoliciesFunction::
    ~AutotestPrivateGetAllEnterprisePoliciesFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetAllEnterprisePoliciesFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetAllEnterprisePoliciesFunction";

  auto client = std::make_unique<policy::ChromePolicyConversionsClient>(
      browser_context());
  base::Value all_policies_array =
      policy::DictionaryPolicyConversions(std::move(client))
          .EnableDeviceLocalAccountPolicies(true)
          .EnableDeviceInfo(true)
          .ToValue();

  return RespondNow(OneArgument(std::move(all_policies_array)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRefreshEnterprisePoliciesFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRefreshEnterprisePoliciesFunction::
    ~AutotestPrivateRefreshEnterprisePoliciesFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRefreshEnterprisePoliciesFunction::Run() {
  DVLOG(1) << "AutotestPrivateRefreshEnterprisePoliciesFunction";

  g_browser_process->policy_service()->RefreshPolicies(base::BindOnce(
      &AutotestPrivateRefreshEnterprisePoliciesFunction::RefreshDone, this));
  return RespondLater();
}

void AutotestPrivateRefreshEnterprisePoliciesFunction::RefreshDone() {
  Respond(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetExtensionsInfoFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetExtensionsInfoFunction::
    ~AutotestPrivateGetExtensionsInfoFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetExtensionsInfoFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetExtensionsInfoFunction";

  ExtensionService* service =
      ExtensionSystem::Get(browser_context())->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  const ExtensionSet& extensions = registry->enabled_extensions();
  const ExtensionSet& disabled_extensions = registry->disabled_extensions();
  ExtensionActionManager* extension_action_manager =
      ExtensionActionManager::Get(browser_context());

  auto extensions_values = std::make_unique<base::ListValue>();
  ExtensionList all;
  all.insert(all.end(), extensions.begin(), extensions.end());
  all.insert(all.end(), disabled_extensions.begin(), disabled_extensions.end());
  for (ExtensionList::const_iterator it = all.begin(); it != all.end(); ++it) {
    const Extension* extension = it->get();
    std::string id = extension->id();
    std::unique_ptr<base::DictionaryValue> extension_value(
        new base::DictionaryValue);
    extension_value->SetString("id", id);
    extension_value->SetString("version", extension->VersionString());
    extension_value->SetString("name", extension->name());
    extension_value->SetString("publicKey", extension->public_key());
    extension_value->SetString("description", extension->description());
    extension_value->SetString(
        "backgroundUrl", BackgroundInfo::GetBackgroundURL(extension).spec());
    extension_value->SetString(
        "optionsUrl", OptionsPageInfo::GetOptionsPage(extension).spec());

    extension_value->Set("hostPermissions",
                         GetHostPermissions(extension, false));
    extension_value->Set("effectiveHostPermissions",
                         GetHostPermissions(extension, true));
    extension_value->Set("apiPermissions", GetAPIPermissions(extension));

    ManifestLocation location = extension->location();
    extension_value->SetBoolean("isComponent",
                                location == ManifestLocation::kComponent);
    extension_value->SetBoolean("isInternal",
                                location == ManifestLocation::kInternal);
    extension_value->SetBoolean("isUserInstalled",
                                location == ManifestLocation::kInternal ||
                                    Manifest::IsUnpackedLocation(location));
    extension_value->SetBoolean("isEnabled", service->IsExtensionEnabled(id));
    extension_value->SetBoolean(
        "allowedInIncognito", util::IsIncognitoEnabled(id, browser_context()));
    const ExtensionAction* action =
        extension_action_manager->GetExtensionAction(*extension);
    extension_value->SetBoolean(
        "hasPageAction",
        action && action->action_type() == ActionInfo::TYPE_PAGE);

    extensions_values->Append(std::move(extension_value));
  }

  std::unique_ptr<base::DictionaryValue> return_value(
      new base::DictionaryValue);
  return_value->Set("extensions", std::move(extensions_values));
  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(std::move(return_value))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSimulateAsanMemoryBugFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSimulateAsanMemoryBugFunction::
    ~AutotestPrivateSimulateAsanMemoryBugFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSimulateAsanMemoryBugFunction::Run() {
  DVLOG(1) << "AutotestPrivateSimulateAsanMemoryBugFunction";

  if (!IsTestMode(browser_context())) {
    // This array is volatile not to let compiler optimize us out.
    volatile int testarray[3] = {0, 0, 0};

    // Cause Address Sanitizer to abort this process.
    volatile int index = 5;
    AccessArray(testarray, &index);
  }
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetTouchpadSensitivityFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetTouchpadSensitivityFunction::
    ~AutotestPrivateSetTouchpadSensitivityFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetTouchpadSensitivityFunction::Run() {
  std::unique_ptr<api::autotest_private::SetTouchpadSensitivity::Params> params(
      api::autotest_private::SetTouchpadSensitivity::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DVLOG(1) << "AutotestPrivateSetTouchpadSensitivityFunction " << params->value;

  chromeos::system::InputDeviceSettings::Get()->SetTouchpadSensitivity(
      params->value);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetTapToClickFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetTapToClickFunction::~AutotestPrivateSetTapToClickFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateSetTapToClickFunction::Run() {
  std::unique_ptr<api::autotest_private::SetTapToClick::Params> params(
      api::autotest_private::SetTapToClick::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DVLOG(1) << "AutotestPrivateSetTapToClickFunction " << params->enabled;

  chromeos::system::InputDeviceSettings::Get()->SetTapToClick(params->enabled);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetThreeFingerClickFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetThreeFingerClickFunction::
    ~AutotestPrivateSetThreeFingerClickFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetThreeFingerClickFunction::Run() {
  std::unique_ptr<api::autotest_private::SetThreeFingerClick::Params> params(
      api::autotest_private::SetThreeFingerClick::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DVLOG(1) << "AutotestPrivateSetThreeFingerClickFunction " << params->enabled;

  chromeos::system::InputDeviceSettings::Get()->SetThreeFingerClick(
      params->enabled);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetTapDraggingFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetTapDraggingFunction::
    ~AutotestPrivateSetTapDraggingFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateSetTapDraggingFunction::Run() {
  std::unique_ptr<api::autotest_private::SetTapDragging::Params> params(
      api::autotest_private::SetTapDragging::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DVLOG(1) << "AutotestPrivateSetTapDraggingFunction " << params->enabled;

  chromeos::system::InputDeviceSettings::Get()->SetTapDragging(params->enabled);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetNaturalScrollFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetNaturalScrollFunction::
    ~AutotestPrivateSetNaturalScrollFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetNaturalScrollFunction::Run() {
  std::unique_ptr<api::autotest_private::SetNaturalScroll::Params> params(
      api::autotest_private::SetNaturalScroll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DVLOG(1) << "AutotestPrivateSetNaturalScrollFunction " << params->enabled;

  chromeos::system::InputDeviceSettings::Get()->SetNaturalScroll(
      params->enabled);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetMouseSensitivityFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetMouseSensitivityFunction::
    ~AutotestPrivateSetMouseSensitivityFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetMouseSensitivityFunction::Run() {
  std::unique_ptr<api::autotest_private::SetMouseSensitivity::Params> params(
      api::autotest_private::SetMouseSensitivity::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DVLOG(1) << "AutotestPrivateSetMouseSensitivityFunction " << params->value;

  chromeos::system::InputDeviceSettings::Get()->SetMouseSensitivity(
      params->value);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetPrimaryButtonRightFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetPrimaryButtonRightFunction::
    ~AutotestPrivateSetPrimaryButtonRightFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetPrimaryButtonRightFunction::Run() {
  std::unique_ptr<api::autotest_private::SetPrimaryButtonRight::Params> params(
      api::autotest_private::SetPrimaryButtonRight::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DVLOG(1) << "AutotestPrivateSetPrimaryButtonRightFunction " << params->right;

  chromeos::system::InputDeviceSettings::Get()->SetPrimaryButtonRight(
      params->right);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetMouseReverseScrollFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetMouseReverseScrollFunction::
    ~AutotestPrivateSetMouseReverseScrollFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetMouseReverseScrollFunction::Run() {
  std::unique_ptr<api::autotest_private::SetMouseReverseScroll::Params> params(
      api::autotest_private::SetMouseReverseScroll::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  DVLOG(1) << "AutotestPrivateSetMouseReverseScrollFunction "
           << params->enabled;

  chromeos::system::InputDeviceSettings::Get()->SetMouseReverseScroll(
      params->enabled);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetVisibleNotificationsFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetVisibleNotificationsFunction::
    AutotestPrivateGetVisibleNotificationsFunction() = default;
AutotestPrivateGetVisibleNotificationsFunction::
    ~AutotestPrivateGetVisibleNotificationsFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetVisibleNotificationsFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetVisibleNotificationsFunction";

  message_center::NotificationList::Notifications notification_set =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  auto values = std::make_unique<base::ListValue>();
  for (auto* notification : notification_set)
    values->Append(MakeDictionaryFromNotification(*notification));
  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(std::move(values))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRemoveAllNotificationsFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRemoveAllNotificationsFunction::
    AutotestPrivateRemoveAllNotificationsFunction() = default;
AutotestPrivateRemoveAllNotificationsFunction::
    ~AutotestPrivateRemoveAllNotificationsFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRemoveAllNotificationsFunction::Run() {
  DVLOG(1) << "AutotestPrivateRemoveAllNotificationsFunction";

  message_center::MessageCenter::Get()->RemoveAllNotifications(
      /*by_user=*/false, message_center::MessageCenter::RemoveType::ALL);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetArcStartTimeFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetArcStartTimeFunction::
    ~AutotestPrivateGetArcStartTimeFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetArcStartTimeFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetArcStartTimeFunction";

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (!arc_session_manager)
    return RespondNow(Error("Could not find ARC session manager"));

  const double start_ticks =
      (arc_session_manager->start_time() - base::TimeTicks()).InMillisecondsF();
  return RespondNow(OneArgument(base::Value(start_ticks)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetArcStateFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetArcStateFunction::~AutotestPrivateGetArcStateFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateGetArcStateFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetArcStateFunction";

  api::autotest_private::ArcState arc_state;
  Profile* const profile = Profile::FromBrowserContext(browser_context());

  if (!arc::IsArcAllowedForProfile(profile))
    return RespondNow(Error("ARC is not available for the current user"));

  arc::ArcSessionManager* const arc_session_manager =
      arc::ArcSessionManager::Get();
  if (!arc_session_manager)
    return RespondNow(Error("Could not find ARC session manager"));

  const base::Time now_time = base::Time::Now();
  const base::TimeTicks now_ticks = base::TimeTicks::Now();
  const base::TimeTicks pre_start_time = arc_session_manager->pre_start_time();
  const base::TimeTicks start_time = arc_session_manager->start_time();

  arc_state.provisioned = arc::IsArcProvisioned(profile);
  arc_state.tos_needed = arc::IsArcTermsOfServiceNegotiationNeeded(profile);
  arc_state.pre_start_time =
      pre_start_time.is_null()
          ? 0
          : (now_time - (now_ticks - pre_start_time)).ToJsTime();
  arc_state.start_time = start_time.is_null()
                             ? 0
                             : (now_time - (now_ticks - start_time)).ToJsTime();

  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(arc_state.ToValue())));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetPlayStoreStateFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetPlayStoreStateFunction::
    ~AutotestPrivateGetPlayStoreStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetPlayStoreStateFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetPlayStoreStateFunction";

  api::autotest_private::PlayStoreState play_store_state;
  play_store_state.allowed = false;
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (arc::IsArcAllowedForProfile(profile)) {
    play_store_state.allowed = true;
    play_store_state.enabled =
        std::make_unique<bool>(arc::IsArcPlayStoreEnabledForProfile(profile));
    play_store_state.managed = std::make_unique<bool>(
        arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile));
  }
  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(play_store_state.ToValue())));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetPlayStoreEnabledFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetPlayStoreEnabledFunction::
    ~AutotestPrivateSetPlayStoreEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetPlayStoreEnabledFunction::Run() {
  std::unique_ptr<api::autotest_private::SetPlayStoreEnabled::Params> params(
      api::autotest_private::SetPlayStoreEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetPlayStoreEnabledFunction " << params->enabled;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (arc::IsArcAllowedForProfile(profile)) {
    if (!arc::SetArcPlayStoreEnabledForProfile(profile, params->enabled)) {
      return RespondNow(
          Error("ARC enabled state cannot be changed for the current user"));
    }
    // kArcLocationServiceEnabled and kArcBackupRestoreEnabled are prefs that
    // set together with enabling ARC. That is why we set it here not using
    // SetWhitelistedPref. At this moment, we don't distinguish the actual
    // values and set kArcLocationServiceEnabled to true and leave
    // kArcBackupRestoreEnabled unmodified, which is acceptable for autotests
    // currently.
    profile->GetPrefs()->SetBoolean(arc::prefs::kArcLocationServiceEnabled,
                                    true);
    return RespondNow(NoArguments());
  } else {
    return RespondNow(Error("ARC is not available for the current user"));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateIsAppShownFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateIsAppShownFunction::~AutotestPrivateIsAppShownFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateIsAppShownFunction::Run() {
  std::unique_ptr<api::autotest_private::IsAppShown::Params> params(
      api::autotest_private::IsAppShown::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateIsAppShownFunction " << params->app_id;

  ChromeLauncherController* const controller =
      ChromeLauncherController::instance();
  if (!controller)
    return RespondNow(Error("Controller not available"));

  const ash::ShelfItem* item =
      controller->GetItem(ash::ShelfID(params->app_id));
  // App must be running and not pending in deferred launch.
  const bool window_attached =
      item && item->status == ash::ShelfItemStatus::STATUS_RUNNING &&
      !controller->GetShelfSpinnerController()->HasApp(params->app_id);
  return RespondNow(OneArgument(base::Value(window_attached)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateIsAppShownFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateIsArcProvisionedFunction::
    ~AutotestPrivateIsArcProvisionedFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateIsArcProvisionedFunction::Run() {
  DVLOG(1) << "AutotestPrivateIsArcProvisionedFunction";
  return RespondNow(OneArgument(base::Value(
      arc::IsArcProvisioned(Profile::FromBrowserContext(browser_context())))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetArcPackageFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetArcAppFunction::~AutotestPrivateGetArcAppFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateGetArcAppFunction::Run() {
  std::unique_ptr<api::autotest_private::GetArcApp::Params> params(
      api::autotest_private::GetArcApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateGetArcAppFunction " << params->app_id;

  ArcAppListPrefs* const prefs =
      ArcAppListPrefs::Get(Profile::FromBrowserContext(browser_context()));
  if (!prefs)
    return RespondNow(Error("ARC is not available"));

  std::unique_ptr<base::DictionaryValue> app_value;
  {
    const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        prefs->GetApp(params->app_id);
    if (!app_info)
      return RespondNow(Error("App is not available"));

    app_value = std::make_unique<base::DictionaryValue>();

    app_value->SetKey("name", base::Value(std::move(app_info->name)));
    app_value->SetKey("packageName",
                      base::Value(std::move(app_info->package_name)));
    app_value->SetKey("activity", base::Value(std::move(app_info->activity)));
    app_value->SetKey("intentUri",
                      base::Value(std::move(app_info->intent_uri)));
    app_value->SetKey("iconResourceId",
                      base::Value(std::move(app_info->icon_resource_id)));
    app_value->SetKey("lastLaunchTime",
                      base::Value(app_info->last_launch_time.ToJsTime()));
    app_value->SetKey("installTime",
                      base::Value(app_info->install_time.ToJsTime()));
    app_value->SetKey("sticky", base::Value(app_info->sticky));
    app_value->SetKey("notificationsEnabled",
                      base::Value(app_info->notifications_enabled));
    app_value->SetKey("ready", base::Value(app_info->ready));
    app_value->SetKey("suspended", base::Value(app_info->suspended));
    app_value->SetKey("showInLauncher",
                      base::Value(app_info->show_in_launcher));
    app_value->SetKey("shortcut", base::Value(app_info->shortcut));
    app_value->SetKey("launchable", base::Value(app_info->launchable));
  }

  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(std::move(app_value))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetArcPackageFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetArcPackageFunction::~AutotestPrivateGetArcPackageFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateGetArcPackageFunction::Run() {
  std::unique_ptr<api::autotest_private::GetArcPackage::Params> params(
      api::autotest_private::GetArcPackage::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateGetArcPackageFunction " << params->package_name;

  ArcAppListPrefs* const prefs =
      ArcAppListPrefs::Get(Profile::FromBrowserContext(browser_context()));
  if (!prefs)
    return RespondNow(Error("ARC is not available"));

  std::unique_ptr<base::DictionaryValue> package_value;
  {
    const std::unique_ptr<ArcAppListPrefs::PackageInfo> package_info =
        prefs->GetPackage(params->package_name);
    if (!package_info)
      return RespondNow(Error("Package is not available"));

    package_value = std::make_unique<base::DictionaryValue>();
    package_value->SetKey("packageName",
                          base::Value(std::move(package_info->package_name)));
    package_value->SetKey("packageVersion",
                          base::Value(package_info->package_version));
    package_value->SetKey("lastBackupAndroidId",
                          base::Value(base::NumberToString(
                              package_info->last_backup_android_id)));
    package_value->SetKey("lastBackupTime",
                          base::Value(base::Time::FromDeltaSinceWindowsEpoch(
                                          base::TimeDelta::FromMicroseconds(
                                              package_info->last_backup_time))
                                          .ToJsTime()));
    package_value->SetKey("shouldSync", base::Value(package_info->should_sync));
    package_value->SetKey("system", base::Value(package_info->system));
    package_value->SetKey("vpnProvider",
                          base::Value(package_info->vpn_provider));
  }
  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(std::move(package_value))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateWaitForSystemWebAppsInstallFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateWaitForSystemWebAppsInstallFunction::
    AutotestPrivateWaitForSystemWebAppsInstallFunction() = default;

AutotestPrivateWaitForSystemWebAppsInstallFunction::
    ~AutotestPrivateWaitForSystemWebAppsInstallFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateWaitForSystemWebAppsInstallFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  web_app::WebAppProviderBase* provider =
      web_app::WebAppProviderBase::GetProviderBase(profile);

  if (!provider)
    return RespondNow(Error("Web Apps are not available for profile."));

  provider->system_web_app_manager().on_apps_synchronized().Post(
      FROM_HERE,
      base::BindOnce(
          &AutotestPrivateWaitForSystemWebAppsInstallFunction::Respond, this,
          NoArguments()));
  return RespondLater();
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetRegisteredSystemWebAppsFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetRegisteredSystemWebAppsFunction::
    AutotestPrivateGetRegisteredSystemWebAppsFunction() = default;

AutotestPrivateGetRegisteredSystemWebAppsFunction::
    ~AutotestPrivateGetRegisteredSystemWebAppsFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetRegisteredSystemWebAppsFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  web_app::WebAppProviderBase* provider =
      web_app::WebAppProviderBase::GetProviderBase(profile);

  if (!provider)
    return RespondNow(Error("Web Apps are not available for profile."));

  std::vector<api::autotest_private::SystemApp> result;

  for (const auto& type_and_info :
       provider->system_web_app_manager().GetRegisteredSystemAppsForTesting()) {
    api::autotest_private::SystemApp system_app;
    web_app::SystemAppInfo info = type_and_info.second;
    system_app.internal_name = info.internal_name;
    system_app.url = info.install_url.GetOrigin().spec();
    result.push_back(std::move(system_app));
  }

  return RespondNow(ArgumentList(
      api::autotest_private::GetRegisteredSystemWebApps::Results::Create(
          result)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateLaunchArcIntentFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateLaunchArcAppFunction::~AutotestPrivateLaunchArcAppFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateLaunchArcAppFunction::Run() {
  std::unique_ptr<api::autotest_private::LaunchArcApp::Params> params(
      api::autotest_private::LaunchArcApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateLaunchArcIntentFunction " << params->app_id << "/"
           << params->intent;

  base::Optional<std::string> launch_intent;
  if (!params->intent.empty())
    launch_intent = params->intent;
  const bool result = arc::LaunchAppWithIntent(
      Profile::FromBrowserContext(browser_context()), params->app_id,
      launch_intent, 0 /* event_flags */,
      arc::UserInteractionType::APP_STARTED_FROM_EXTENSION_API,
      0 /* display_id */);
  return RespondNow(OneArgument(base::Value(result)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateLaunchAppFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateLaunchAppFunction::~AutotestPrivateLaunchAppFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateLaunchAppFunction::Run() {
  std::unique_ptr<api::autotest_private::LaunchApp::Params> params(
      api::autotest_private::LaunchApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateLaunchAppFunction " << params->app_id;

  ChromeLauncherController* const controller =
      ChromeLauncherController::instance();
  if (!controller)
    return RespondNow(Error("Controller not available"));
  controller->LaunchApp(ash::ShelfID(params->app_id),
                        ash::ShelfLaunchSource::LAUNCH_FROM_UNKNOWN,
                        0, /* event_flags */
                        display::Screen::GetScreen()->GetPrimaryDisplay().id());
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateLaunchSystemWebAppFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateLaunchSystemWebAppFunction::
    ~AutotestPrivateLaunchSystemWebAppFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateLaunchSystemWebAppFunction::Run() {
  std::unique_ptr<api::autotest_private::LaunchSystemWebApp::Params> params(
      api::autotest_private::LaunchSystemWebApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateLaunchSystemWebAppFunction name: "
           << params->app_name << " url: " << params->url;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  auto* provider = web_app::WebAppProvider::Get(profile);
  if (!provider)
    return RespondNow(Error("Web Apps not enabled for profile."));

  base::Optional<web_app::SystemAppType> app_type;
  for (const auto& type_and_info :
       provider->system_web_app_manager().GetRegisteredSystemAppsForTesting()) {
    if (type_and_info.second.internal_name == params->app_name) {
      app_type = type_and_info.first;
      break;
    }
  }
  if (!app_type.has_value())
    return RespondNow(Error("No mapped system web app found"));

  web_app::LaunchSystemWebAppAsync(profile, *app_type,
                                   {.url = GURL(params->url)});

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateCloseAppFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateCloseAppFunction::~AutotestPrivateCloseAppFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateCloseAppFunction::Run() {
  std::unique_ptr<api::autotest_private::CloseApp::Params> params(
      api::autotest_private::CloseApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateCloseAppFunction " << params->app_id;

  ChromeLauncherController* const controller =
      ChromeLauncherController::instance();
  if (!controller)
    return RespondNow(Error("Controller not available"));
  controller->Close(ash::ShelfID(params->app_id));
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetClipboardTextDataFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetClipboardTextDataFunction::
    ~AutotestPrivateGetClipboardTextDataFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetClipboardTextDataFunction::Run() {
  std::u16string data;
  // This clipboard data read is initiated an extension API, then the user
  // shouldn't see a notification if the clipboard is restricted by the rules of
  // data leak prevention policy.
  ui::DataTransferEndpoint data_dst = ui::DataTransferEndpoint(
      ui::EndpointType::kDefault, /*notify_if_restricted=*/false);
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst, &data);
  return RespondNow(OneArgument(base::Value(data)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetClipboardTextDataFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetClipboardTextDataFunction::
    AutotestPrivateSetClipboardTextDataFunction() = default;

AutotestPrivateSetClipboardTextDataFunction::
    ~AutotestPrivateSetClipboardTextDataFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetClipboardTextDataFunction::Run() {
  observation_.Observe(ui::ClipboardMonitor::GetInstance());
  std::unique_ptr<api::autotest_private::SetClipboardTextData::Params> params(
      api::autotest_private::SetClipboardTextData::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::u16string data = base::UTF8ToUTF16(params->data);
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteText(data);

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutotestPrivateSetClipboardTextDataFunction::OnClipboardDataChanged() {
  observation_.Reset();
  Respond(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetCrostiniEnabledFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetCrostiniEnabledFunction::
    ~AutotestPrivateSetCrostiniEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetCrostiniEnabledFunction::Run() {
  std::unique_ptr<api::autotest_private::SetCrostiniEnabled::Params> params(
      api::autotest_private::SetCrostiniEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetCrostiniEnabledFunction " << params->enabled;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile))
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));

  // Set the preference to indicate Crostini is enabled/disabled.
  profile->GetPrefs()->SetBoolean(crostini::prefs::kCrostiniEnabled,
                                  params->enabled);
  // Set the flag to indicate we are in testing mode so that Chrome doesn't
  // try to start the VM/container itself.
  crostini::CrostiniManager* crostini_manager =
      crostini::CrostiniManager::GetForProfile(profile);
  crostini_manager->set_skip_restart_for_testing();
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRunCrostiniInstallerFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRunCrostiniInstallerFunction::
    ~AutotestPrivateRunCrostiniInstallerFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRunCrostiniInstallerFunction::Run() {
  DVLOG(1) << "AutotestPrivateInstallCrostiniFunction";

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile))
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));

  // Run GUI installer which will install crostini vm / container and
  // start terminal app on completion.  After starting the installer,
  // we call RestartCrostini and we will be put in the pending restarters
  // queue and be notified on success/otherwise of installation.
  chromeos::CrostiniInstallerDialog::Show(
      profile, base::BindOnce([](chromeos::CrostiniInstallerUI* installer_ui) {
        installer_ui->ClickInstallForTesting();
      }));
  crostini::CrostiniManager::GetForProfile(profile)->RestartCrostini(
      crostini::ContainerId::GetDefault(),
      base::BindOnce(
          &AutotestPrivateRunCrostiniInstallerFunction::CrostiniRestarted,
          this));

  return RespondLater();
}

void AutotestPrivateRunCrostiniInstallerFunction::CrostiniRestarted(
    crostini::CrostiniResult result) {
  if (result == crostini::CrostiniResult::SUCCESS) {
    Respond(NoArguments());
  } else {
    Respond(Error("Error installing crostini"));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRunCrostiniUninstallerFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRunCrostiniUninstallerFunction::
    ~AutotestPrivateRunCrostiniUninstallerFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRunCrostiniUninstallerFunction::Run() {
  DVLOG(1) << "AutotestPrivateRunCrostiniUninstallerFunction";

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile))
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));

  // Run GUI uninstaller which will remove crostini vm / container. We then
  // receive the callback with the result when that is complete.
  crostini::CrostiniManager::GetForProfile(profile)->AddRemoveCrostiniCallback(
      base::BindOnce(
          &AutotestPrivateRunCrostiniUninstallerFunction::CrostiniRemoved,
          this));
  CrostiniUninstallerView::Show(profile);
  CrostiniUninstallerView::GetActiveViewForTesting()->Accept();
  return RespondLater();
}

void AutotestPrivateRunCrostiniUninstallerFunction::CrostiniRemoved(
    crostini::CrostiniResult result) {
  if (result == crostini::CrostiniResult::SUCCESS)
    Respond(NoArguments());
  else
    Respond(Error("Error uninstalling crostini"));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateExportCrostiniFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateExportCrostiniFunction::
    ~AutotestPrivateExportCrostiniFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateExportCrostiniFunction::Run() {
  std::unique_ptr<api::autotest_private::ExportCrostini::Params> params(
      api::autotest_private::ExportCrostini::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateExportCrostiniFunction " << params->path;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile) ||
      !crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile)) {
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));
  }

  base::FilePath path(params->path);
  if (path.ReferencesParent()) {
    return RespondNow(Error("Invalid export path must not reference parent"));
  }

  crostini::CrostiniExportImport::GetForProfile(profile)->ExportContainer(
      crostini::ContainerId::GetDefault(),
      file_manager::util::GetDownloadsFolderForProfile(profile).Append(path),
      base::BindOnce(&AutotestPrivateExportCrostiniFunction::CrostiniExported,
                     this));

  return RespondLater();
}

void AutotestPrivateExportCrostiniFunction::CrostiniExported(
    crostini::CrostiniResult result) {
  if (result == crostini::CrostiniResult::SUCCESS) {
    Respond(NoArguments());
  } else {
    Respond(Error("Error exporting crostini"));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateImportCrostiniFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateImportCrostiniFunction::
    ~AutotestPrivateImportCrostiniFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateImportCrostiniFunction::Run() {
  std::unique_ptr<api::autotest_private::ImportCrostini::Params> params(
      api::autotest_private::ImportCrostini::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateImportCrostiniFunction " << params->path;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile) ||
      !crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile))
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));

  base::FilePath path(params->path);
  if (path.ReferencesParent()) {
    return RespondNow(Error("Invalid import path must not reference parent"));
  }
  crostini::CrostiniExportImport::GetForProfile(profile)->ImportContainer(
      crostini::ContainerId::GetDefault(),
      file_manager::util::GetDownloadsFolderForProfile(profile).Append(path),
      base::BindOnce(&AutotestPrivateImportCrostiniFunction::CrostiniImported,
                     this));

  return RespondLater();
}

void AutotestPrivateImportCrostiniFunction::CrostiniImported(
    crostini::CrostiniResult result) {
  if (result == crostini::CrostiniResult::SUCCESS) {
    Respond(NoArguments());
  } else {
    Respond(Error("Error importing crostini"));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetPluginVMPolicyFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetPluginVMPolicyFunction::
    ~AutotestPrivateSetPluginVMPolicyFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetPluginVMPolicyFunction::Run() {
  std::unique_ptr<api::autotest_private::SetPluginVMPolicy::Params> params(
      api::autotest_private::SetPluginVMPolicy::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetPluginVMPolicyFunction " << params->image_url
           << ", " << params->image_hash << ", " << params->license_key;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  plugin_vm::SetFakePluginVmPolicy(profile, params->image_url,
                                   params->image_hash, params->license_key);

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateShowPluginVMInstallerFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateShowPluginVMInstallerFunction::
    ~AutotestPrivateShowPluginVMInstallerFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateShowPluginVMInstallerFunction::Run() {
  DVLOG(1) << "AutotestPrivateShowPluginVMInstallerFunction";

  Profile* profile = Profile::FromBrowserContext(browser_context());
  plugin_vm::PluginVmInstallerFactory::GetForProfile(profile)
      ->SetFreeDiskSpaceForTesting(
          plugin_vm::PluginVmInstallerFactory::GetForProfile(profile)
              ->RequiredFreeDiskSpace());
  plugin_vm::ShowPluginVmInstallerView(profile);

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateInstallBorealisFunction
///////////////////////////////////////////////////////////////////////////////

class AutotestPrivateInstallBorealisFunction::InstallationObserver
    : public borealis::BorealisInstaller::Observer {
 public:
  InstallationObserver(Profile* profile,
                       base::OnceCallback<void(bool)> completion_callback)
      : observation_(this),
        completion_callback_(std::move(completion_callback)) {
    observation_.Observe(
        &borealis::BorealisService::GetForProfile(profile)->Installer());
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](Profile* profile) {
                         borealis::BorealisService::GetForProfile(profile)
                             ->Installer()
                             .Start();
                       },
                       profile));
  }

  void OnProgressUpdated(double fraction_complete) override {}

  void OnStateUpdated(
      borealis::BorealisInstaller::InstallingState new_state) override {}

  void OnInstallationEnded(borealis::BorealisInstallResult result) override {
    std::move(completion_callback_)
        .Run(result == borealis::BorealisInstallResult::kSuccess);
  }

  void OnCancelInitiated() override {}

 private:
  base::ScopedObservation<borealis::BorealisInstaller,
                          borealis::BorealisInstaller::Observer>
      observation_;
  base::OnceCallback<void(bool)> completion_callback_;
};

AutotestPrivateInstallBorealisFunction::
    AutotestPrivateInstallBorealisFunction() = default;

AutotestPrivateInstallBorealisFunction::
    ~AutotestPrivateInstallBorealisFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateInstallBorealisFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  installation_observer_ = std::make_unique<InstallationObserver>(
      profile,
      base::BindOnce(&AutotestPrivateInstallBorealisFunction::Complete, this));
  return RespondLater();
}

void AutotestPrivateInstallBorealisFunction::Complete(bool was_successful) {
  if (was_successful) {
    Respond(NoArguments());
  } else {
    Respond(Error("Failed to install borealis"));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRegisterComponentFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRegisterComponentFunction::
    ~AutotestPrivateRegisterComponentFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRegisterComponentFunction::Run() {
  std::unique_ptr<api::autotest_private::RegisterComponent::Params> params(
      api::autotest_private::RegisterComponent::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateRegisterComponentFunction " << params->name
           << ", " << params->path;

  g_browser_process->platform_part()
      ->cros_component_manager()
      ->RegisterCompatiblePath(params->name, base::FilePath(params->path));

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateTakeScreenshotFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateTakeScreenshotFunction::
    ~AutotestPrivateTakeScreenshotFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateTakeScreenshotFunction::Run() {
  DVLOG(1) << "AutotestPrivateTakeScreenshotFunction";
  auto grabber = std::make_unique<ui::ScreenshotGrabber>();
  auto* const grabber_ptr = grabber.get();
  // TODO(mash): Fix for mash, http://crbug.com/557397
  aura::Window* primary_root = ash::Shell::GetPrimaryRootWindow();
  // Pass the ScreenshotGrabber to the callback so that it stays alive for the
  // duration of the operation, it'll then get deallocated when the callback
  // completes.
  grabber_ptr->TakeScreenshot(
      primary_root, primary_root->bounds(),
      base::BindOnce(&AutotestPrivateTakeScreenshotFunction::ScreenshotTaken,
                     this, std::move(grabber)));
  return RespondLater();
}

void AutotestPrivateTakeScreenshotFunction::ScreenshotTaken(
    std::unique_ptr<ui::ScreenshotGrabber> grabber,
    ui::ScreenshotResult screenshot_result,
    scoped_refptr<base::RefCountedMemory> png_data) {
  if (screenshot_result != ui::ScreenshotResult::SUCCESS) {
    return Respond(Error(base::StrCat(
        {"Error taking screenshot ",
         base::NumberToString(static_cast<int>(screenshot_result))})));
  }
  Respond(OneArgument(base::Value(GetPngDataAsString(png_data))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateTakeScreenshotForDisplayFunction
///////////////////////////////////////////////////////////////////////////////
AutotestPrivateTakeScreenshotForDisplayFunction::
    ~AutotestPrivateTakeScreenshotForDisplayFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateTakeScreenshotForDisplayFunction::Run() {
  std::unique_ptr<api::autotest_private::TakeScreenshotForDisplay::Params>
      params(api::autotest_private::TakeScreenshotForDisplay::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateTakeScreenshotForDisplayFunction "
           << params->display_id;
  int64_t target_display_id;
  base::StringToInt64(params->display_id, &target_display_id);
  auto grabber = std::make_unique<ui::ScreenshotGrabber>();

  for (auto* const window : ash::Shell::GetAllRootWindows()) {
    const int64_t display_id =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
    if (display_id == target_display_id) {
      auto* const grabber_ptr = grabber.get();
      grabber_ptr->TakeScreenshot(
          window, window->bounds(),
          base::BindOnce(
              &AutotestPrivateTakeScreenshotForDisplayFunction::ScreenshotTaken,
              this, std::move(grabber)));
      return RespondLater();
    }
  }
  return RespondNow(Error(base::StrCat(
      {"Error taking screenshot for display ", params->display_id})));
}

void AutotestPrivateTakeScreenshotForDisplayFunction::ScreenshotTaken(
    std::unique_ptr<ui::ScreenshotGrabber> grabber,
    ui::ScreenshotResult screenshot_result,
    scoped_refptr<base::RefCountedMemory> png_data) {
  if (screenshot_result != ui::ScreenshotResult::SUCCESS) {
    return Respond(Error(base::StrCat(
        {"Error taking screenshot ",
         base::NumberToString(static_cast<int>(screenshot_result))})));
  }
  Respond(OneArgument(base::Value(GetPngDataAsString(png_data))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetPrinterListFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetPrinterListFunction::AutotestPrivateGetPrinterListFunction()
    : results_(std::make_unique<base::Value>(base::Value::Type::LIST)) {}

AutotestPrivateGetPrinterListFunction::
    ~AutotestPrivateGetPrinterListFunction() {
  DCHECK(!printers_manager_);
}

ExtensionFunction::ResponseAction AutotestPrivateGetPrinterListFunction::Run() {
  // |printers_manager_| should be created on UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(1) << "AutotestPrivateGetPrinterListFunction";

  Profile* profile = Profile::FromBrowserContext(browser_context());
  printers_manager_ = chromeos::CupsPrintersManager::Create(profile);
  printers_manager_->AddObserver(this);

  // Set up a timer to finish waiting after 10 seconds
  timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(10),
      base::BindOnce(
          &AutotestPrivateGetPrinterListFunction::RespondWithTimeoutError,
          this));

  return RespondLater();
}

void AutotestPrivateGetPrinterListFunction::DestroyPrintersManager() {
  // |printers_manager_| should be destroyed on UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!printers_manager_)
    return;

  printers_manager_->RemoveObserver(this);
  printers_manager_.reset();
}

void AutotestPrivateGetPrinterListFunction::RespondWithTimeoutError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (did_respond())
    return;

  DestroyPrintersManager();
  Respond(
      Error("Timeout occurred before Enterprise printers were initialized"));
}

void AutotestPrivateGetPrinterListFunction::RespondWithSuccess() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (did_respond())
    return;

  timeout_timer_.AbandonAndStop();
  DestroyPrintersManager();
  Respond(OneArgument(base::Value::FromUniquePtrValue(std::move(results_))));
}

void AutotestPrivateGetPrinterListFunction::OnEnterprisePrintersInitialized() {
  // |printers_manager_| should call this on UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  constexpr PrinterClass kClassesToFetch[] = {
      PrinterClass::kEnterprise,
      PrinterClass::kSaved,
      PrinterClass::kAutomatic,
  };

  // We are ready to get the list of printers and finish.
  for (const auto& type : kClassesToFetch) {
    std::vector<chromeos::Printer> printer_list =
        printers_manager_->GetPrinters(type);
    for (const auto& printer : printer_list) {
      base::Value result(base::Value::Type::DICTIONARY);
      result.SetKey("printerName", base::Value(printer.display_name()));
      result.SetKey("printerId", base::Value(printer.id()));
      result.SetKey("printerType", base::Value(GetPrinterType(type)));
      results_->Append(std::move(result));
    }
  }
  // We have to respond in separate task on the same thread, because it will
  // cause a destruction of CupsPrintersManager which needs to happen after
  // we return and on the same thread.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&AutotestPrivateGetPrinterListFunction::RespondWithSuccess,
                     this));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateUpdatePrinterFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateUpdatePrinterFunction::~AutotestPrivateUpdatePrinterFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateUpdatePrinterFunction::Run() {
  std::unique_ptr<api::autotest_private::UpdatePrinter::Params> params(
      api::autotest_private::UpdatePrinter::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateUpdatePrinterFunction";

  const api::autotest_private::Printer& js_printer = params->printer;
  chromeos::Printer printer(js_printer.printer_id ? *js_printer.printer_id
                                                  : "");
  printer.set_display_name(js_printer.printer_name);
  if (js_printer.printer_desc)
    printer.set_description(*js_printer.printer_desc);

  if (js_printer.printer_make_and_model)
    printer.set_make_and_model(*js_printer.printer_make_and_model);

  if (js_printer.printer_uri) {
    std::string message;
    if (!printer.SetUri(*js_printer.printer_uri, &message)) {
      LOG(ERROR) << message;
      return RespondNow(Error("Incorrect URI: " + message));
    }
  }

  if (js_printer.printer_ppd) {
    const GURL ppd =
        net::FilePathToFileURL(base::FilePath(*js_printer.printer_ppd));
    if (ppd.is_valid())
      printer.mutable_ppd_reference()->user_supplied_ppd_url = ppd.spec();
    else
      LOG(ERROR) << "Invalid ppd path: " << *js_printer.printer_ppd;
  }
  auto* printers_manager =
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(
          browser_context());
  printers_manager->SavePrinter(printer);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRemovePrinterFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRemovePrinterFunction::~AutotestPrivateRemovePrinterFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateRemovePrinterFunction::Run() {
  std::unique_ptr<api::autotest_private::RemovePrinter::Params> params(
      api::autotest_private::RemovePrinter::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateRemovePrinterFunction " << params->printer_id;

  auto* printers_manager =
      chromeos::CupsPrintersManagerFactory::GetForBrowserContext(
          browser_context());
  printers_manager->RemoveSavedPrinter(params->printer_id);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateBootstrapMachineLearningServiceFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateBootstrapMachineLearningServiceFunction::
    AutotestPrivateBootstrapMachineLearningServiceFunction() = default;
AutotestPrivateBootstrapMachineLearningServiceFunction::
    ~AutotestPrivateBootstrapMachineLearningServiceFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateBootstrapMachineLearningServiceFunction::Run() {
  DVLOG(1) << "AutotestPrivateBootstrapMachineLearningServiceFunction";

  // Load a model. This will first bootstrap the Mojo connection to ML Service.
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadBuiltinModel(
          chromeos::machine_learning::mojom::BuiltinModelSpec::New(
              chromeos::machine_learning::mojom::BuiltinModelId::TEST_MODEL),
          model_.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              &AutotestPrivateBootstrapMachineLearningServiceFunction::
                  ModelLoaded,
              this));
  model_.set_disconnect_handler(base::BindOnce(
      &AutotestPrivateBootstrapMachineLearningServiceFunction::OnMojoDisconnect,
      this));
  return RespondLater();
}

void AutotestPrivateBootstrapMachineLearningServiceFunction::ModelLoaded(
    chromeos::machine_learning::mojom::LoadModelResult result) {
  if (result == chromeos::machine_learning::mojom::LoadModelResult::OK) {
    Respond(NoArguments());
  } else {
    Respond(Error(base::StrCat(
        {"Model load error ", (std::ostringstream() << result).str()})));
  }
}

void AutotestPrivateBootstrapMachineLearningServiceFunction::
    OnMojoDisconnect() {
  Respond(Error("ML Service connection error"));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetAssistantEnabled
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetAssistantEnabledFunction::
    AutotestPrivateSetAssistantEnabledFunction() {
  // |AddObserver| will immediately trigger |OnAssistantStatusChanged|.
  ash::AssistantState::Get()->AddObserver(this);
}

AutotestPrivateSetAssistantEnabledFunction::
    ~AutotestPrivateSetAssistantEnabledFunction() {
  ash::AssistantState::Get()->RemoveObserver(this);
}

ExtensionFunction::ResponseAction
AutotestPrivateSetAssistantEnabledFunction::Run() {
  DVLOG(1) << "AutotestPrivateSetAssistantEnabledFunction";

  std::unique_ptr<api::autotest_private::SetAssistantEnabled::Params> params(
      api::autotest_private::SetAssistantEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const std::string& err_msg =
      SetWhitelistedPref(profile, chromeos::assistant::prefs::kAssistantEnabled,
                         base::Value(params->enabled));
  if (!err_msg.empty())
    return RespondNow(Error(err_msg));

  // Any state that's not |NOT_READY| would be considered a ready state.
  const bool not_ready = (ash::AssistantState::Get()->assistant_status() ==
                          chromeos::assistant::AssistantStatus::NOT_READY);
  const bool success = (params->enabled != not_ready);
  if (success)
    return RespondNow(NoArguments());

  // Assistant service has not responded yet, set up a delayed timer to wait for
  // it and holder a reference to |this|. Also make sure we stop and respond
  // when timeout.
  enabled_ = params->enabled;
  timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(params->timeout_ms),
      base::BindOnce(&AutotestPrivateSetAssistantEnabledFunction::Timeout,
                     this));
  return RespondLater();
}

void AutotestPrivateSetAssistantEnabledFunction::OnAssistantStatusChanged(
    chromeos::assistant::AssistantStatus status) {
  // Must check if the Optional contains value first to avoid possible
  // segmentation fault caused by Respond() below being called before
  // RespondLater() in Run(). This will happen due to AddObserver() call
  // in the constructor will trigger this function immediately.
  if (!enabled_.has_value())
    return;

  const bool not_ready =
      (status == chromeos::assistant::AssistantStatus::NOT_READY);
  const bool success = (enabled_.value() != not_ready);
  if (!success)
    return;

  Respond(NoArguments());
  enabled_.reset();
  timeout_timer_.AbandonAndStop();
}

void AutotestPrivateSetAssistantEnabledFunction::Timeout() {
  DCHECK(!did_respond());
  Respond(Error("Assistant service timed out"));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateEnableAssistantAndWaitForReadyFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateEnableAssistantAndWaitForReadyFunction::
    AutotestPrivateEnableAssistantAndWaitForReadyFunction() = default;

AutotestPrivateEnableAssistantAndWaitForReadyFunction::
    ~AutotestPrivateEnableAssistantAndWaitForReadyFunction() {
  ash::AssistantState::Get()->RemoveObserver(this);
}

ExtensionFunction::ResponseAction
AutotestPrivateEnableAssistantAndWaitForReadyFunction::Run() {
  DVLOG(1) << "AutotestPrivateEnableAssistantAndWaitForReadyFunction";

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const std::string& err_msg =
      SetWhitelistedPref(profile, chromeos::assistant::prefs::kAssistantEnabled,
                         base::Value(true));
  if (!err_msg.empty())
    return RespondNow(Error(err_msg));

  // Asynchronously subscribe to status changes to avoid a possible segmentation
  // fault caused by Respond() in the subscriber callback being called before
  // RespondLater() below.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&AutotestPrivateEnableAssistantAndWaitForReadyFunction::
                         SubscribeToStatusChanges,
                     this));

  // Prevent |this| from being freed before we get a response from the
  // Assistant.
  self_ = this;

  return RespondLater();
}

void AutotestPrivateEnableAssistantAndWaitForReadyFunction::
    SubscribeToStatusChanges() {
  // |AddObserver| will immediately trigger |OnAssistantStatusChanged|.
  ash::AssistantState::Get()->AddObserver(this);
}

void AutotestPrivateEnableAssistantAndWaitForReadyFunction::
    OnAssistantStatusChanged(chromeos::assistant::AssistantStatus status) {
  if (status == chromeos::assistant::AssistantStatus::READY) {
    Respond(NoArguments());
    self_.reset();
  }
}

// AssistantInteractionHelper is a helper class used to interact with Assistant
// server and store interaction states for tests. It is shared by
// |AutotestPrivateSendAssistantTextQueryFunction| and
// |AutotestPrivateWaitForAssistantQueryStatusFunction|.
class AssistantInteractionHelper
    : public chromeos::assistant::AssistantInteractionSubscriber {
 public:
  using OnInteractionFinishedCallback =
      base::OnceCallback<void(const base::Optional<std::string>& error)>;

  AssistantInteractionHelper()
      : query_status_(std::make_unique<base::DictionaryValue>()) {}

  ~AssistantInteractionHelper() override {
    if (GetAssistant()) {
      GetAssistant()->RemoveAssistantInteractionSubscriber(this);
    }
  }

  void Init(OnInteractionFinishedCallback on_interaction_finished_callback) {
    // Subscribe to Assistant interaction events.
    GetAssistant()->AddAssistantInteractionSubscriber(this);

    on_interaction_finished_callback_ =
        std::move(on_interaction_finished_callback);
  }

  void SendTextQuery(const std::string& query, bool allow_tts) {
    // Start text interaction with Assistant server.
    GetAssistant()->StartTextInteraction(
        query, chromeos::assistant::AssistantQuerySource::kUnspecified,
        allow_tts);

    query_status_->SetKey("queryText", base::Value(query));
  }

  std::unique_ptr<base::DictionaryValue> GetQueryStatus() {
    return std::move(query_status_);
  }

  chromeos::assistant::Assistant* GetAssistant() {
    auto* assistant_service = chromeos::assistant::AssistantService::Get();
    return assistant_service ? assistant_service->GetAssistant() : nullptr;
  }

 private:
  // chromeos::assistant::AssistantInteractionSubscriber:
  using AssistantSuggestion = chromeos::assistant::AssistantSuggestion;
  using AssistantInteractionMetadata =
      chromeos::assistant::AssistantInteractionMetadata;
  using AssistantInteractionResolution =
      chromeos::assistant::AssistantInteractionResolution;

  void OnInteractionStarted(
      const AssistantInteractionMetadata& metadata) override {
    const bool is_voice_interaction =
        chromeos::assistant::AssistantInteractionType::kVoice == metadata.type;
    query_status_->SetKey("isMicOpen", base::Value(is_voice_interaction));
    interaction_in_progress_ = true;
  }

  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override {
    interaction_in_progress_ = false;

    // Only invoke the callback when |result_| is not empty to avoid an early
    // return before the entire session is completed. This happens when
    // sending queries to modify device settings, e.g. "turn on bluetooth",
    // which results in a round trip due to the need to fetch device state
    // on the client and return that to the server as part of a follow-up
    // interaction.
    if (result_.empty())
      return;

    query_status_->SetKey("queryResponse", std::move(result_));

    if (on_interaction_finished_callback_) {
      if (resolution == AssistantInteractionResolution::kNormal) {
        SendSuccessResponse();
      } else {
        SendErrorResponse("Interaction closed with resolution " +
                          ResolutionToString(resolution));
      }
    }
  }

  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override {
    result_.SetKey("htmlResponse", base::Value(response));
    CheckResponseIsValid(__FUNCTION__);
  }

  void OnTextResponse(const std::string& response) override {
    result_.SetKey("text", base::Value(response));
    CheckResponseIsValid(__FUNCTION__);
  }

  void OnOpenUrlResponse(const ::GURL& url, bool in_background) override {
    result_.SetKey("openUrl", base::Value(url.possibly_invalid_spec()));
  }

  void OnOpenAppResponse(
      const chromeos::assistant::AndroidAppInfo& app_info) override {
    result_.SetKey("openAppResponse", base::Value(app_info.package_name));
    CheckResponseIsValid(__FUNCTION__);
  }

  void OnSpeechRecognitionFinalResult(
      const std::string& final_result) override {
    query_status_->SetKey("queryText", base::Value(final_result));
  }

  void CheckResponseIsValid(const std::string& function_name) {
    if (!interaction_in_progress_) {
      // We should only get a response while the interaction is open
      // (started and not finished).
      SendErrorResponse(function_name +
                        " was called after the interaction was closed");
    }
  }

  void SendSuccessResponse() {
    std::move(on_interaction_finished_callback_).Run(base::nullopt);
  }

  void SendErrorResponse(const std::string& error) {
    std::move(on_interaction_finished_callback_).Run(error);
  }

  std::unique_ptr<base::DictionaryValue> query_status_;
  base::DictionaryValue result_;
  bool interaction_in_progress_ = false;

  // Callback triggered when interaction finished with non-empty response.
  OnInteractionFinishedCallback on_interaction_finished_callback_;

  DISALLOW_COPY_AND_ASSIGN(AssistantInteractionHelper);
};

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSendAssistantTextQueryFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSendAssistantTextQueryFunction::
    AutotestPrivateSendAssistantTextQueryFunction()
    : interaction_helper_(std::make_unique<AssistantInteractionHelper>()) {}

AutotestPrivateSendAssistantTextQueryFunction::
    ~AutotestPrivateSendAssistantTextQueryFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSendAssistantTextQueryFunction::Run() {
  DVLOG(1) << "AutotestPrivateSendAssistantTextQueryFunction";

  std::unique_ptr<api::autotest_private::SendAssistantTextQuery::Params> params(
      api::autotest_private::SendAssistantTextQuery::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  chromeos::assistant::AssistantAllowedState allowed_state =
      assistant::IsAssistantAllowedForProfile(profile);
  if (allowed_state != chromeos::assistant::AssistantAllowedState::ALLOWED) {
    return RespondNow(Error(base::StringPrintf(
        "Assistant not allowed - state: %d", allowed_state)));
  }

  interaction_helper_->Init(
      base::BindOnce(&AutotestPrivateSendAssistantTextQueryFunction::
                         OnInteractionFinishedCallback,
                     this));

  // Start text interaction with Assistant server.
  interaction_helper_->SendTextQuery(params->query, /*allow_tts=*/false);

  // Set up a delayed timer to wait for the query response and hold a reference
  // to |this| to avoid being destructed. Also make sure we stop and respond
  // when timeout.
  timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(params->timeout_ms),
      base::BindOnce(&AutotestPrivateSendAssistantTextQueryFunction::Timeout,
                     this));

  return RespondLater();
}

void AutotestPrivateSendAssistantTextQueryFunction::
    OnInteractionFinishedCallback(const base::Optional<std::string>& error) {
  DCHECK(!did_respond());
  if (error) {
    Respond(Error(error.value()));
  } else {
    Respond(OneArgument(base::Value::FromUniquePtrValue(
        interaction_helper_->GetQueryStatus())));
  }

  // |timeout_timer_| need to be hold until |Respond(.)| is called to avoid
  // |this| being destructed.
  timeout_timer_.AbandonAndStop();
}

void AutotestPrivateSendAssistantTextQueryFunction::Timeout() {
  DCHECK(!did_respond());
  Respond(Error("Assistant response timeout."));

  // Reset to unsubscribe OnInteractionFinishedCallback().
  interaction_helper_.reset();
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateWaitForAssistantQueryStatusFunction
///////////////////////////////////////////////////////////////////////////////
AutotestPrivateWaitForAssistantQueryStatusFunction::
    AutotestPrivateWaitForAssistantQueryStatusFunction()
    : interaction_helper_(std::make_unique<AssistantInteractionHelper>()) {}

AutotestPrivateWaitForAssistantQueryStatusFunction::
    ~AutotestPrivateWaitForAssistantQueryStatusFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateWaitForAssistantQueryStatusFunction::Run() {
  DVLOG(1) << "AutotestPrivateWaitForAssistantQueryStatusFunction";

  std::unique_ptr<api::autotest_private::WaitForAssistantQueryStatus::Params>
      params(api::autotest_private::WaitForAssistantQueryStatus::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  chromeos::assistant::AssistantAllowedState allowed_state =
      assistant::IsAssistantAllowedForProfile(profile);
  if (allowed_state != chromeos::assistant::AssistantAllowedState::ALLOWED) {
    return RespondNow(Error(base::StringPrintf(
        "Assistant not allowed - state: %d", allowed_state)));
  }

  interaction_helper_->Init(
      base::BindOnce(&AutotestPrivateWaitForAssistantQueryStatusFunction::
                         OnInteractionFinishedCallback,
                     this));

  // Start waiting for the response before time out.
  timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(params->timeout_s),
      base::BindOnce(
          &AutotestPrivateWaitForAssistantQueryStatusFunction::Timeout, this));
  return RespondLater();
}

void AutotestPrivateWaitForAssistantQueryStatusFunction::
    OnInteractionFinishedCallback(const base::Optional<std::string>& error) {
  DCHECK(!did_respond());
  if (error) {
    Respond(Error(error.value()));
  } else {
    Respond(OneArgument(base::Value::FromUniquePtrValue(
        interaction_helper_->GetQueryStatus())));
  }

  // |timeout_timer_| need to be hold until |Respond(.)| is called to avoid
  // |this| being destructed.
  timeout_timer_.AbandonAndStop();
}

void AutotestPrivateWaitForAssistantQueryStatusFunction::Timeout() {
  DCHECK(!did_respond());
  Respond(Error("No query response received before time out."));

  // Reset to unsubscribe OnInteractionFinishedCallback().
  interaction_helper_.reset();
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateIsArcPackageListInitialRefreshedFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateIsArcPackageListInitialRefreshedFunction::
    AutotestPrivateIsArcPackageListInitialRefreshedFunction() = default;

AutotestPrivateIsArcPackageListInitialRefreshedFunction::
    ~AutotestPrivateIsArcPackageListInitialRefreshedFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateIsArcPackageListInitialRefreshedFunction::Run() {
  DVLOG(1) << "AutotestPrivateIsArcPackageListInitialRefreshedFunction";

  ArcAppListPrefs* const prefs =
      ArcAppListPrefs::Get(Profile::FromBrowserContext(browser_context()));

  return RespondNow(
      OneArgument(base::Value(prefs->package_list_initial_refreshed())));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetWhitelistedPrefFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetWhitelistedPrefFunction::
    ~AutotestPrivateSetWhitelistedPrefFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetWhitelistedPrefFunction::Run() {
  DVLOG(1) << "AutotestPrivateSetWhitelistedPrefFunction";

  std::unique_ptr<api::autotest_private::SetWhitelistedPref::Params> params(
      api::autotest_private::SetWhitelistedPref::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& pref_name = params->pref_name;
  const base::Value& value = *(params->value);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const std::string& err_msg = SetWhitelistedPref(profile, pref_name, value);

  if (!err_msg.empty())
    return RespondNow(Error(err_msg));

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetCrostiniAppScaledFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetCrostiniAppScaledFunction::
    ~AutotestPrivateSetCrostiniAppScaledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetCrostiniAppScaledFunction::Run() {
  std::unique_ptr<api::autotest_private::SetCrostiniAppScaled::Params> params(
      api::autotest_private::SetCrostiniAppScaled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetCrostiniAppScaledFunction " << params->app_id
           << " " << params->scaled;

  ChromeLauncherController* const controller =
      ChromeLauncherController::instance();
  if (!controller)
    return RespondNow(Error("Controller not available"));

  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(
          controller->profile());
  if (!registry_service)
    return RespondNow(Error("Crostini registry not available"));

  registry_service->SetAppScaled(params->app_id, params->scaled);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetPrimaryDisplayScaleFactorFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetPrimaryDisplayScaleFactorFunction::
    ~AutotestPrivateGetPrimaryDisplayScaleFactorFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetPrimaryDisplayScaleFactorFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetPrimaryDisplayScaleFactorFunction";

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  float scale_factor = primary_display.device_scale_factor();
  return RespondNow(OneArgument(base::Value(scale_factor)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateIsTabletModeEnabledFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateIsTabletModeEnabledFunction::
    ~AutotestPrivateIsTabletModeEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateIsTabletModeEnabledFunction::Run() {
  DVLOG(1) << "AutotestPrivateIsTabletModeEnabledFunction";

  return RespondNow(
      OneArgument(base::Value(ash::TabletMode::Get()->InTabletMode())));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetTabletModeEnabledFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetTabletModeEnabledFunction::
    ~AutotestPrivateSetTabletModeEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetTabletModeEnabledFunction::Run() {
  DVLOG(1) << "AutotestPrivateSetTabletModeEnabledFunction";

  std::unique_ptr<api::autotest_private::SetTabletModeEnabled::Params> params(
      api::autotest_private::SetTabletModeEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  auto* tablet_mode = ash::TabletMode::Get();
  if (tablet_mode->InTabletMode() == params->enabled) {
    return RespondNow(
        OneArgument(base::Value(ash::TabletMode::Get()->InTabletMode())));
  }

  ash::TabletMode::Waiter waiter(params->enabled);
  if (!tablet_mode->ForceUiTabletModeState(params->enabled))
    return RespondNow(Error("failed to switch the tablet mode state"));
  waiter.Wait();
  return RespondNow(
      OneArgument(base::Value(ash::TabletMode::Get()->InTabletMode())));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetAllInstalledAppsFunction
///////////////////////////////////////////////////////////////////////////////
AutotestPrivateGetAllInstalledAppsFunction::
    AutotestPrivateGetAllInstalledAppsFunction() = default;

AutotestPrivateGetAllInstalledAppsFunction::
    ~AutotestPrivateGetAllInstalledAppsFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetAllInstalledAppsFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetAllInstalledAppsFunction";

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  std::vector<api::autotest_private::App> installed_apps;
  proxy->AppRegistryCache().ForEachApp([&installed_apps](
                                           const apps::AppUpdate& update) {
    api::autotest_private::App app;
    app.app_id = update.AppId();
    app.name = update.Name();
    app.short_name = update.ShortName();
    app.publisher_id = update.PublisherId();
    app.additional_search_terms = update.AdditionalSearchTerms();
    app.type = GetAppType(update.AppType());
    app.install_source = GetAppInstallSource(update.InstallSource());
    app.readiness = GetAppReadiness(update.Readiness());
    app.show_in_launcher = ConvertMojomOptionalBool(update.ShowInLauncher());
    app.show_in_search = ConvertMojomOptionalBool(update.ShowInSearch());
    installed_apps.emplace_back(std::move(app));
  });

  return RespondNow(
      ArgumentList(api::autotest_private::GetAllInstalledApps::Results::Create(
          installed_apps)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetShelfItemsFunction
///////////////////////////////////////////////////////////////////////////////
AutotestPrivateGetShelfItemsFunction::AutotestPrivateGetShelfItemsFunction() =
    default;

AutotestPrivateGetShelfItemsFunction::~AutotestPrivateGetShelfItemsFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateGetShelfItemsFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetShelfItemsFunction";

  ChromeLauncherController* const controller =
      ChromeLauncherController::instance();
  if (!controller)
    return RespondNow(Error("Controller not available"));

  std::vector<api::autotest_private::ShelfItem> result_items;
  for (const auto& item : controller->shelf_model()->items()) {
    api::autotest_private::ShelfItem result_item;
    result_item.app_id = item.id.app_id;
    result_item.launch_id = item.id.launch_id;
    result_item.title = base::UTF16ToUTF8(item.title);
    result_item.type = GetShelfItemType(item.type);
    result_item.status = GetShelfItemStatus(item.status);
    result_item.shows_tooltip = item.shows_tooltip;
    result_item.pinned_by_policy = item.pinned_by_policy;
    result_item.has_notification = item.has_notification;
    result_items.emplace_back(std::move(result_item));
  }

  return RespondNow(ArgumentList(
      api::autotest_private::GetShelfItems::Results::Create(result_items)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetShelfAutoHideBehaviorFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetShelfAutoHideBehaviorFunction::
    AutotestPrivateGetShelfAutoHideBehaviorFunction() = default;

AutotestPrivateGetShelfAutoHideBehaviorFunction::
    ~AutotestPrivateGetShelfAutoHideBehaviorFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetShelfAutoHideBehaviorFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetShelfAutoHideBehaviorFunction";

  std::unique_ptr<api::autotest_private::GetShelfAutoHideBehavior::Params>
      params(api::autotest_private::GetShelfAutoHideBehavior::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t display_id;
  if (!base::StringToInt64(params->display_id, &display_id)) {
    return RespondNow(Error(base::StrCat(
        {"Invalid display_id; expected string with numbers only, got ",
         params->display_id})));
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  ash::ShelfAutoHideBehavior behavior =
      ash::GetShelfAutoHideBehaviorPref(profile->GetPrefs(), display_id);
  std::string str_behavior;
  switch (behavior) {
    case ash::ShelfAutoHideBehavior::kAlways:
      str_behavior = "always";
      break;
    case ash::ShelfAutoHideBehavior::kNever:
      str_behavior = "never";
      break;
    case ash::ShelfAutoHideBehavior::kAlwaysHidden:
      // SHELF_AUTO_HIDE_ALWAYS_HIDDEN not supported by shelf_prefs.cc
      return RespondNow(Error("SHELF_AUTO_HIDE_ALWAYS_HIDDEN not supported"));
  }
  return RespondNow(OneArgument(base::Value(std::move(str_behavior))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetShelfAutoHideBehaviorFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetShelfAutoHideBehaviorFunction::
    AutotestPrivateSetShelfAutoHideBehaviorFunction() = default;

AutotestPrivateSetShelfAutoHideBehaviorFunction::
    ~AutotestPrivateSetShelfAutoHideBehaviorFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetShelfAutoHideBehaviorFunction::Run() {
  DVLOG(1) << "AutotestPrivateSetShelfAutoHideBehaviorFunction";

  std::unique_ptr<api::autotest_private::SetShelfAutoHideBehavior::Params>
      params(api::autotest_private::SetShelfAutoHideBehavior::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::ShelfAutoHideBehavior behavior;
  if (params->behavior == "always") {
    behavior = ash::ShelfAutoHideBehavior::kAlways;
  } else if (params->behavior == "never") {
    behavior = ash::ShelfAutoHideBehavior::kNever;
  } else {
    return RespondNow(Error(
        base::StrCat({"Invalid behavior; expected 'always', 'never', got ",
                      params->behavior})));
  }
  int64_t display_id;
  if (!base::StringToInt64(params->display_id, &display_id)) {
    return RespondNow(Error(base::StrCat(
        {"Invalid display_id; expected string with numbers only, got ",
         params->display_id})));
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  ash::SetShelfAutoHideBehaviorPref(profile->GetPrefs(), display_id, behavior);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetShelfAlignmentFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetShelfAlignmentFunction::
    AutotestPrivateGetShelfAlignmentFunction() = default;

AutotestPrivateGetShelfAlignmentFunction::
    ~AutotestPrivateGetShelfAlignmentFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetShelfAlignmentFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetShelfAlignmentFunction";

  std::unique_ptr<api::autotest_private::GetShelfAlignment::Params> params(
      api::autotest_private::GetShelfAlignment::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t display_id;
  if (!base::StringToInt64(params->display_id, &display_id)) {
    return RespondNow(Error(base::StrCat(
        {"Invalid display_id; expected string with numbers only, got ",
         params->display_id})));
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  ash::ShelfAlignment alignment =
      ash::GetShelfAlignmentPref(profile->GetPrefs(), display_id);
  api::autotest_private::ShelfAlignmentType alignment_type;
  switch (alignment) {
    case ash::ShelfAlignment::kBottom:
      alignment_type = api::autotest_private::ShelfAlignmentType::
          SHELF_ALIGNMENT_TYPE_BOTTOM;
      break;
    case ash::ShelfAlignment::kLeft:
      alignment_type =
          api::autotest_private::ShelfAlignmentType::SHELF_ALIGNMENT_TYPE_LEFT;
      break;
    case ash::ShelfAlignment::kRight:
      alignment_type =
          api::autotest_private::ShelfAlignmentType::SHELF_ALIGNMENT_TYPE_RIGHT;
      break;
    case ash::ShelfAlignment::kBottomLocked:
      // ShelfAlignment::kBottomLocked not supported by
      // shelf_prefs.cc
      return RespondNow(Error("ShelfAlignment::kBottomLocked not supported"));
  }
  return RespondNow(OneArgument(
      base::Value(api::autotest_private::ToString(alignment_type))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetShelfAlignmentFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetShelfAlignmentFunction::
    AutotestPrivateSetShelfAlignmentFunction() = default;

AutotestPrivateSetShelfAlignmentFunction::
    ~AutotestPrivateSetShelfAlignmentFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetShelfAlignmentFunction::Run() {
  DVLOG(1) << "AutotestPrivateSetShelfAlignmentFunction";

  std::unique_ptr<api::autotest_private::SetShelfAlignment::Params> params(
      api::autotest_private::SetShelfAlignment::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::ShelfAlignment alignment;
  switch (params->alignment) {
    case api::autotest_private::ShelfAlignmentType::SHELF_ALIGNMENT_TYPE_BOTTOM:
      alignment = ash::ShelfAlignment::kBottom;
      break;
    case api::autotest_private::ShelfAlignmentType::SHELF_ALIGNMENT_TYPE_LEFT:
      alignment = ash::ShelfAlignment::kLeft;
      break;
    case api::autotest_private::ShelfAlignmentType::SHELF_ALIGNMENT_TYPE_RIGHT:
      alignment = ash::ShelfAlignment::kRight;
      break;
    case api::autotest_private::ShelfAlignmentType::SHELF_ALIGNMENT_TYPE_NONE:
      return RespondNow(
          Error("Invalid None alignment; expected 'Bottom', 'Left', or "
                "'Right'"));
  }
  int64_t display_id;
  if (!base::StringToInt64(params->display_id, &display_id)) {
    return RespondNow(Error(base::StrCat(
        {"Invalid display_id; expected string with numbers only, got ",
         params->display_id})));
  }

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  ash::SetShelfAlignmentPref(profile->GetPrefs(), display_id, alignment);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateWaitForOverviewStateFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateWaitForOverviewStateFunction::
    AutotestPrivateWaitForOverviewStateFunction() = default;
AutotestPrivateWaitForOverviewStateFunction::
    ~AutotestPrivateWaitForOverviewStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateWaitForOverviewStateFunction::Run() {
  std::unique_ptr<api::autotest_private::WaitForOverviewState::Params> params(
      api::autotest_private::WaitForOverviewState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  const ash::OverviewAnimationState overview_state =
      ToOverviewAnimationState(params->overview_state);
  ash::OverviewTestApi().WaitForOverviewState(
      overview_state,
      base::BindOnce(&AutotestPrivateWaitForOverviewStateFunction::Done, this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutotestPrivateWaitForOverviewStateFunction::Done(bool success) {
  if (!success) {
    Respond(Error("Overview animation was canceled."));
    return;
  }
  Respond(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetOverviewModeStateFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetOverviewModeStateFunction::
    AutotestPrivateSetOverviewModeStateFunction() = default;

AutotestPrivateSetOverviewModeStateFunction::
    ~AutotestPrivateSetOverviewModeStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetOverviewModeStateFunction::Run() {
  std::unique_ptr<api::autotest_private::SetOverviewModeState::Params> params(
      api::autotest_private::SetOverviewModeState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::OverviewTestApi().SetOverviewMode(
      params->start,
      base::BindOnce(
          &AutotestPrivateSetOverviewModeStateFunction::OnOverviewModeChanged,
          this, params->start));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutotestPrivateSetOverviewModeStateFunction::OnOverviewModeChanged(
    bool for_start,
    bool finished) {
  auto arg = OneArgument(base::Value(finished));
  // On starting the overview animation, it needs to wait for 1 extra second
  // to trigger the occlusion tracker.
  if (for_start) {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AutotestPrivateSetOverviewModeStateFunction::Respond,
                       this, std::move(arg)),
        base::TimeDelta::FromSeconds(1));
  } else {
    Respond(std::move(arg));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateShowVirtualKeyboardIfEnabledFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateShowVirtualKeyboardIfEnabledFunction::
    AutotestPrivateShowVirtualKeyboardIfEnabledFunction() = default;
AutotestPrivateShowVirtualKeyboardIfEnabledFunction::
    ~AutotestPrivateShowVirtualKeyboardIfEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateShowVirtualKeyboardIfEnabledFunction::Run() {
  if (!ui::IMEBridge::Get() ||
      !ui::IMEBridge::Get()->GetInputContextHandler() ||
      !ui::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod()) {
    return RespondNow(NoArguments());
  }

  ui::IMEBridge::Get()
      ->GetInputContextHandler()
      ->GetInputMethod()
      ->ShowVirtualKeyboardIfEnabled();
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateArcAppTracingStartFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateArcAppTracingStartFunction::
    AutotestPrivateArcAppTracingStartFunction() = default;
AutotestPrivateArcAppTracingStartFunction::
    ~AutotestPrivateArcAppTracingStartFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateArcAppTracingStartFunction::Run() {
  DVLOG(1) << "AutotestPrivateArcAppTracingStartFunction";

  arc::ArcAppPerformanceTracing* const tracing =
      arc::ArcAppPerformanceTracing::GetForBrowserContext(browser_context());
  if (!tracing)
    return RespondNow(Error("No ARC performance tracing is available."));

  if (!tracing->StartCustomTracing())
    return RespondNow(Error("Failed to start custom tracing."));

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateArcAppTracingStopAndAnalyzeFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateArcAppTracingStopAndAnalyzeFunction::
    AutotestPrivateArcAppTracingStopAndAnalyzeFunction() = default;
AutotestPrivateArcAppTracingStopAndAnalyzeFunction::
    ~AutotestPrivateArcAppTracingStopAndAnalyzeFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateArcAppTracingStopAndAnalyzeFunction::Run() {
  DVLOG(1) << "AutotestPrivateArcAppTracingStopAndAnalyzeFunction";

  arc::ArcAppPerformanceTracing* const tracing =
      arc::ArcAppPerformanceTracing::GetForBrowserContext(browser_context());
  if (!tracing)
    return RespondNow(Error("No ARC performance tracing is available."));

  tracing->StopCustomTracing(base::BindOnce(
      &AutotestPrivateArcAppTracingStopAndAnalyzeFunction::OnTracingResult,
      this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutotestPrivateArcAppTracingStopAndAnalyzeFunction::OnTracingResult(
    bool success,
    double fps,
    double commit_deviation,
    double render_quality) {
  auto result = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  result->SetBoolKey("success", success);
  result->SetDoubleKey("fps", fps);
  result->SetDoubleKey("commitDeviation", commit_deviation);
  result->SetDoubleKey("renderQuality", render_quality);
  Respond(OneArgument(base::Value::FromUniquePtrValue(std::move(result))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetArcAppWindowFocusFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetArcAppWindowFocusFunction::
    AutotestPrivateSetArcAppWindowFocusFunction() = default;
AutotestPrivateSetArcAppWindowFocusFunction::
    ~AutotestPrivateSetArcAppWindowFocusFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetArcAppWindowFocusFunction::Run() {
  std::unique_ptr<api::autotest_private::SetArcAppWindowFocus::Params> params(
      api::autotest_private::SetArcAppWindowFocus::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetArcAppWindowFocusFunction "
           << params->package_name;
  aura::Window* arc_window = GetArcAppWindow(params->package_name);
  if (!arc_window) {
    return RespondNow(Error(base::StrCat(
        {"No ARC app window was found for ", params->package_name})));
  }
  if (!arc_window->CanFocus()) {
    return RespondNow(Error(base::StrCat(
        {"ARC app window can't focus for ", params->package_name})));
  }
  // No matter whether it is focused already, set it focused.
  arc_window->Focus();
  if (!arc_window->HasFocus()) {
    return RespondNow(Error(base::StrCat(
        {"Failed to set focus for ARC App window ", params->package_name})));
  }
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSwapWindowsInSplitViewFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSwapWindowsInSplitViewFunction::
    AutotestPrivateSwapWindowsInSplitViewFunction() = default;
AutotestPrivateSwapWindowsInSplitViewFunction::
    ~AutotestPrivateSwapWindowsInSplitViewFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSwapWindowsInSplitViewFunction::Run() {
  ash::SplitViewTestApi().SwapWindows();
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateWaitForDisplayRotationFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateWaitForDisplayRotationFunction::
    AutotestPrivateWaitForDisplayRotationFunction() = default;
AutotestPrivateWaitForDisplayRotationFunction::
    ~AutotestPrivateWaitForDisplayRotationFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateWaitForDisplayRotationFunction::Run() {
  DVLOG(1) << "AutotestPrivateWaitForDisplayRotationFunction";

  std::unique_ptr<api::autotest_private::WaitForDisplayRotation::Params> params(
      api::autotest_private::WaitForDisplayRotation::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!base::StringToInt64(params->display_id, &display_id_)) {
    return RespondNow(Error(base::StrCat(
        {"Invalid display_id; expected string with numbers only, got ",
         params->display_id})));
  }

  if (params->rotation ==
      api::autotest_private::RotationType::ROTATION_TYPE_ROTATEANY) {
    display::Display display;
    if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id_,
                                                               &display)) {
      return RespondNow(Error(base::StrCat(
          {"Display is not found for display_id ", params->display_id})));
    }
    DCHECK(display.is_valid());
    if (!display.IsInternal()) {
      return RespondNow(
          Error("RotateAny is valid only for the internal display"));
    }
    auto* screen_orientation_controller =
        ash::Shell::Get()->screen_orientation_controller();
    if (screen_orientation_controller->user_rotation_locked()) {
      self_ = this;
      screen_orientation_controller->AddObserver(this);
      return RespondLater();
    }
    target_rotation_.reset();
  } else {
    target_rotation_ = ToRotation(params->rotation);
  }

  auto result = CheckScreenRotationAnimation();
  if (result)
    return RespondNow(std::move(result));
  return RespondLater();
}

void AutotestPrivateWaitForDisplayRotationFunction::
    OnScreenCopiedBeforeRotation() {}

void AutotestPrivateWaitForDisplayRotationFunction::
    OnScreenRotationAnimationFinished(ash::ScreenRotationAnimator* animator,
                                      bool canceled) {
  animator->RemoveObserver(this);

  display::Display display;
  display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id_, &display);
  Respond(OneArgument(base::Value(display.is_valid() &&
                                  (!target_rotation_.has_value() ||
                                   display.rotation() == *target_rotation_))));
  self_.reset();
}

void AutotestPrivateWaitForDisplayRotationFunction::
    OnUserRotationLockChanged() {
  auto* screen_orientation_controller =
      ash::Shell::Get()->screen_orientation_controller();
  if (screen_orientation_controller->user_rotation_locked())
    return;
  screen_orientation_controller->RemoveObserver(this);
  self_.reset();
  target_rotation_.reset();
  auto result = CheckScreenRotationAnimation();
  // Wait for the rotation if unlocking causes rotation.
  if (result)
    Respond(std::move(result));
}

ExtensionFunction::ResponseValue
AutotestPrivateWaitForDisplayRotationFunction::CheckScreenRotationAnimation() {
  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id_);
  if (!root_window) {
    return Error(base::StringPrintf(
        "Invalid display_id; no root window found for the display id %" PRId64,
        display_id_));
  }
  auto* animator = ash::ScreenRotationAnimator::GetForRootWindow(root_window);
  if (!animator->IsRotating()) {
    display::Display display;
    display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id_,
                                                          &display);
    // This should never fail.
    DCHECK(display.is_valid());
    return OneArgument(base::Value(!target_rotation_.has_value() ||
                                   display.rotation() == *target_rotation_));
  }
  self_ = this;

  animator->AddObserver(this);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetAppWindowListFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetAppWindowListFunction::
    AutotestPrivateGetAppWindowListFunction() = default;
AutotestPrivateGetAppWindowListFunction::
    ~AutotestPrivateGetAppWindowListFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetAppWindowListFunction::Run() {
  // Use negative number to avoid potential collision with normal use if any.
  static int id_count = -10000;

  base::Optional<ash::OverviewInfo> overview_info =
      ash::OverviewTestApi().GetOverviewInfo();

  auto window_list = ash::GetAppWindowList();
  std::vector<api::autotest_private::AppWindowInfo> result_list;

  for (auto* window : window_list) {
    if (window->id() == aura::Window::kInitialId)
      window->set_id(id_count--);
    api::autotest_private::AppWindowInfo window_info;
    window_info.id = window->id();
    window_info.name = window->GetName();
    window_info.window_type = GetAppWindowType(
        static_cast<ash::AppType>(window->GetProperty(aura::client::kAppType)));
    window_info.state_type =
        ToWindowStateType(window->GetProperty(chromeos::kWindowStateTypeKey));
    window_info.bounds_in_root =
        ToBoundsDictionary(window->GetBoundsInRootWindow());
    window_info.target_bounds = ToBoundsDictionary(window->GetTargetBounds());
    window_info.display_id = base::NumberToString(
        display::Screen::GetScreen()->GetDisplayNearestWindow(window).id());
    window_info.title = base::UTF16ToUTF8(window->GetTitle());
    window_info.is_animating = window->layer()->GetAnimator()->is_animating();
    window_info.is_visible = window->IsVisible();
    window_info.target_visibility = window->TargetVisibility();
    window_info.can_focus = window->CanFocus();
    window_info.has_focus = window->HasFocus();
    window_info.on_active_desk =
        ash::DesksHelper::Get()->BelongsToActiveDesk(window);
    window_info.is_active = wm::IsActiveWindow(window);
    window_info.has_capture = window->HasCapture();
    window_info.can_resize =
        (window->GetProperty(aura::client::kResizeBehaviorKey) &
         aura::client::kResizeBehaviorCanResize) != 0;

    if (window->GetProperty(aura::client::kAppType) ==
        static_cast<int>(ash::AppType::ARC_APP)) {
      std::string* package_name = window->GetProperty(ash::kArcPackageNameKey);
      if (package_name) {
        window_info.arc_package_name =
            std::make_unique<std::string>(*package_name);
      } else {
        LOG(ERROR) << "The package name for window " << window->GetTitle()
                   << " (ID: " << window->id()
                   << ") isn't available even though it is an ARC window.";
      }
    }

    // Frame information
    auto* immersive_controller = chromeos::ImmersiveFullscreenController::Get(
        views::Widget::GetWidgetForNativeWindow(window));
    if (immersive_controller) {
      // The widget that hosts the immersive frame can be different from the
      // application's widget itself. Use the widget from the immersive
      // controller to obtain the FrameHeader.
      auto* widget = immersive_controller->widget();
      if (immersive_controller->IsEnabled()) {
        window_info.frame_mode =
            api::autotest_private::FrameMode::FRAME_MODE_IMMERSIVE;
        window_info.is_frame_visible = immersive_controller->IsRevealed();
      } else {
        window_info.frame_mode =
            api::autotest_private::FrameMode::FRAME_MODE_NORMAL;
        window_info.is_frame_visible = IsFrameVisible(widget);
      }
      auto* frame_header = chromeos::FrameHeader::Get(widget);
      window_info.caption_height = frame_header->GetHeaderHeight();

      const chromeos::CaptionButtonModel* button_model =
          frame_header->GetCaptionButtonModel();
      int caption_button_enabled_status = 0;
      int caption_button_visible_status = 0;

      constexpr views::CaptionButtonIcon all_button_icons[] = {
          views::CAPTION_BUTTON_ICON_MINIMIZE,
          views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
          views::CAPTION_BUTTON_ICON_CLOSE,
          views::CAPTION_BUTTON_ICON_LEFT_SNAPPED,
          views::CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
          views::CAPTION_BUTTON_ICON_BACK,
          views::CAPTION_BUTTON_ICON_LOCATION,
          views::CAPTION_BUTTON_ICON_MENU,
          views::CAPTION_BUTTON_ICON_ZOOM};

      for (const auto button : all_button_icons) {
        if (button_model->IsEnabled(button))
          caption_button_enabled_status |= (1 << button);
        if (button_model->IsVisible(button))
          caption_button_visible_status |= (1 << button);
      }
      window_info.caption_button_enabled_status = caption_button_enabled_status;
      window_info.caption_button_visible_status = caption_button_visible_status;
    } else {
      auto* widget = views::Widget::GetWidgetForNativeWindow(window);
      // All widgets for app windows in chromeos should have a frame with
      // immersive controller. Non app windows may not have a frame and
      // frame mode will be NONE.
      DCHECK(!widget || widget->GetNativeWindow()->type() !=
                            aura::client::WINDOW_TYPE_NORMAL);
      window_info.frame_mode =
          api::autotest_private::FrameMode::FRAME_MODE_NONE;
      window_info.is_frame_visible = false;
    }

    // Overview info.
    if (overview_info.has_value()) {
      auto it = overview_info->find(window);
      if (it != overview_info->end()) {
        window_info.overview_info =
            std::make_unique<api::autotest_private::OverviewInfo>();
        window_info.overview_info->bounds =
            ToBoundsDictionary(it->second.bounds_in_screen);
        window_info.overview_info->is_dragged = it->second.is_dragged;
      }
    }

    result_list.emplace_back(std::move(window_info));
  }
  return RespondNow(ArgumentList(
      api::autotest_private::GetAppWindowList::Results::Create(result_list)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetAppWindowStateFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetAppWindowStateFunction::
    AutotestPrivateSetAppWindowStateFunction() = default;
AutotestPrivateSetAppWindowStateFunction::
    ~AutotestPrivateSetAppWindowStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetAppWindowStateFunction::Run() {
  std::unique_ptr<api::autotest_private::SetAppWindowState::Params> params(
      api::autotest_private::SetAppWindowState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetAppWindowStateFunction " << params->id;

  aura::Window* window = FindAppWindowById(params->id);
  if (!window) {
    return RespondNow(Error(
        base::StringPrintf("No app window was found : id=%d", params->id)));
  }

  chromeos::WindowStateType expected_state =
      GetExpectedWindowState(params->change.event_type);
  if (window->GetProperty(chromeos::kWindowStateTypeKey) == expected_state) {
    if (params->change.fail_if_no_change &&
        *(params->change.fail_if_no_change)) {
      return RespondNow(
          Error("The app window was already in the expected window state! "));
    } else {
      return RespondNow(OneArgument(base::Value(
          api::autotest_private::ToString(ToWindowStateType(expected_state)))));
    }
  }

  window_state_observer_ = std::make_unique<WindowStateChangeObserver>(
      window, expected_state,
      base::BindOnce(
          &AutotestPrivateSetAppWindowStateFunction::WindowStateChanged, this,
          expected_state));

  // TODO(crbug.com/990713): Make WMEvent trigger split view in tablet mode.
  if (ash::TabletMode::Get()->InTabletMode()) {
    if (expected_state == chromeos::WindowStateType::kLeftSnapped) {
      ash::SplitViewTestApi().SnapWindow(
          window, ash::SplitViewTestApi::SnapPosition::LEFT);
      return RespondLater();
    } else if (expected_state == chromeos::WindowStateType::kRightSnapped) {
      ash::SplitViewTestApi().SnapWindow(
          window, ash::SplitViewTestApi::SnapPosition::RIGHT);
      return RespondLater();
    }
  }

  const ash::WMEvent event(ToWMEventType(params->change.event_type));
  ash::WindowState::Get(window)->OnWMEvent(&event);

  return RespondLater();
}

void AutotestPrivateSetAppWindowStateFunction::WindowStateChanged(
    chromeos::WindowStateType expected_type,
    bool success) {
  if (!success) {
    Respond(Error(
        "The app window was destroyed while waiting for its state change! "));
  } else {
    Respond(OneArgument(base::Value(
        api::autotest_private::ToString(ToWindowStateType(expected_type)))));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateCloseAppWindowFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateCloseAppWindowFunction::
    ~AutotestPrivateCloseAppWindowFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateCloseAppWindowFunction::Run() {
  std::unique_ptr<api::autotest_private::CloseAppWindow::Params> params(
      api::autotest_private::CloseAppWindow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateCloseAppWindowFunction " << params->id;

  auto* window = FindAppWindowById(params->id);
  if (!window) {
    return RespondNow(Error(
        base::StringPrintf("No app window was found : id=%d", params->id)));
  }
  auto* widget = views::Widget::GetWidgetForNativeWindow(window);
  widget->Close();
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateInstallPWAForCurrentURL
///////////////////////////////////////////////////////////////////////////////

// Used to notify when when a certain URL contains a WPA.
class AutotestPrivateInstallPWAForCurrentURLFunction::PWABannerObserver
    : public webapps::AppBannerManager::Observer {
 public:
  PWABannerObserver(webapps::AppBannerManager* manager,
                    base::OnceCallback<void()> callback)
      : callback_(std::move(callback)), app_banner_manager_(manager) {
    DCHECK(manager);
    observer_.Add(manager);

    // If PWA is already loaded, call callback immediately.
    Installable installable =
        app_banner_manager_->GetInstallableWebAppCheckResultForTesting();
    if (installable == Installable::kPromotable ||
        installable == Installable::kByUserRequest) {
      observer_.RemoveAll();
      std::move(callback_).Run();
    }
  }
  ~PWABannerObserver() override {}

  void OnInstallableWebAppStatusUpdated() override {
    Installable installable =
        app_banner_manager_->GetInstallableWebAppCheckResultForTesting();
    switch (installable) {
      case Installable::kNo:
        FALLTHROUGH;
      case Installable::kNoAlreadyInstalled:
        FALLTHROUGH;
      case Installable::kUnknown:
        DCHECK(false) << "Unexpected AppBannerManager::Installable value (kNo "
                         "or kNoAlreadyInstalled or kUnknown)";
        break;

      case Installable::kPromotable:
        FALLTHROUGH;
      case Installable::kByUserRequest:
        observer_.RemoveAll();
        std::move(callback_).Run();
        break;
    }
  }

 private:
  using Installable = webapps::AppBannerManager::InstallableWebAppCheckResult;

  ScopedObserver<webapps::AppBannerManager, webapps::AppBannerManager::Observer>
      observer_{this};
  base::OnceCallback<void()> callback_;
  webapps::AppBannerManager* app_banner_manager_;

  DISALLOW_COPY_AND_ASSIGN(PWABannerObserver);
};

// Used to notify when a WPA is installed.
class AutotestPrivateInstallPWAForCurrentURLFunction::PWARegistrarObserver
    : public web_app::AppRegistrarObserver {
 public:
  PWARegistrarObserver(Profile* profile,
                       base::OnceCallback<void(const web_app::AppId&)> callback)
      : callback_(std::move(callback)) {
    observer_.Add(
        &web_app::WebAppProviderBase::GetProviderBase(profile)->registrar());
  }
  ~PWARegistrarObserver() override {}

  void OnWebAppInstalled(const web_app::AppId& app_id) override {
    observer_.RemoveAll();
    std::move(callback_).Run(app_id);
  }

 private:
  ScopedObserver<web_app::AppRegistrar, web_app::AppRegistrarObserver>
      observer_{this};
  base::OnceCallback<void(const web_app::AppId&)> callback_;

  DISALLOW_COPY_AND_ASSIGN(PWARegistrarObserver);
};

AutotestPrivateInstallPWAForCurrentURLFunction::
    AutotestPrivateInstallPWAForCurrentURLFunction() = default;
AutotestPrivateInstallPWAForCurrentURLFunction::
    ~AutotestPrivateInstallPWAForCurrentURLFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateInstallPWAForCurrentURLFunction::Run() {
  DVLOG(1) << "AutotestPrivateInstallPWAForCurrentURLFunction";

  std::unique_ptr<api::autotest_private::InstallPWAForCurrentURL::Params>
      params(api::autotest_private::InstallPWAForCurrentURL::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  Browser* browser = GetFirstRegularBrowser();
  if (!browser) {
    return RespondNow(Error("Failed to find regular browser"));
  }
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();

  webapps::AppBannerManager* app_banner_manager =
      webapps::AppBannerManagerDesktop::FromWebContents(web_contents);
  if (!app_banner_manager) {
    return RespondNow(Error("Failed to create AppBannerManager"));
  }

  banner_observer_ = std::make_unique<PWABannerObserver>(
      app_banner_manager,
      base::BindOnce(&AutotestPrivateInstallPWAForCurrentURLFunction::PWALoaded,
                     this));

  // Adding timeout to catch:
  // - There is no way to know whether ExecuteCommand fails.
  // - Current URL might not have a valid PWA.
  timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(params->timeout_ms),
      base::BindOnce(
          &AutotestPrivateInstallPWAForCurrentURLFunction::PWATimeout, this));
  return RespondLater();
}

void AutotestPrivateInstallPWAForCurrentURLFunction::PWALoaded() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  Browser* browser = GetFirstRegularBrowser();

  registrar_observer_ = std::make_unique<PWARegistrarObserver>(
      profile,
      base::BindOnce(
          &AutotestPrivateInstallPWAForCurrentURLFunction::PWAInstalled, this));

  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  if (!chrome::ExecuteCommand(browser, IDC_INSTALL_PWA)) {
    return Respond(Error("Failed to execute INSTALL_PWA command"));
  }
}

void AutotestPrivateInstallPWAForCurrentURLFunction::PWAInstalled(
    const web_app::AppId& app_id) {
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);
  Respond(OneArgument(base::Value(app_id)));
  timeout_timer_.AbandonAndStop();
}

void AutotestPrivateInstallPWAForCurrentURLFunction::PWATimeout() {
  chrome::SetAutoAcceptPWAInstallConfirmationForTesting(false);
  Respond(Error("Install PWA timed out"));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateActivateAcceleratorFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateActivateAcceleratorFunction::
    AutotestPrivateActivateAcceleratorFunction() = default;
AutotestPrivateActivateAcceleratorFunction::
    ~AutotestPrivateActivateAcceleratorFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateActivateAcceleratorFunction::Run() {
  std::unique_ptr<api::autotest_private::ActivateAccelerator::Params> params(
      api::autotest_private::ActivateAccelerator::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int modifiers = (params->accelerator.control ? ui::EF_CONTROL_DOWN : 0) |
                  (params->accelerator.shift ? ui::EF_SHIFT_DOWN : 0) |
                  (params->accelerator.alt ? ui::EF_ALT_DOWN : 0) |
                  (params->accelerator.search ? ui::EF_COMMAND_DOWN : 0);
  ui::Accelerator accelerator(
      StringToKeyCode(params->accelerator.key_code), modifiers,
      params->accelerator.pressed ? ui::Accelerator::KeyState::PRESSED
                                  : ui::Accelerator::KeyState::RELEASED);
  auto* accelerator_controller = ash::AcceleratorController::Get();
  accelerator_controller->GetAcceleratorHistory()->StoreCurrentAccelerator(
      accelerator);

  if (!accelerator_controller->IsRegistered(accelerator)) {
    // If it's not ash accelerator, try aplication's accelerator.
    auto* window = GetActiveWindow();
    if (!window) {
      return RespondNow(
          Error(base::StringPrintf("Accelerator is not registered 1")));
    }
    auto* widget = views::Widget::GetWidgetForNativeWindow(window);
    if (!widget) {
      return RespondNow(
          Error(base::StringPrintf("Accelerator is not registered 2")));
    }
    bool result = widget->GetFocusManager()->ProcessAccelerator(accelerator);
    return RespondNow(OneArgument(base::Value(result)));
  }
  bool result = accelerator_controller->Process(accelerator);
  return RespondNow(OneArgument(base::Value(result)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateWaitForLauncherStateFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateWaitForLauncherStateFunction::
    AutotestPrivateWaitForLauncherStateFunction() = default;
AutotestPrivateWaitForLauncherStateFunction::
    ~AutotestPrivateWaitForLauncherStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateWaitForLauncherStateFunction::Run() {
  std::unique_ptr<api::autotest_private::WaitForLauncherState::Params> params(
      api::autotest_private::WaitForLauncherState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  auto target_state = ToAppListViewState(params->launcher_state);
  if (WaitForLauncherState(
          target_state,
          base::BindOnce(&AutotestPrivateWaitForLauncherStateFunction::Done,
                         this))) {
    return AlreadyResponded();
  }
  return RespondLater();
}

void AutotestPrivateWaitForLauncherStateFunction::Done() {
  Respond(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateCreateNewDeskFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateCreateNewDeskFunction::AutotestPrivateCreateNewDeskFunction() =
    default;
AutotestPrivateCreateNewDeskFunction::~AutotestPrivateCreateNewDeskFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateCreateNewDeskFunction::Run() {
  const bool success = ash::AutotestDesksApi().CreateNewDesk();
  return RespondNow(OneArgument(base::Value(success)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateActivateDeskAtIndexFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateActivateDeskAtIndexFunction::
    AutotestPrivateActivateDeskAtIndexFunction() = default;
AutotestPrivateActivateDeskAtIndexFunction::
    ~AutotestPrivateActivateDeskAtIndexFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateActivateDeskAtIndexFunction::Run() {
  std::unique_ptr<api::autotest_private::ActivateDeskAtIndex::Params> params(
      api::autotest_private::ActivateDeskAtIndex::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!ash::AutotestDesksApi().ActivateDeskAtIndex(
          params->index,
          base::BindOnce(
              &AutotestPrivateActivateDeskAtIndexFunction::OnAnimationComplete,
              this))) {
    return RespondNow(OneArgument(base::Value(false)));
  }

  return RespondLater();
}

void AutotestPrivateActivateDeskAtIndexFunction::OnAnimationComplete() {
  Respond(OneArgument(base::Value(true)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRemoveActiveDeskFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRemoveActiveDeskFunction::
    AutotestPrivateRemoveActiveDeskFunction() = default;
AutotestPrivateRemoveActiveDeskFunction::
    ~AutotestPrivateRemoveActiveDeskFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRemoveActiveDeskFunction::Run() {
  if (!ash::AutotestDesksApi().RemoveActiveDesk(base::BindOnce(
          &AutotestPrivateRemoveActiveDeskFunction::OnAnimationComplete,
          this))) {
    return RespondNow(OneArgument(base::Value(false)));
  }

  return RespondLater();
}

void AutotestPrivateRemoveActiveDeskFunction::OnAnimationComplete() {
  Respond(OneArgument(base::Value(true)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateActivateAdjacentDesksToTargetIndexFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateActivateAdjacentDesksToTargetIndexFunction::
    AutotestPrivateActivateAdjacentDesksToTargetIndexFunction() = default;
AutotestPrivateActivateAdjacentDesksToTargetIndexFunction::
    ~AutotestPrivateActivateAdjacentDesksToTargetIndexFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateActivateAdjacentDesksToTargetIndexFunction::Run() {
  std::unique_ptr<
      api::autotest_private::ActivateAdjacentDesksToTargetIndex::Params>
      params(api::autotest_private::ActivateAdjacentDesksToTargetIndex::Params::
                 Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!ash::AutotestDesksApi().ActivateAdjacentDesksToTargetIndex(
          params->index,
          base::BindOnce(
              &AutotestPrivateActivateAdjacentDesksToTargetIndexFunction::
                  OnAnimationComplete,
              this))) {
    return RespondNow(OneArgument(base::Value(false)));
  }

  return RespondLater();
}

void AutotestPrivateActivateAdjacentDesksToTargetIndexFunction::
    OnAnimationComplete() {
  Respond(OneArgument(base::Value(true)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateMouseClickFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateMouseClickFunction::AutotestPrivateMouseClickFunction() =
    default;

AutotestPrivateMouseClickFunction::~AutotestPrivateMouseClickFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateMouseClickFunction::Run() {
  std::unique_ptr<api::autotest_private::MouseClick::Params> params(
      api::autotest_private::MouseClick::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* env = aura::Env::GetInstance();
  if (env->mouse_button_flags() != 0) {
    return RespondNow(Error(base::StringPrintf("Already pressed; flags %d",
                                               env->mouse_button_flags())));
  }

  int64_t display_id = ash::Shell::Get()->cursor_manager()->GetDisplay().id();
  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  if (!root_window)
    return RespondNow(Error("Failed to find the root window"));

  gfx::PointF location_in_host(env->last_mouse_location().x(),
                               env->last_mouse_location().y());
  wm::ConvertPointFromScreen(root_window, &location_in_host);
  ConvertPointToHost(root_window, &location_in_host);

  int flags = GetMouseEventFlags(params->button);
  event_generator_ = std::make_unique<EventGenerator>(
      root_window->GetHost(),
      base::BindOnce(&AutotestPrivateMouseClickFunction::Respond, this,
                     NoArguments()));
  event_generator_->ScheduleMouseEvent(ui::ET_MOUSE_PRESSED, location_in_host,
                                       flags);
  event_generator_->ScheduleMouseEvent(ui::ET_MOUSE_RELEASED, location_in_host,
                                       flags);
  event_generator_->Run();

  return RespondLater();
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateMousePressFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateMousePressFunction::AutotestPrivateMousePressFunction() =
    default;
AutotestPrivateMousePressFunction::~AutotestPrivateMousePressFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateMousePressFunction::Run() {
  std::unique_ptr<api::autotest_private::MousePress::Params> params(
      api::autotest_private::MousePress::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* env = aura::Env::GetInstance();
  int input_flags = GetMouseEventFlags(params->button);
  if ((input_flags | env->mouse_button_flags()) == env->mouse_button_flags())
    return RespondNow(NoArguments());

  int64_t display_id = ash::Shell::Get()->cursor_manager()->GetDisplay().id();
  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  if (!root_window)
    return RespondNow(Error("Failed to find the root window"));

  gfx::PointF location_in_host(env->last_mouse_location().x(),
                               env->last_mouse_location().y());
  wm::ConvertPointFromScreen(root_window, &location_in_host);
  ConvertPointToHost(root_window, &location_in_host);

  event_generator_ = std::make_unique<EventGenerator>(
      root_window->GetHost(),
      base::BindOnce(&AutotestPrivateMousePressFunction::Respond, this,
                     NoArguments()));
  event_generator_->ScheduleMouseEvent(ui::ET_MOUSE_PRESSED, location_in_host,
                                       input_flags);
  event_generator_->Run();

  return RespondLater();
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateMouseReleaseFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateMouseReleaseFunction::AutotestPrivateMouseReleaseFunction() =
    default;
AutotestPrivateMouseReleaseFunction::~AutotestPrivateMouseReleaseFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateMouseReleaseFunction::Run() {
  std::unique_ptr<api::autotest_private::MouseRelease::Params> params(
      api::autotest_private::MouseRelease::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* env = aura::Env::GetInstance();

  int input_flags = GetMouseEventFlags(params->button);
  if ((env->mouse_button_flags() & (~input_flags)) == env->mouse_button_flags())
    return RespondNow(NoArguments());

  int64_t display_id = ash::Shell::Get()->cursor_manager()->GetDisplay().id();
  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  if (!root_window)
    return RespondNow(Error("Failed to find the root window"));

  gfx::PointF location_in_host(env->last_mouse_location().x(),
                               env->last_mouse_location().y());
  wm::ConvertPointFromScreen(root_window, &location_in_host);
  ConvertPointToHost(root_window, &location_in_host);

  event_generator_ = std::make_unique<EventGenerator>(
      root_window->GetHost(),
      base::BindOnce(&AutotestPrivateMouseReleaseFunction::Respond, this,
                     NoArguments()));
  event_generator_->ScheduleMouseEvent(ui::ET_MOUSE_RELEASED, location_in_host,
                                       input_flags);
  event_generator_->Run();

  return RespondLater();
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateMouseMoveFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateMouseMoveFunction::AutotestPrivateMouseMoveFunction() = default;
AutotestPrivateMouseMoveFunction::~AutotestPrivateMouseMoveFunction() = default;
ExtensionFunction::ResponseAction AutotestPrivateMouseMoveFunction::Run() {
  std::unique_ptr<api::autotest_private::MouseMove::Params> params(
      api::autotest_private::MouseMove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t display_id = ash::Shell::Get()->cursor_manager()->GetDisplay().id();
  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  if (!root_window)
    return RespondNow(Error("Failed to find the root window"));

  const gfx::PointF location_in_root(params->location.x, params->location.y);
  gfx::PointF location_in_screen = location_in_root;
  wm::ConvertPointToScreen(root_window, &location_in_screen);
  auto* env = aura::Env::GetInstance();
  const gfx::Point last_mouse_location(env->last_mouse_location());
  if (last_mouse_location == gfx::ToFlooredPoint(location_in_screen))
    return RespondNow(NoArguments());

  gfx::PointF location_in_host = location_in_root;
  ConvertPointToHost(root_window, &location_in_host);

  event_generator_ = std::make_unique<EventGenerator>(
      root_window->GetHost(),
      base::BindOnce(&AutotestPrivateMouseMoveFunction::Respond, this,
                     NoArguments()));
  gfx::PointF start_in_host(last_mouse_location.x(), last_mouse_location.y());
  wm::ConvertPointFromScreen(root_window, &start_in_host);
  ConvertPointToHost(root_window, &start_in_host);

  int64_t steps = std::max(
      base::ClampFloor<int64_t>(params->duration_in_ms /
                                event_generator_->interval().InMillisecondsF()),
      static_cast<int64_t>(1));
  int flags = env->mouse_button_flags();
  ui::EventType type = (flags == 0) ? ui::ET_MOUSE_MOVED : ui::ET_MOUSE_DRAGGED;
  for (int64_t i = 1; i <= steps; ++i) {
    double progress = static_cast<double>(i) / static_cast<double>(steps);
    gfx::PointF point(gfx::Tween::FloatValueBetween(progress, start_in_host.x(),
                                                    location_in_host.x()),
                      gfx::Tween::FloatValueBetween(progress, start_in_host.y(),
                                                    location_in_host.y()));
    event_generator_->ScheduleMouseEvent(type, point, flags);
  }
  event_generator_->Run();
  return RespondLater();
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetMetricsEnabledFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetMetricsEnabledFunction::
    AutotestPrivateSetMetricsEnabledFunction() = default;

AutotestPrivateSetMetricsEnabledFunction::
    ~AutotestPrivateSetMetricsEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetMetricsEnabledFunction::Run() {
  std::unique_ptr<api::autotest_private::SetMetricsEnabled::Params> params(
      api::autotest_private::SetMetricsEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  VLOG(1) << "AutotestPrivateSetMetricsEnabledFunction " << std::boolalpha
          << params->enabled;

  target_value_ = params->enabled;

  Profile* profile = Profile::FromBrowserContext(browser_context());

  bool value;
  if (ash::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                           &value) &&
      value == target_value_) {
    VLOG(1) << "Value at target; returning early";
    return RespondNow(NoArguments());
  }

  ash::StatsReportingController* stats_reporting_controller =
      ash::StatsReportingController::Get();

  stats_reporting_controller->SetOnDeviceSettingsStoredCallBack(base::BindOnce(
      &AutotestPrivateSetMetricsEnabledFunction::OnDeviceSettingsStored, this));

  // Set the preference to indicate metrics are enabled/disabled.
  stats_reporting_controller->SetEnabled(profile, target_value_);

  return RespondLater();
}

void AutotestPrivateSetMetricsEnabledFunction::OnDeviceSettingsStored() {
  bool actual;
  if (!ash::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &actual)) {
    NOTREACHED() << "AutotestPrivateSetMetricsEnabledFunction: "
                 << "kStatsReportingPref should be set";
    Respond(Error(base::StrCat(
        {"Failed to set metrics consent: ", chromeos::kStatsReportingPref,
         " is not set."})));
    return;
  }
  VLOG(1) << "AutotestPrivateSetMetricsEnabledFunction: actual: "
          << std::boolalpha << actual << " and expected: " << std::boolalpha
          << target_value_;
  if (actual == target_value_) {
    Respond(NoArguments());
  } else {
    Respond(Error(base::StrCat(
        {"Failed to set metrics consent: ", chromeos::kStatsReportingPref,
         " has wrong value."})));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStartTracingFunction
///////////////////////////////////////////////////////////////////////////////
AutotestPrivateStartTracingFunction::AutotestPrivateStartTracingFunction() =
    default;
AutotestPrivateStartTracingFunction::~AutotestPrivateStartTracingFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateStartTracingFunction::Run() {
  std::unique_ptr<api::autotest_private::StartTracing::Params> params(
      api::autotest_private::StartTracing::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  std::unique_ptr<base::Value> config_value = params->config.ToValue();
  base::trace_event::TraceConfig config(*config_value.get());

  if (!content::TracingController::GetInstance()->StartTracing(
          config,
          base::BindOnce(&AutotestPrivateStartTracingFunction::OnStartTracing,
                         this))) {
    return RespondNow(Error("Failed to start tracing"));
  }

  return RespondLater();
}

void AutotestPrivateStartTracingFunction::OnStartTracing() {
  Respond(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStopTracingFunction
///////////////////////////////////////////////////////////////////////////////
AutotestPrivateStopTracingFunction::AutotestPrivateStopTracingFunction() =
    default;
AutotestPrivateStopTracingFunction::~AutotestPrivateStopTracingFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateStopTracingFunction::Run() {
  if (!content::TracingController::GetInstance()->StopTracing(
          content::TracingController::CreateStringEndpoint(base::BindOnce(
              &AutotestPrivateStopTracingFunction::OnTracingComplete, this)))) {
    return RespondNow(Error("Failed to stop tracing"));
  }
  return RespondLater();
}

void AutotestPrivateStopTracingFunction::OnTracingComplete(
    std::unique_ptr<std::string> trace) {
  base::Value value(*trace.get());
  Respond(OneArgument(std::move(value)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetArcTouchModeFunction
///////////////////////////////////////////////////////////////////////////////
AutotestPrivateSetArcTouchModeFunction::
    AutotestPrivateSetArcTouchModeFunction() = default;
AutotestPrivateSetArcTouchModeFunction::
    ~AutotestPrivateSetArcTouchModeFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetArcTouchModeFunction::Run() {
  std::unique_ptr<api::autotest_private::SetArcTouchMode::Params> params(
      api::autotest_private::SetArcTouchMode::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetArcTouchModeFunction " << params->enabled;

  if (!arc::SetTouchMode(params->enabled))
    return RespondNow(Error("Could not send intent to ARC."));

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutotestPrivatePinShelfIconFunction
////////////////////////////////////////////////////////////////////////////////
AutotestPrivatePinShelfIconFunction::AutotestPrivatePinShelfIconFunction() =
    default;
AutotestPrivatePinShelfIconFunction::~AutotestPrivatePinShelfIconFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivatePinShelfIconFunction::Run() {
  std::unique_ptr<api::autotest_private::PinShelfIcon::Params> params(
      api::autotest_private::PinShelfIcon::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivatePinShelfIconFunction " << params->app_id;

  ChromeLauncherController* const controller =
      ChromeLauncherController::instance();
  if (!controller)
    return RespondNow(Error("Controller not available"));

  controller->PinAppWithID(params->app_id);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetScrollableShelfInfoForStateFunction
////////////////////////////////////////////////////////////////////////////////
AutotestPrivateGetScrollableShelfInfoForStateFunction::
    AutotestPrivateGetScrollableShelfInfoForStateFunction() = default;
AutotestPrivateGetScrollableShelfInfoForStateFunction::
    ~AutotestPrivateGetScrollableShelfInfoForStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetScrollableShelfInfoForStateFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetScrollableShelfInfoForStateFunction";
  std::unique_ptr<api::autotest_private::GetScrollableShelfInfoForState::Params>
      params(
          api::autotest_private::GetScrollableShelfInfoForState::Params::Create(
              *args_));

  ash::ShelfTestApi shelf_test_api;

  ash::ShelfState state;

  if (params->state.scroll_distance)
    state.scroll_distance = *params->state.scroll_distance;

  ash::ScrollableShelfInfo fetched_info =
      shelf_test_api.GetScrollableShelfInfoForState(state);

  api::autotest_private::ScrollableShelfInfo info;
  info.main_axis_offset = fetched_info.main_axis_offset;
  info.page_offset = fetched_info.page_offset;
  info.left_arrow_bounds = ToBoundsDictionary(fetched_info.left_arrow_bounds);
  info.right_arrow_bounds = ToBoundsDictionary(fetched_info.right_arrow_bounds);
  info.is_animating = fetched_info.is_animating;
  info.is_overflow = fetched_info.is_overflow;

  if (params->state.scroll_distance) {
    info.target_main_axis_offset =
        std::make_unique<double>(fetched_info.target_main_axis_offset);
  }

  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(info.ToValue())));
}

////////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetShelfUIInfoForStateFunction
////////////////////////////////////////////////////////////////////////////////
AutotestPrivateGetShelfUIInfoForStateFunction::
    AutotestPrivateGetShelfUIInfoForStateFunction() = default;
AutotestPrivateGetShelfUIInfoForStateFunction::
    ~AutotestPrivateGetShelfUIInfoForStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetShelfUIInfoForStateFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetShelfUIInfoForStateFunction";
  std::unique_ptr<api::autotest_private::GetShelfUIInfoForState::Params> params(
      api::autotest_private::GetShelfUIInfoForState::Params::Create(*args_));

  ash::ShelfState state;
  if (params->state.scroll_distance)
    state.scroll_distance = *params->state.scroll_distance;

  api::autotest_private::ShelfUIInfo shelf_ui_info;
  ash::ShelfTestApi shelf_test_api;

  // Fetch scrollable shelf ui information.
  {
    ash::ScrollableShelfInfo fetched_info =
        shelf_test_api.GetScrollableShelfInfoForState(state);

    api::autotest_private::ScrollableShelfInfo scrollable_shelf_ui_info;
    scrollable_shelf_ui_info.main_axis_offset = fetched_info.main_axis_offset;
    scrollable_shelf_ui_info.page_offset = fetched_info.page_offset;
    scrollable_shelf_ui_info.left_arrow_bounds =
        ToBoundsDictionary(fetched_info.left_arrow_bounds);
    scrollable_shelf_ui_info.right_arrow_bounds =
        ToBoundsDictionary(fetched_info.right_arrow_bounds);
    scrollable_shelf_ui_info.is_animating = fetched_info.is_animating;
    scrollable_shelf_ui_info.is_overflow = fetched_info.is_overflow;
    scrollable_shelf_ui_info.icons_bounds_in_screen =
        ToBoundsDictionaryList(fetched_info.icons_bounds_in_screen);
    scrollable_shelf_ui_info.is_shelf_widget_animating =
        fetched_info.is_shelf_widget_animating;

    if (state.scroll_distance) {
      scrollable_shelf_ui_info.target_main_axis_offset =
          std::make_unique<double>(fetched_info.target_main_axis_offset);
    }

    shelf_ui_info.scrollable_shelf_info = std::move(scrollable_shelf_ui_info);
  }

  // Fetch hotseat ui information.
  {
    ash::HotseatInfo hotseat_info = shelf_test_api.GetHotseatInfo();
    api::autotest_private::HotseatSwipeDescriptor swipe_up_descriptor;
    swipe_up_descriptor.swipe_start_location =
        ToLocationDictionary(hotseat_info.swipe_up.swipe_start_location);
    swipe_up_descriptor.swipe_end_location =
        ToLocationDictionary(hotseat_info.swipe_up.swipe_end_location);

    api::autotest_private::HotseatInfo hotseat_ui_info;
    hotseat_ui_info.swipe_up = std::move(swipe_up_descriptor);
    hotseat_ui_info.is_animating = hotseat_info.is_animating;
    hotseat_ui_info.state = GetHotseatState(hotseat_info.hotseat_state);

    shelf_ui_info.hotseat_info = std::move(hotseat_ui_info);
  }

  return RespondNow(
      OneArgument(base::Value::FromUniquePtrValue(shelf_ui_info.ToValue())));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetWindowBoundsFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetWindowBoundsFunction::
    AutotestPrivateSetWindowBoundsFunction() = default;
AutotestPrivateSetWindowBoundsFunction::
    ~AutotestPrivateSetWindowBoundsFunction() = default;

namespace {

std::unique_ptr<base::DictionaryValue> BuildSetWindowBoundsResult(
    const gfx::Rect& bounds_in_display,
    int64_t display_id) {
  auto result = std::make_unique<base::DictionaryValue>();
  result->SetDictionary("bounds",
                        ToBoundsDictionary(bounds_in_display).ToValue());
  result->SetString("displayId", base::NumberToString(display_id));
  return result;
}

}  // namespace

ExtensionFunction::ResponseAction
AutotestPrivateSetWindowBoundsFunction::Run() {
  std::unique_ptr<api::autotest_private::SetWindowBounds::Params> params(
      api::autotest_private::SetWindowBounds::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  aura::Window* window = FindAppWindowById(params->id);
  if (!window) {
    return RespondNow(Error(
        base::StringPrintf("No app window was found : id=%d", params->id)));
  }

  auto* state = ash::WindowState::Get(window);
  if (!state || chromeos::ToWindowShowState(state->GetStateType()) !=
                    ui::SHOW_STATE_NORMAL) {
    return RespondNow(
        Error("Cannot set bounds of window not in normal show state."));
  }

  int64_t display_id;
  if (!base::StringToInt64(params->display_id, &display_id)) {
    return RespondNow(Error(base::StrCat(
        {"Invalid display_id; expected string with numbers only, got ",
         params->display_id})));
  }

  display::Display display;
  display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id, &display);
  if (!display.is_valid()) {
    return RespondNow(
        Error("Given display ID does not correspond to a valid display"));
  }

  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  if (!root_window)
    return RespondNow(Error("Failed to find the root window"));

  gfx::Rect to_bounds = ToRect(params->bounds);

  if (window->GetBoundsInRootWindow() == to_bounds &&
      state->GetDisplay().id() == display_id) {
    return RespondNow(OneArgument(base::Value::FromUniquePtrValue(
        BuildSetWindowBoundsResult(to_bounds, display_id))));
  }

  window_bounds_observer_ = std::make_unique<WindowBoundsChangeObserver>(
      window, to_bounds, display_id,
      base::BindOnce(
          &AutotestPrivateSetWindowBoundsFunction::WindowBoundsChanged, this));

  ::wm::ConvertRectToScreen(root_window, &to_bounds);
  window->SetBoundsInScreen(to_bounds, display);

  return RespondLater();
}

void AutotestPrivateSetWindowBoundsFunction::WindowBoundsChanged(
    const gfx::Rect& bounds_in_display,
    int64_t display_id,
    bool success) {
  if (!success) {
    Respond(Error(
        "The app window was destroyed while waiting for bounds to change!"));
  } else {
    Respond(OneArgument(base::Value::FromUniquePtrValue(
        BuildSetWindowBoundsResult(bounds_in_display, display_id))));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStartSmoothnessTrackingFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateStartSmoothnessTrackingFunction::
    ~AutotestPrivateStartSmoothnessTrackingFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateStartSmoothnessTrackingFunction::Run() {
  auto params(
      api::autotest_private::StartSmoothnessTracking::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t display_id;
  if (!GetDisplayIdFromOptionalArg(params->display_id, &display_id)) {
    return RespondNow(
        Error(base::StrCat({"Invalid display id: ", *params->display_id})));
  }

  auto* infos = GetDisplaySmoothnessTrackerInfos();
  if (infos->find(display_id) != infos->end()) {
    return RespondNow(
        Error(base::StrCat({"Smoothness already tracked for display: ",
                            base::NumberToString(display_id)})));
  }

  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  if (!root_window) {
    return RespondNow(Error(base::StrCat(
        {"Invalid display_id; no root window found for the display id ",
         base::NumberToString(display_id)})));
  }

  auto tracker =
      root_window->layer()->GetCompositor()->RequestNewThroughputTracker();
  tracker.Start(base::BindOnce(&ForwardFrameRateDataAndReset, display_id));
  (*infos)[display_id].tracker = std::move(tracker);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStopSmoothnessTrackingFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateStopSmoothnessTrackingFunction::
    ~AutotestPrivateStopSmoothnessTrackingFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateStopSmoothnessTrackingFunction::Run() {
  auto params(
      api::autotest_private::StopSmoothnessTracking::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t display_id;
  if (!GetDisplayIdFromOptionalArg(params->display_id, &display_id)) {
    return RespondNow(
        Error(base::StrCat({"Invalid display id: ", *params->display_id})));
  }

  auto* infos = GetDisplaySmoothnessTrackerInfos();
  auto it = infos->find(display_id);
  if (it == infos->end()) {
    return RespondNow(
        Error(base::StrCat({"Smoothness is not tracked for display: ",
                            base::NumberToString(display_id)})));
  }

  it->second.callback = base::BindOnce(
      &AutotestPrivateStopSmoothnessTrackingFunction::OnReportData, this);
  it->second.tracker->Stop();

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutotestPrivateStopSmoothnessTrackingFunction::OnReportData(
    const cc::FrameSequenceMetrics::CustomReportData& data) {
  api::autotest_private::ThroughputTrackerAnimationData result_data;
  result_data.frames_expected = data.frames_expected;
  result_data.frames_produced = data.frames_produced;
  result_data.jank_count = data.jank_count;

  Respond(ArgumentList(
      api::autotest_private::StopSmoothnessTracking::Results::Create(
          result_data)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateWaitForAmbientPhotoAnimationFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateWaitForAmbientPhotoAnimationFunction::
    AutotestPrivateWaitForAmbientPhotoAnimationFunction() = default;

AutotestPrivateWaitForAmbientPhotoAnimationFunction::
    ~AutotestPrivateWaitForAmbientPhotoAnimationFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateWaitForAmbientPhotoAnimationFunction::Run() {
  std::unique_ptr<api::autotest_private::WaitForAmbientPhotoAnimation::Params>
      params(
          api::autotest_private::WaitForAmbientPhotoAnimation::Params::Create(
              *args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // Wait for photo transition animation completed in ambient mode.
  ash::AutotestAmbientApi().WaitForPhotoTransitionAnimationCompleted(
      params->num_completions,
      base::BindOnce(&AutotestPrivateWaitForAmbientPhotoAnimationFunction::
                         OnPhotoTransitionAnimationCompleted,
                     this));

  // Set up a timer to finish waiting after |timeout_s|.
  timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(params->timeout),
      base::BindOnce(
          &AutotestPrivateWaitForAmbientPhotoAnimationFunction::Timeout, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutotestPrivateWaitForAmbientPhotoAnimationFunction::
    OnPhotoTransitionAnimationCompleted() {
  if (did_respond())
    return;

  Respond(NoArguments());
}

void AutotestPrivateWaitForAmbientPhotoAnimationFunction::Timeout() {
  if (did_respond())
    return;

  Respond(Error("No enough animations completed before time out."));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateDisableSwitchAccessDialogFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateDisableSwitchAccessDialogFunction::
    AutotestPrivateDisableSwitchAccessDialogFunction() = default;

AutotestPrivateDisableSwitchAccessDialogFunction::
    ~AutotestPrivateDisableSwitchAccessDialogFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateDisableSwitchAccessDialogFunction::Run() {
  auto* accessibility_controller = ash::AccessibilityController::Get();
  accessibility_controller
      ->DisableSwitchAccessDisableConfirmationDialogTesting();
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateDisableAutomationFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateDisableAutomationFunction::
    AutotestPrivateDisableAutomationFunction() = default;

AutotestPrivateDisableAutomationFunction::
    ~AutotestPrivateDisableAutomationFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateDisableAutomationFunction::Run() {
  AutomationManagerAura::GetInstance()->Disable();
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStartThroughputTrackerDataCollectionFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateStartThroughputTrackerDataCollectionFunction::
    AutotestPrivateStartThroughputTrackerDataCollectionFunction() = default;

AutotestPrivateStartThroughputTrackerDataCollectionFunction::
    ~AutotestPrivateStartThroughputTrackerDataCollectionFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateStartThroughputTrackerDataCollectionFunction::Run() {
  ash::metrics_util::StartDataCollection();
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStopThroughputTrackerDataCollectionFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateStopThroughputTrackerDataCollectionFunction::
    AutotestPrivateStopThroughputTrackerDataCollectionFunction() = default;

AutotestPrivateStopThroughputTrackerDataCollectionFunction::
    ~AutotestPrivateStopThroughputTrackerDataCollectionFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateStopThroughputTrackerDataCollectionFunction::Run() {
  auto collected_data = ash::metrics_util::StopDataCollection();
  std::vector<api::autotest_private::ThroughputTrackerAnimationData>
      result_data;
  for (const auto& data : collected_data) {
    api::autotest_private::ThroughputTrackerAnimationData animation_data;
    animation_data.frames_expected = data.frames_expected;
    animation_data.frames_produced = data.frames_produced;
    animation_data.jank_count = data.jank_count;
    result_data.emplace_back(std::move(animation_data));
  }

  return RespondNow(
      ArgumentList(api::autotest_private::StopThroughputTrackerDataCollection::
                       Results::Create(result_data)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateAPI
///////////////////////////////////////////////////////////////////////////////

static base::LazyInstance<BrowserContextKeyedAPIFactory<AutotestPrivateAPI>>::
    DestructorAtExit g_autotest_private_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<AutotestPrivateAPI>*
AutotestPrivateAPI::GetFactoryInstance() {
  return g_autotest_private_api_factory.Pointer();
}

template <>
KeyedService*
BrowserContextKeyedAPIFactory<AutotestPrivateAPI>::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AutotestPrivateAPI(context);
}

AutotestPrivateAPI::AutotestPrivateAPI(content::BrowserContext* context)
    : clipboard_observer_(this), browser_context_(context), test_mode_(false) {
  clipboard_observer_.Add(ui::ClipboardMonitor::GetInstance());
}

AutotestPrivateAPI::~AutotestPrivateAPI() = default;

void AutotestPrivateAPI::OnClipboardDataChanged() {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;

  std::unique_ptr<base::ListValue> event_args =
      std::make_unique<base::ListValue>();
  std::unique_ptr<Event> event(
      new Event(events::AUTOTESTPRIVATE_ON_CLIPBOARD_DATA_CHANGED,
                api::autotest_private::OnClipboardDataChanged::kEventName,
                std::move(event_args)));
  event_router->BroadcastEvent(std::move(event));
}

}  // namespace extensions
