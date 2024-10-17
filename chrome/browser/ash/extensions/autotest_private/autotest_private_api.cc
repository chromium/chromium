// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/extensions/autotest_private/autotest_private_api.h"

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string_view>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_public_test_util.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/mojom/system_ui.mojom-shared.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/system_ui/arc_system_ui_bridge.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/metrics/login_unlock_throughput_recorder.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/autotest_ambient_api.h"
#include "ash/public/cpp/autotest_desks_api.h"
#include "ash/public/cpp/autotest_private_api_utils.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_prefs.h"
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
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/wm_event.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/ash/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/borealis/borealis_installer.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_types.mojom.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/crosapi/automation_ash.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crostini/crostini_export_import.h"
#include "chrome/browser/ash/crostini/crostini_export_import_factory.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_installer.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/power/ml/smart_dim/ml_agent.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/component_updater/smart_dim_component_installer.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/default_pinned_apps/default_pinned_apps.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/bruschetta/bruschetta_installer_view.h"
#include "chrome/browser/ui/views/crostini/crostini_uninstaller_view.h"
#include "chrome/browser/ui/views/plugin_vm/plugin_vm_installer_view.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer_dialog.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer_ui.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/extensions/api/autotest_private.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/metrics/login_event_recorder.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/assistant/assistant_manager_service_impl.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/caption_button_model.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "components/device_event_log/device_event_log.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/update_client/update_client_errors.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/variations/pref_names.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
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
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-shared.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/text_input_method.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/compositor/throughput_tracker_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/caption_button_types.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

using extensions::mojom::ManifestLocation;

namespace extensions {
namespace {

using chromeos::PrinterClass;

// Features used for testing `isFeatureEnabled`.
BASE_FEATURE(kEnabledFeatureForTest,
             "EnabledFeatureForTest",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kDisabledFeatureForTest,
             "DisabledFeatureForTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr char kCrostiniNotAvailableForCurrentUserError[] =
    "Crostini is not available for the current user";

NOINLINE int AccessArray(const int arr[], const int* index) {
  return arr[*index];
}

base::Value::List GetHostPermissions(const Extension* ext,
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

  base::Value::List permissions;
  for (const auto& perm : *pattern_set) {
    permissions.Append(perm.GetAsString());
  }

  return permissions;
}

base::Value::List GetAPIPermissions(const Extension* ext) {
  base::Value::List permissions;
  std::set<std::string> perm_list =
      ext->permissions_data()->active_permissions().GetAPIsAsStrings();
  for (const auto& perm : perm_list) {
    permissions.Append(perm);
  }
  return permissions;
}

bool IsTestMode(content::BrowserContext* context) {
  return AutotestPrivateAPI::GetFactoryInstance()->Get(context)->test_mode();
}

std::string ConvertToString(message_center::NotificationType type) {
  switch (type) {
    case message_center::NOTIFICATION_TYPE_SIMPLE:
    case message_center::DEPRECATED_NOTIFICATION_TYPE_BASE_FORMAT:
      return "simple";
    case message_center::NOTIFICATION_TYPE_IMAGE:
      return "image";
    case message_center::NOTIFICATION_TYPE_MULTIPLE:
      return "multiple";
    case message_center::NOTIFICATION_TYPE_PROGRESS:
      return "progress";
    case message_center::NOTIFICATION_TYPE_CUSTOM:
      return "custom";
    case message_center::NOTIFICATION_TYPE_CONVERSATION:
      return "conversation";
  }
  return "unknown";
}

base::Value::Dict MakeDictionaryFromNotification(
    const message_center::Notification& notification) {
  return base::Value::Dict()
      .Set("id", notification.id())
      .Set("type", ConvertToString(notification.type()))
      .Set("title", notification.title())
      .Set("message", notification.message())
      .Set("priority", notification.priority())
      .Set("progress", notification.progress());
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
      return api::autotest_private::ShelfItemType::kPinnedApp;
    case ash::TYPE_BROWSER_SHORTCUT:
      return api::autotest_private::ShelfItemType::kBrowserShortcut;
    case ash::TYPE_APP:
      return api::autotest_private::ShelfItemType::kApp;
    case ash::TYPE_UNPINNED_BROWSER_SHORTCUT:
      return api::autotest_private::ShelfItemType::kUnpinnedBrowserShortcut;
    case ash::TYPE_DIALOG:
      return api::autotest_private::ShelfItemType::kDialog;
    case ash::TYPE_UNDEFINED:
      return api::autotest_private::ShelfItemType::kNone;
  }
  NOTREACHED_IN_MIGRATION();
  return api::autotest_private::ShelfItemType::kNone;
}

api::autotest_private::ShelfItemStatus GetShelfItemStatus(
    ash::ShelfItemStatus status) {
  switch (status) {
    case ash::STATUS_CLOSED:
      return api::autotest_private::ShelfItemStatus::kClosed;
    case ash::STATUS_RUNNING:
      return api::autotest_private::ShelfItemStatus::kRunning;
    case ash::STATUS_ATTENTION:
      return api::autotest_private::ShelfItemStatus::kAttention;
  }
  NOTREACHED_IN_MIGRATION();
  return api::autotest_private::ShelfItemStatus::kNone;
}

api::autotest_private::AppType GetAppType(apps::AppType type) {
  switch (type) {
    case apps::AppType::kArc:
      return api::autotest_private::AppType::kArc;
    case apps::AppType::kBuiltIn:
      return api::autotest_private::AppType::kBuiltIn;
    case apps::AppType::kCrostini:
      return api::autotest_private::AppType::kCrostini;
    case apps::AppType::kChromeApp:
    case apps::AppType::kExtension:
      return api::autotest_private::AppType::kExtension;
    case apps::AppType::kPluginVm:
      return api::autotest_private::AppType::kPluginVm;
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
      return api::autotest_private::AppType::kWeb;
    case apps::AppType::kUnknown:
      return api::autotest_private::AppType::kNone;
    case apps::AppType::kStandaloneBrowser:
      return api::autotest_private::AppType::kStandaloneBrowser;
    case apps::AppType::kRemote:
      return api::autotest_private::AppType::kRemote;
    case apps::AppType::kBorealis:
      return api::autotest_private::AppType::kBorealis;
    case apps::AppType::kBruschetta:
      return api::autotest_private::AppType::kBruschetta;
    case apps::AppType::kStandaloneBrowserExtension:
      return api::autotest_private::AppType::kNone;
    case apps::AppType::kStandaloneBrowserChromeApp:
      return api::autotest_private::AppType::kExtension;
  }
  NOTREACHED_IN_MIGRATION();
  return api::autotest_private::AppType::kNone;
}

api::autotest_private::AppInstallSource GetAppInstallSource(
    apps::InstallReason install_reason) {
  switch (install_reason) {
    case apps::InstallReason::kUnknown:
      return api::autotest_private::AppInstallSource::kUnknown;
    case apps::InstallReason::kSystem:
      return api::autotest_private::AppInstallSource::kSystem;
    case apps::InstallReason::kPolicy:
      return api::autotest_private::AppInstallSource::kPolicy;
    case apps::InstallReason::kOem:
      return api::autotest_private::AppInstallSource::kOem;
    case apps::InstallReason::kDefault:
      return api::autotest_private::AppInstallSource::kDefault;
    case apps::InstallReason::kSync:
      return api::autotest_private::AppInstallSource::kSync;
    case apps::InstallReason::kUser:
      return api::autotest_private::AppInstallSource::kUser;
    case apps::InstallReason::kSubApp:
      return api::autotest_private::AppInstallSource::kSubApp;
    case apps::InstallReason::kKiosk:
      return api::autotest_private::AppInstallSource::kKiosk;
    case apps::InstallReason::kCommandLine:
      return api::autotest_private::AppInstallSource::kCommandLine;
  }
  NOTREACHED_IN_MIGRATION();
  return api::autotest_private::AppInstallSource::kNone;
}

api::autotest_private::AppWindowType GetAppWindowType(chromeos::AppType type) {
  switch (type) {
    case chromeos::AppType::ARC_APP:
      return api::autotest_private::AppWindowType::kArcApp;
    case chromeos::AppType::SYSTEM_APP:
      return api::autotest_private::AppWindowType::kSystemApp;
    case chromeos::AppType::CROSTINI_APP:
      return api::autotest_private::AppWindowType::kCrostiniApp;
    case chromeos::AppType::CHROME_APP:
      return api::autotest_private::AppWindowType::kExtensionApp;
    case chromeos::AppType::BROWSER:
      return api::autotest_private::AppWindowType::kBrowser;
    case chromeos::AppType::LACROS:
      return api::autotest_private::AppWindowType::kLacros;
    case chromeos::AppType::NON_APP:
      return api::autotest_private::AppWindowType::kNone;
      // TODO(oshima): Investigate if we want to have "extension" type.
  }
  NOTREACHED_IN_MIGRATION();
  return api::autotest_private::AppWindowType::kNone;
}

api::autotest_private::AppReadiness GetAppReadiness(apps::Readiness readiness) {
  switch (readiness) {
    case apps::Readiness::kReady:
      return api::autotest_private::AppReadiness::kReady;
    case apps::Readiness::kDisabledByBlocklist:
      return api::autotest_private::AppReadiness::kDisabledByBlacklist;
    case apps::Readiness::kDisabledByPolicy:
      return api::autotest_private::AppReadiness::kDisabledByPolicy;
    case apps::Readiness::kDisabledByUser:
      return api::autotest_private::AppReadiness::kDisabledByUser;
    case apps::Readiness::kTerminated:
      return api::autotest_private::AppReadiness::kTerminated;
    case apps::Readiness::kUninstalledByUser:
      return api::autotest_private::AppReadiness::kUninstalledByUser;
    case apps::Readiness::kRemoved:
      return api::autotest_private::AppReadiness::kRemoved;
    case apps::Readiness::kUninstalledByNonUser:
      return api::autotest_private::AppReadiness::kUninstalledByMigration;
    case apps::Readiness::kDisabledByLocalSettings:
      return api::autotest_private::AppReadiness::kDisabledByLocalSettings;
    case apps::Readiness::kUnknown:
      return api::autotest_private::AppReadiness::kNone;
  }
  NOTREACHED_IN_MIGRATION();
  return api::autotest_private::AppReadiness::kNone;
}

api::autotest_private::HotseatState GetHotseatState(
    ash::HotseatState hotseat_state) {
  switch (hotseat_state) {
    case ash::HotseatState::kNone:
      return api::autotest_private::HotseatState::kNone;
    case ash::HotseatState::kHidden:
      return api::autotest_private::HotseatState::kHidden;
    case ash::HotseatState::kShownClamshell:
      return api::autotest_private::HotseatState::kShownClamShell;
    case ash::HotseatState::kShownHomeLauncher:
      return api::autotest_private::HotseatState::kShownHomeLauncher;
    case ash::HotseatState::kExtended:
      return api::autotest_private::HotseatState::kExtended;
  }

  NOTREACHED_IN_MIGRATION();
}

api::autotest_private::WakefulnessMode GetWakefulnessMode(
    arc::mojom::WakefulnessMode mode) {
  switch (mode) {
    case arc::mojom::WakefulnessMode::ASLEEP:
      return api::autotest_private::WakefulnessMode::kAsleep;
    case arc::mojom::WakefulnessMode::AWAKE:
      return api::autotest_private::WakefulnessMode::kAwake;
    case arc::mojom::WakefulnessMode::DOZING:
      return api::autotest_private::WakefulnessMode::kDozing;
    case arc::mojom::WakefulnessMode::DREAMING:
      return api::autotest_private::WakefulnessMode::kDreaming;
    case arc::mojom::WakefulnessMode::UNKNOWN:
      return api::autotest_private::WakefulnessMode::kUnknown;
  }

  NOTREACHED_IN_MIGRATION();
}

// Helper function to set allowed user pref based on |pref_name| with any
// specific pref validations. Returns error messages if any.
std::string SetAllowedPref(Profile* profile,
                           const std::string& pref_name,
                           const base::Value& value) {
  // Special case for the preference that is stored in the "Local State"
  // profile.
  if (pref_name == prefs::kEnableAdbSideloadingRequested) {
    DCHECK(value.is_bool());
    g_browser_process->local_state()->Set(pref_name, value);
    return std::string();
  }
  if (pref_name == variations::prefs::kVariationsCompressedSeed ||
      pref_name == variations::prefs::kVariationsSeedSignature) {
    DCHECK(value.is_string());
    g_browser_process->local_state()->Set(pref_name, value);
    return std::string();
  }

  if (pref_name == ash::assistant::prefs::kAssistantEnabled) {
    if (!value.is_bool()) {
      return "Invalid value type.";
    }
    // Validate the Assistant service allowed state.
    ash::assistant::AssistantAllowedState allowed_state =
        assistant::IsAssistantAllowedForProfile(profile);
    if (allowed_state != ash::assistant::AssistantAllowedState::ALLOWED) {
      return base::StringPrintf("Assistant not allowed - state: %d",
                                allowed_state);
    }
  } else if (pref_name == ash::assistant::prefs::kAssistantConsentStatus) {
    if (!value.is_int()) {
      return "Invalid value type.";
    }
    if (!profile->GetPrefs()->GetBoolean(
            ash::assistant::prefs::kAssistantEnabled)) {
      return "Unable to set the pref because Assistant has not been enabled.";
    }
  } else if (pref_name == ash::assistant::prefs::kAssistantContextEnabled ||
             pref_name == ash::assistant::prefs::kAssistantHotwordEnabled) {
    if (!value.is_bool()) {
      return "Invalid value type.";
    }
    // Assistant service must be enabled first for those prefs to take effect.
    if (!profile->GetPrefs()->GetBoolean(
            ash::assistant::prefs::kAssistantEnabled)) {
      return std::string(
          "Unable to set the pref because Assistant has not been enabled.");
    }
  } else if (pref_name ==
             ash::prefs::kAssistantNumSessionsWhereOnboardingShown) {
    if (!value.is_int()) {
      return "Invalid value type.";
    }
  } else if (pref_name == ash::prefs::kAccessibilitySpokenFeedbackEnabled) {
    DCHECK(value.is_bool());
  } else if (pref_name == ash::prefs::kAccessibilityVirtualKeyboardEnabled) {
    DCHECK(value.is_bool());
  } else if (pref_name == prefs::kDocumentScanAPITrustedExtensions) {
    DCHECK(value.is_list());
  } else if (pref_name == ash::prefs::kEnableAutoScreenLock) {
    DCHECK(value.is_bool());
  } else if (pref_name == prefs::kLanguagePreloadEngines) {
    DCHECK(value.is_string());
  } else if (pref_name == plugin_vm::prefs::kPluginVmCameraAllowed) {
    DCHECK(value.is_bool());
  } else if (pref_name == plugin_vm::prefs::kPluginVmMicAllowed) {
    DCHECK(value.is_bool());
  } else if (pref_name == plugin_vm::prefs::kPluginVmDataCollectionAllowed) {
    DCHECK(value.is_bool());
  } else if (pref_name == prefs::kPrintingAPIExtensionsAllowlist) {
    DCHECK(value.is_list());
  } else if (pref_name == quick_answers::prefs::kQuickAnswersEnabled) {
    DCHECK(value.is_bool());
  } else if (pref_name ==
             quick_answers::prefs::kQuickAnswersDefinitionEnabled) {
    DCHECK(value.is_bool());
  } else if (pref_name ==
             quick_answers::prefs::kQuickAnswersTranslationEnabled) {
    DCHECK(value.is_bool());
  } else if (pref_name ==
             quick_answers::prefs::kQuickAnswersUnitConversionEnabled) {
    DCHECK(value.is_bool());
  } else if (pref_name == quick_answers::prefs::kQuickAnswersConsentStatus) {
    DCHECK(value.is_int());
  } else if (pref_name == arc::prefs::kArcShowResizeLockSplashScreenLimits) {
    DCHECK(value.is_int());
  } else {
    return "The pref " + pref_name + " is not allowed.";
  }

  // Set value for the specified user pref after validation.
  profile->GetPrefs()->Set(pref_name, value);

  return std::string();
}

// Helper function to clear allowed user pref based on |pref_name|. Returns
// true on success or false if |pref_name| is not in the allowlist.
bool ClearAllowedPref(Profile* profile, const std::string& pref_name) {
  if (pref_name != ash::ambient::prefs::kAmbientUiSettings) {
    return false;
  }
  profile->GetPrefs()->ClearPref(pref_name);
  return true;
}

// Returns the ARC app window that associates with |package_name|. Note there
// might be more than 1 windows that have the same package name. This function
// just returns the first window it finds.
aura::Window* GetArcAppWindow(const std::string& package_name) {
  for (auto* window : ChromeShelfController::instance()->GetArcWindows()) {
    std::string* pkg_name = window->GetProperty(ash::kArcPackageNameKey);
    if (pkg_name && *pkg_name == package_name) {
      return window;
    }
  }
  return nullptr;
}

// Gets expected window state type according to |event_type|.
chromeos::WindowStateType GetExpectedWindowState(
    api::autotest_private::WMEventType event_type) {
  switch (event_type) {
    case api::autotest_private::WMEventType::kWmeventNormal:
      return chromeos::WindowStateType::kNormal;
    case api::autotest_private::WMEventType::kWmeventMaximize:
      return chromeos::WindowStateType::kMaximized;
    case api::autotest_private::WMEventType::kWmeventMinimize:
      return chromeos::WindowStateType::kMinimized;
    case api::autotest_private::WMEventType::kWmeventFullscreen:
      return chromeos::WindowStateType::kFullscreen;
    case api::autotest_private::WMEventType::kWmeventSnapPrimary:
      return chromeos::WindowStateType::kPrimarySnapped;
    case api::autotest_private::WMEventType::kWmeventSnapSecondary:
      return chromeos::WindowStateType::kSecondarySnapped;
    case api::autotest_private::WMEventType::kWmeventFloat:
      return chromeos::WindowStateType::kFloated;
    default:
      NOTREACHED_IN_MIGRATION();
      return chromeos::WindowStateType::kNormal;
  }
}

ash::WMEventType ToWMEventType(api::autotest_private::WMEventType event_type) {
  switch (event_type) {
    case api::autotest_private::WMEventType::kWmeventNormal:
      return ash::WMEventType::WM_EVENT_NORMAL;
    case api::autotest_private::WMEventType::kWmeventMaximize:
      return ash::WMEventType::WM_EVENT_MAXIMIZE;
    case api::autotest_private::WMEventType::kWmeventMinimize:
      return ash::WMEventType::WM_EVENT_MINIMIZE;
    case api::autotest_private::WMEventType::kWmeventFullscreen:
      return ash::WMEventType::WM_EVENT_FULLSCREEN;
    case api::autotest_private::WMEventType::kWmeventSnapPrimary:
      return ash::WMEventType::WM_EVENT_SNAP_PRIMARY;
    case api::autotest_private::WMEventType::kWmeventSnapSecondary:
      return ash::WMEventType::WM_EVENT_SNAP_SECONDARY;
    case api::autotest_private::WMEventType::kWmeventFloat:
      return ash::WMEventType::WM_EVENT_FLOAT;
    default:
      NOTREACHED_IN_MIGRATION();
      return ash::WMEventType::WM_EVENT_NORMAL;
  }
}

api::autotest_private::WindowStateType ToWindowStateType(
    chromeos::WindowStateType state_type) {
  switch (state_type) {
    // Consider adding DEFAULT type to idl.
    case chromeos::WindowStateType::kDefault:
    case chromeos::WindowStateType::kNormal:
      return api::autotest_private::WindowStateType::kNormal;
    case chromeos::WindowStateType::kMinimized:
      return api::autotest_private::WindowStateType::kMinimized;
    case chromeos::WindowStateType::kMaximized:
      return api::autotest_private::WindowStateType::kMaximized;
    case chromeos::WindowStateType::kFullscreen:
      return api::autotest_private::WindowStateType::kFullscreen;
    case chromeos::WindowStateType::kPrimarySnapped:
      return api::autotest_private::WindowStateType::kPrimarySnapped;
    case chromeos::WindowStateType::kSecondarySnapped:
      return api::autotest_private::WindowStateType::kSecondarySnapped;
    case chromeos::WindowStateType::kPinned:
      return api::autotest_private::WindowStateType::kPinned;
    case chromeos::WindowStateType::kTrustedPinned:
      return api::autotest_private::WindowStateType::kTrustedPinned;
    case chromeos::WindowStateType::kPip:
      return api::autotest_private::WindowStateType::kPip;
    case chromeos::WindowStateType::kFloated:
      return api::autotest_private::WindowStateType::kFloated;
    default:
      NOTREACHED_IN_MIGRATION();
      return api::autotest_private::WindowStateType::kNone;
  }
}

std::string GetPngDataAsString(scoped_refptr<base::RefCountedMemory> png_data) {
  // Base64 encode the result so we can return it as a string.
  std::string base64_png(png_data->front(),
                        png_data->front() + png_data->size());
  return base::Base64Encode(base64_png);
}

display::Display::Rotation ToRotation(
    api::autotest_private::RotationType rotation) {
  switch (rotation) {
    case api::autotest_private::RotationType::kRotate0:
      return display::Display::ROTATE_0;
    case api::autotest_private::RotationType::kRotate90:
      return display::Display::ROTATE_90;
    case api::autotest_private::RotationType::kRotate180:
      return display::Display::ROTATE_180;
    case api::autotest_private::RotationType::kRotate270:
      return display::Display::ROTATE_270;
    case api::autotest_private::RotationType::kRotateAny:
    case api::autotest_private::RotationType::kNone:
      break;
  }
  NOTREACHED_IN_MIGRATION();
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
  for (const gfx::Rect& bounds : items_bounds) {
    bounds_list.push_back(ToBoundsDictionary(bounds));
  }
  return bounds_list;
}

api::autotest_private::Location ToLocationDictionary(const gfx::Point& point) {
  api::autotest_private::Location result;
  result.x = point.x();
  result.y = point.y();
  return result;
}

arc::mojom::ThemeStyleType ToThemeStyleType(
    const api::autotest_private::ThemeStyle& theme) {
  switch (theme) {
    case api::autotest_private::ThemeStyle::kTonalSpot:
      return arc::mojom::ThemeStyleType::TONAL_SPOT;
    case api::autotest_private::ThemeStyle::kVibrant:
      return arc::mojom::ThemeStyleType::VIBRANT;
    case api::autotest_private::ThemeStyle::kExpressive:
      return arc::mojom::ThemeStyleType::EXPRESSIVE;
    case api::autotest_private::ThemeStyle::kSpritz:
      return arc::mojom::ThemeStyleType::SPRITZ;
    case api::autotest_private::ThemeStyle::kRainbow:
      return arc::mojom::ThemeStyleType::RAINBOW;
    case api::autotest_private::ThemeStyle::kFruitSalad:
      return arc::mojom::ThemeStyleType::FRUIT_SALAD;
    default:
      return arc::mojom::ThemeStyleType::TONAL_SPOT;
  }
}

aura::Window* FindAppWindowById(const int64_t id) {
  auto list = ash::GetAppWindowList();
  auto iter = base::ranges::find(list, id, &aura::Window::GetId);
  if (iter == list.end()) {
    return nullptr;
  }
  return *iter;
}

// Returns the first available Browser that is not a web app.
Browser* GetFirstRegularBrowser() {
  const BrowserList* list = BrowserList::GetInstance();
  const web_app::AppBrowserController* (Browser::*app_controller)() const =
      &Browser::app_controller;
  auto iter = base::ranges::find(*list, nullptr, app_controller);
  if (iter == list->end()) {
    return nullptr;
  }
  return *iter;
}

ash::AppListViewState ToAppListViewState(
    api::autotest_private::LauncherStateType state) {
  switch (state) {
    case api::autotest_private::LauncherStateType::kClosed:
      return ash::AppListViewState::kClosed;
    case api::autotest_private::LauncherStateType::kFullscreenAllApps:
      return ash::AppListViewState::kFullscreenAllApps;
    case api::autotest_private::LauncherStateType::kFullscreenSearch:
      return ash::AppListViewState::kFullscreenSearch;
    case api::autotest_private::LauncherStateType::kNone:
      break;
  }
  return ash::AppListViewState::kClosed;
}

ash::OverviewAnimationState ToOverviewAnimationState(
    api::autotest_private::OverviewStateType state) {
  switch (state) {
    case api::autotest_private::OverviewStateType::kShown:
      return ash::OverviewAnimationState::kEnterAnimationComplete;
    case api::autotest_private::OverviewStateType::kHidden:
      return ash::OverviewAnimationState::kExitAnimationComplete;
    case api::autotest_private::OverviewStateType::kNone:
      break;
  }
  NOTREACHED_IN_MIGRATION();
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
      if (str == entry.str) {
        return entry.key_code;
      }
    }
  }
  NOTREACHED_IN_MIGRATION();
  return ui::VKEY_A;
}

aura::Window* GetActiveWindow() {
  std::vector<raw_ptr<aura::Window, VectorExperimental>> list =
      ash::GetAppWindowList();
  if (!list.size()) {
    return nullptr;
  }
  return wm::GetActivationClient(list[0]->GetRootWindow())->GetActiveWindow();
}

bool IsFrameVisible(views::Widget* widget) {
  views::NonClientFrameView* frame_view =
      widget->non_client_view() ? widget->non_client_view()->frame_view()
                                : nullptr;
  return frame_view && frame_view->GetEnabled() && frame_view->GetVisible();
}

void ConvertPointToHost(aura::Window* root_window, gfx::PointF* location) {
  *location = root_window->GetHost()->GetRootTransform().MapPoint(*location);
}

int GetMouseEventFlags(api::autotest_private::MouseButton button) {
  switch (button) {
    case api::autotest_private::MouseButton::kLeft:
      return ui::EF_LEFT_MOUSE_BUTTON;
    case api::autotest_private::MouseButton::kRight:
      return ui::EF_RIGHT_MOUSE_BUTTON;
    case api::autotest_private::MouseButton::kMiddle:
      return ui::EF_MIDDLE_MOUSE_BUTTON;
    case api::autotest_private::MouseButton::kBack:
      return ui::EF_BACK_MOUSE_BUTTON;
    case api::autotest_private::MouseButton::kForward:
      return ui::EF_FORWARD_MOUSE_BUTTON;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return ui::EF_NONE;
}

// Gets display id out of an optional DOMString display id argument. Returns
// false if optional display id is given but in bad format. Otherwise returns
// true and fills |display_id| with either the primary display id when the
// optional arg is not given or the parsed display id out of the arg
bool GetDisplayIdFromOptionalArg(const std::optional<std::string>& arg,
                                 int64_t* display_id) {
  if (arg && !arg->empty()) {
    return base::StringToInt64(*arg, display_id);
  }

  *display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  return true;
}

class DisplaySmoothnessTracker {
 public:
  using ReportCallback = base::OnceCallback<void(
      const cc::FrameSequenceMetrics::CustomReportData& frame_data,
      std::vector<int>&& throughput)>;

  DisplaySmoothnessTracker() = default;
  DisplaySmoothnessTracker(const DisplaySmoothnessTracker&) = delete;
  DisplaySmoothnessTracker& operator=(const DisplaySmoothnessTracker&) = delete;
  ~DisplaySmoothnessTracker() = default;

  // Return true if tracking is started successfully.
  bool Start(int64_t display_id,
             base::TimeDelta throughput_interval,
             ui::ThroughputTrackerHost::ReportCallback callback) {
    auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
    if (!root_window) {
      return false;
    }

    start_time_ = base::TimeTicks::Now();

    DCHECK(root_window_tracker_.windows().empty());
    root_window_tracker_.Add(root_window);

    tracker_ =
        root_window->layer()->GetCompositor()->RequestNewThroughputTracker();
    tracker_->Start(std::move(callback));

    throughtput_timer_.Start(FROM_HERE, throughput_interval, this,
                             &DisplaySmoothnessTracker::OnThroughputTimerFired);

    return true;
  }

  bool Stop(ReportCallback callback) {
    stopping_ = true;
    throughtput_timer_.Stop();
    callback_ = std::move(callback);
    return tracker_->Stop();
  }

  void CancelReport() { tracker_->CancelReport(); }

  ReportCallback TakeCallback() { return std::move(callback_); }
  std::vector<int> TakeThroughput() { return std::move(throughput_); }

  bool stopping() const { return stopping_; }
  bool has_error() const { return has_error_; }
  base::TimeTicks start_time() const { return start_time_; }

 private:
  void OnThroughputTimerFired() {
    auto windows = root_window_tracker_.windows();
    if (windows.empty()) {
      // RootWindow is gone. This could happen when display is reconfigured
      // during the test run. Treat it as error since no meaningful smoothness
      // data would be captured in such case.
      LOG(ERROR) << "Unable to collect throughput because underlying "
                    "RootWindow is gone.";
      has_error_ = true;
      throughtput_timer_.Stop();
      return;
    }

    DCHECK_EQ(windows.size(), 1u);
    auto* root_window = windows[0].get();
    throughput_.push_back(
        100 - root_window->GetHost()->compositor()->GetPercentDroppedFrames());
  }

  aura::WindowTracker root_window_tracker_;
  std::optional<ui::ThroughputTracker> tracker_;
  ReportCallback callback_;
  bool stopping_ = false;
  bool has_error_ = false;
  base::TimeTicks start_time_;

  base::RepeatingTimer throughtput_timer_;
  std::vector<int> throughput_;
};

using DisplaySmoothnessTrackers =
    std::map<int64_t, std::unique_ptr<DisplaySmoothnessTracker>>;
DisplaySmoothnessTrackers* GetDisplaySmoothnessTrackers() {
  static base::NoDestructor<DisplaySmoothnessTrackers> trackers;
  return trackers.get();
}

// Forwards frame rate data to the callback for |display_id| and resets.
void ForwardFrameRateDataAndReset(
    int64_t display_id,
    const cc::FrameSequenceMetrics::CustomReportData& frame_data) {
  auto* trackers = GetDisplaySmoothnessTrackers();
  auto it = trackers->find(display_id);
  DCHECK(it != trackers->end());

  auto throughput = it->second->TakeThroughput();

  // Moves the callback out and erases the mapping first to allow new tracking
  // for |display_id| to start before |callback| run returns.
  // See https://crbug.com/1098886.
  auto callback = it->second->TakeCallback();
  DCHECK(callback);
  trackers->erase(it);
  std::move(callback).Run(frame_data, std::move(throughput));
}

std::string ResolutionToString(
    ash::assistant::AssistantInteractionResolution resolution) {
  using ash::assistant::AssistantInteractionResolution;
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

std::string CompositorFrameSinkTypeToString(
    viz::mojom::CompositorFrameSinkType type) {
  switch (type) {
    case viz::mojom::CompositorFrameSinkType::kUnspecified:
      return "unspecified";
    case viz::mojom::CompositorFrameSinkType::kVideo:
      return "video";
    case viz::mojom::CompositorFrameSinkType::kMediaStream:
      return "media-stream";
    case viz::mojom::CompositorFrameSinkType::kLayerTree:
      return "layer-tree";
  }
}

// Update when `startThroughputTrackerDataCollection` is called.
base::TimeTicks g_last_start_throughput_data_collection_tick;

}  // namespace

class WindowStateChangeObserver : public aura::WindowObserver {
 public:
  WindowStateChangeObserver(aura::Window* window,
                            chromeos::WindowStateType expected_type,
                            base::OnceCallback<void(bool)> callback)
      : expected_type_(expected_type), callback_(std::move(callback)) {
    DCHECK_NE(window->GetProperty(chromeos::kWindowStateTypeKey),
              expected_type_);
    scoped_observation_.Observe(window);
  }

  WindowStateChangeObserver(const WindowStateChangeObserver&) = delete;
  WindowStateChangeObserver& operator=(const WindowStateChangeObserver&) =
      delete;

  ~WindowStateChangeObserver() override {}

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK(scoped_observation_.IsObservingSource(window));
    if (key == chromeos::kWindowStateTypeKey &&
        window->GetProperty(chromeos::kWindowStateTypeKey) == expected_type_) {
      scoped_observation_.Reset();
      std::move(callback_).Run(/*success=*/true);
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(scoped_observation_.IsObservingSource(window));
    scoped_observation_.Reset();
    std::move(callback_).Run(/*success=*/false);
  }

 private:
  chromeos::WindowStateType expected_type_;
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      scoped_observation_{this};
  base::OnceCallback<void(bool)> callback_;
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
    scoped_observation_.Observe(window);
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
    DCHECK(scoped_observation_.IsObservingSource(window));
    if (!wait_for_bounds_change_ && !wait_for_display_change_) {
      scoped_observation_.Reset();
      std::move(callback_).Run(window->GetBoundsInRootWindow(),
                               ash::WindowState::Get(window)->GetDisplay().id(),
                               success);
    }
  }

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      scoped_observation_{this};
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
        interval_(base::Seconds(1) /
                  std::max(host->compositor()->refresh_rate(), 60.0f)),
        closure_(std::move(closure)),
        weak_ptr_factory_(this) {
    // VM may report slightly lower than 60hz refresh rate.
    LOG_IF(ERROR, host->compositor()->refresh_rate() < 59.98f)
        << "Refresh rate (" << host->compositor()->refresh_rate()
        << ") is too low.";
  }
  ~EventGenerator() = default;

  void ScheduleMouseEvent(ui::EventType type,
                          gfx::PointF location_in_screen,
                          int flags) {
    if (flags == 0 && (type == ui::EventType::kMousePressed ||
                       type == ui::EventType::kMouseReleased)) {
      LOG(ERROR) << "No flags specified for mouse button changes";
    }
    tasks_.push_back(Task(type, location_in_screen, flags));
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
    const gfx::PointF location_in_screen;
    const int flags;
    Status status = kNotScheduled;

    Task(ui::EventType type, gfx::PointF location_in_screen, int flags)
        : type(type), location_in_screen(location_in_screen), flags(flags) {}
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
      case ui::EventType::kMousePressed:
      case ui::EventType::kMouseReleased: {
        bool pressed = (task->type == ui::EventType::kMousePressed);
        if (task->flags & ui::EF_LEFT_MOUSE_BUTTON) {
          input_injector_->InjectMouseButton(ui::EF_LEFT_MOUSE_BUTTON, pressed);
        }
        if (task->flags & ui::EF_MIDDLE_MOUSE_BUTTON) {
          input_injector_->InjectMouseButton(ui::EF_MIDDLE_MOUSE_BUTTON,
                                             pressed);
        }
        if (task->flags & ui::EF_RIGHT_MOUSE_BUTTON) {
          input_injector_->InjectMouseButton(ui::EF_RIGHT_MOUSE_BUTTON,
                                             pressed);
        }
        if (task->flags & ui::EF_BACK_MOUSE_BUTTON) {
          input_injector_->InjectMouseButton(ui::EF_BACK_MOUSE_BUTTON, pressed);
        }
        if (task->flags & ui::EF_FORWARD_MOUSE_BUTTON) {
          input_injector_->InjectMouseButton(ui::EF_FORWARD_MOUSE_BUTTON,
                                             pressed);
        }
        break;
      }
      case ui::EventType::kMouseMoved: {
        display::Display display =
            display::Screen::GetScreen()->GetDisplayNearestPoint(
                gfx::ToFlooredPoint((task->location_in_screen)));
        auto* root_window = ash::Shell::GetRootWindowForDisplayId(display.id());
        if (!root_window->GetBoundsInScreen().Contains(
                gfx::ToFlooredPoint(task->location_in_screen))) {
          // Not in any of the display. Does nothing and schedules a new task.
          OnFinishedProcessingEvent();
          return;
        }
        gfx::PointF location_in_host(task->location_in_screen);
        wm::ConvertPointFromScreen(root_window, &location_in_host);
        ConvertPointToHost(root_window, &location_in_host);
        if (root_window->GetHost() != host_) {
          // Switching to the new display.
          host_ = root_window->GetHost();
          host_->MoveCursorToLocationInPixels(
              gfx::ToFlooredPoint(location_in_host));
        }
        // The location should be offset by the origin of the root-window since
        // ui::SystemInputInjector expects so.
        input_injector_->MoveCursorTo(
            location_in_host + host_->GetBoundsInPixels().OffsetFromOrigin());
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
    }

    // Post a task after scheduling the event and assumes that when the task
    // runs, it implies that the processing of the scheduled event is finished.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&EventGenerator::OnFinishedProcessingEvent,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

  void OnFinishedProcessingEvent() {
    if (tasks_.empty()) {
      return;
    }

    DCHECK_EQ(tasks_.front().status, Task::kScheduled);
    tasks_.pop_front();
    const auto& runner = base::SequencedTaskRunner::GetCurrentDefault();
    auto closure = base::BindOnce(&EventGenerator::SendEvent,
                                  weak_ptr_factory_.GetWeakPtr());
    // Non moving tasks can be done immediately.
    if (tasks_.empty() || tasks_.front().type == ui::EventType::kMousePressed ||
        tasks_.front().type == ui::EventType::kMouseReleased) {
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
  raw_ptr<aura::WindowTreeHost> host_;
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
  if (!IsTestMode(browser_context())) {
    chrome::AttemptUserExit();
  }
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRestartFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRestartFunction::~AutotestPrivateRestartFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateRestartFunction::Run() {
  DVLOG(1) << "AutotestPrivateRestartFunction";
  if (!IsTestMode(browser_context())) {
    chrome::AttemptRestart();
  }
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateShutdownFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateShutdownFunction::~AutotestPrivateShutdownFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateShutdownFunction::Run() {
  std::optional<api::autotest_private::Shutdown::Params> params =
      api::autotest_private::Shutdown::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateShutdownFunction " << params->force;

  if (!IsTestMode(browser_context())) {
    chrome::AttemptExit();
  }
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateLoginStatusFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateLoginStatusFunction::~AutotestPrivateLoginStatusFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateLoginStatusFunction::Run() {
  DVLOG(1) << "AutotestPrivateLoginStatusFunction";
  base::Value::Dict result;
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();

  // default_screen_locker()->locked() is set when the UI is ready, so this
  // tells us both views based lockscreen UI and screenlocker are ready.
  const bool is_screen_locked =
      !!ash::ScreenLocker::default_screen_locker() &&
      ash::ScreenLocker::default_screen_locker()->locked();

  if (user_manager) {
    result.Set("isLoggedIn", user_manager->IsUserLoggedIn());
    result.Set("isOwner", user_manager->IsCurrentUserOwner());
    result.Set("isScreenLocked", is_screen_locked);
    result.Set("isLockscreenWallpaperAnimating",
               is_screen_locked && ash::Shell::Get()
                                       ->GetPrimaryRootWindowController()
                                       ->wallpaper_widget_controller()
                                       ->IsAnimating());
    result.Set("isReadyForPassword",
               ash::LoginScreen::Get()->IsReadyForPassword());

    const user_manager::UserList& users = user_manager->GetUsers();
    bool user_images_loaded = true;
    for (const user_manager::User* user : users) {
      if (user->image_is_loading()) {
        user_images_loaded = false;
        break;
      }
    }
    result.Set("areAllUserImagesLoaded", user_images_loaded);

    if (user_manager->IsUserLoggedIn()) {
      result.Set("isRegularUser",
                 user_manager->IsLoggedInAsUserWithGaiaAccount());
      result.Set("isGuest", user_manager->IsLoggedInAsGuest());
      result.Set("isKiosk", user_manager->IsLoggedInAsKioskApp());

      const user_manager::User* user = user_manager->GetActiveUser();
      result.Set("email", user->GetAccountId().GetUserEmail());
      result.Set("displayEmail", user->display_email());
      result.Set("displayName", user->display_name());

      std::string user_image;
      switch (user->image_index()) {
        case user_manager::UserImage::Type::kExternal:
          user_image = "file";
          break;

        case user_manager::UserImage::Type::kProfile:
          user_image = "profile";
          break;

        default:
          user_image = base::NumberToString(user->image_index());
          break;
      }
      result.Set("userImage", user_image);

      if (user->HasGaiaAccount()) {
        result.Set("hasValidOauth2Token",
                   user->oauth_token_status() ==
                       user_manager::User::OAUTH2_TOKEN_STATUS_VALID);
      }
    }
  }
  return RespondNow(WithArguments(std::move(result)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateWaitForLoginAnimationEndFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateWaitForLoginAnimationEndFunction::
    ~AutotestPrivateWaitForLoginAnimationEndFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateWaitForLoginAnimationEndFunction::Run() {
  DVLOG(1) << "AutotestPrivateWaitForLoginAnimationEndFunction";
  ash::Shell::Get()
      ->login_unlock_throughput_recorder()
      ->post_login_deferred_task_runner()
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&AutotestPrivateWaitForLoginAnimationEndFunction::
                             OnLoginAnimationEnd,
                         this));
  return RespondLater();
}

void AutotestPrivateWaitForLoginAnimationEndFunction::OnLoginAnimationEnd() {
  DVLOG(1)
      << "AutotestPrivateWaitForLoginAnimationEndFunction::OnLoginAnimationEnd";
  Respond(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateLockScreenFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateLockScreenFunction::~AutotestPrivateLockScreenFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateLockScreenFunction::Run() {
  DVLOG(1) << "AutotestPrivateLockScreenFunction";

  ash::SessionManagerClient::Get()->RequestLockScreen();
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

  base::Value::Dict all_policies_dict =
      policy::PolicyConversions(
          std::make_unique<policy::ChromePolicyConversionsClient>(
              browser_context()))
          .EnableDeviceLocalAccountPolicies(true)
          .EnableDeviceInfo(true)
          .ToValueDict();

  return RespondNow(WithArguments(std::move(all_policies_dict)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRefreshEnterprisePoliciesFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRefreshEnterprisePoliciesFunction::
    ~AutotestPrivateRefreshEnterprisePoliciesFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRefreshEnterprisePoliciesFunction::Run() {
  DVLOG(1) << "AutotestPrivateRefreshEnterprisePoliciesFunction";

  g_browser_process->policy_service()->RefreshPolicies(
      base::BindOnce(
          &AutotestPrivateRefreshEnterprisePoliciesFunction::RefreshDone, this),
      policy::PolicyFetchReason::kTest);
  return RespondLater();
}

void AutotestPrivateRefreshEnterprisePoliciesFunction::RefreshDone() {
  Respond(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRefreshRemoteCommandsFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRefreshRemoteCommandsFunction::
    ~AutotestPrivateRefreshRemoteCommandsFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRefreshRemoteCommandsFunction::Run() {
  DVLOG(1) << "AutotestPrivateRefreshRemoteCommandsFunction";
  // Allow tests to manually fetch remote commands. Useful for testing or when
  // the invalidation service is not working properly.
  policy::CloudPolicyManager* const device_manager =
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetDeviceCloudPolicyManager();
  policy::CloudPolicyManager* const user_manager =
      Profile::FromBrowserContext(browser_context())
          ->GetUserCloudPolicyManagerAsh();

  // Fetch both device and user remote commands.
  for (policy::CloudPolicyManager* manager : {device_manager, user_manager}) {
    if (manager) {
      policy::RemoteCommandsService* const remote_commands_service =
          manager->core()->remote_commands_service();
      if (remote_commands_service) {
        remote_commands_service->FetchRemoteCommands();
      }
    }
  }
  // TODO(b/260972611): Wait till remote commands are fetched.
  return RespondNow(NoArguments());
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

  base::Value::List extensions_values;
  ExtensionList all;
  all.insert(all.end(), extensions.begin(), extensions.end());
  all.insert(all.end(), disabled_extensions.begin(), disabled_extensions.end());
  for (ExtensionList::const_iterator it = all.begin(); it != all.end(); ++it) {
    const Extension* extension = it->get();
    std::string id = extension->id();
    ManifestLocation location = extension->location();
    const ExtensionAction* action =
        extension_action_manager->GetExtensionAction(*extension);

    extensions_values.Append(
        base::Value::Dict()
            .Set("id", id)
            .Set("version", extension->VersionString())
            .Set("name", extension->name())
            .Set("publicKey", extension->public_key())
            .Set("description", extension->description())
            .Set("backgroundUrl",
                 BackgroundInfo::GetBackgroundURL(extension).spec())
            .Set("optionsUrl",
                 OptionsPageInfo::GetOptionsPage(extension).spec())
            .Set("hostPermissions", GetHostPermissions(extension, false))
            .Set("effectiveHostPermissions",
                 GetHostPermissions(extension, true))
            .Set("apiPermissions", GetAPIPermissions(extension))

            .Set("isComponent", location == ManifestLocation::kComponent)
            .Set("isInternal", location == ManifestLocation::kInternal)
            .Set("isUserInstalled", location == ManifestLocation::kInternal ||
                                        Manifest::IsUnpackedLocation(location))
            .Set("isEnabled", service->IsExtensionEnabled(id))
            .Set("allowedInIncognito",
                 util::IsIncognitoEnabled(id, browser_context()))
            .Set("hasPageAction",
                 action && action->action_type() == ActionInfo::Type::kPage));
  }

  return RespondNow(WithArguments(
      base::Value::Dict().Set("extensions", std::move(extensions_values))));
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
    int testarray[3] = {0, 0, 0};

    // Cause Address Sanitizer to abort this process.
    int index = 5;
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
  std::optional<api::autotest_private::SetTouchpadSensitivity::Params> params =
      api::autotest_private::SetTouchpadSensitivity::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetTouchpadSensitivityFunction " << params->value;

  ash::system::InputDeviceSettings::Get()->SetTouchpadSensitivity(
      params->value);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetTapToClickFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetTapToClickFunction::~AutotestPrivateSetTapToClickFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateSetTapToClickFunction::Run() {
  std::optional<api::autotest_private::SetTapToClick::Params> params =
      api::autotest_private::SetTapToClick::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetTapToClickFunction " << params->enabled;

  ash::system::InputDeviceSettings::Get()->SetTapToClick(params->enabled);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetThreeFingerClickFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetThreeFingerClickFunction::
    ~AutotestPrivateSetThreeFingerClickFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetThreeFingerClickFunction::Run() {
  std::optional<api::autotest_private::SetThreeFingerClick::Params> params =
      api::autotest_private::SetThreeFingerClick::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetThreeFingerClickFunction " << params->enabled;

  ash::system::InputDeviceSettings::Get()->SetThreeFingerClick(params->enabled);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetTapDraggingFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetTapDraggingFunction::
    ~AutotestPrivateSetTapDraggingFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateSetTapDraggingFunction::Run() {
  std::optional<api::autotest_private::SetTapDragging::Params> params =
      api::autotest_private::SetTapDragging::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetTapDraggingFunction " << params->enabled;

  ash::system::InputDeviceSettings::Get()->SetTapDragging(params->enabled);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetNaturalScrollFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetNaturalScrollFunction::
    ~AutotestPrivateSetNaturalScrollFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetNaturalScrollFunction::Run() {
  std::optional<api::autotest_private::SetNaturalScroll::Params> params =
      api::autotest_private::SetNaturalScroll::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetNaturalScrollFunction " << params->enabled;

  ash::system::InputDeviceSettings::Get()->SetNaturalScroll(params->enabled);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetMouseSensitivityFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetMouseSensitivityFunction::
    ~AutotestPrivateSetMouseSensitivityFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetMouseSensitivityFunction::Run() {
  std::optional<api::autotest_private::SetMouseSensitivity::Params> params =
      api::autotest_private::SetMouseSensitivity::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetMouseSensitivityFunction " << params->value;

  ash::system::InputDeviceSettings::Get()->SetMouseSensitivity(params->value);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetPrimaryButtonRightFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetPrimaryButtonRightFunction::
    ~AutotestPrivateSetPrimaryButtonRightFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetPrimaryButtonRightFunction::Run() {
  std::optional<api::autotest_private::SetPrimaryButtonRight::Params> params =
      api::autotest_private::SetPrimaryButtonRight::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetPrimaryButtonRightFunction " << params->right;

  ash::system::InputDeviceSettings::Get()->SetPrimaryButtonRight(params->right);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetMouseReverseScrollFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetMouseReverseScrollFunction::
    ~AutotestPrivateSetMouseReverseScrollFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetMouseReverseScrollFunction::Run() {
  std::optional<api::autotest_private::SetMouseReverseScroll::Params> params =
      api::autotest_private::SetMouseReverseScroll::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetMouseReverseScrollFunction "
           << params->enabled;

  ash::system::InputDeviceSettings::Get()->SetMouseReverseScroll(
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
  base::Value::List values;
  for (message_center::Notification* notification : notification_set) {
    values.Append(MakeDictionaryFromNotification(*notification));
  }
  return RespondNow(WithArguments(std::move(values)));
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
  if (!arc_session_manager) {
    return RespondNow(Error("Could not find ARC session manager"));
  }

  const double start_ticks =
      (arc_session_manager->start_time() - base::TimeTicks()).InMillisecondsF();
  return RespondNow(WithArguments(start_ticks));
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

  if (!arc::IsArcAllowedForProfile(profile)) {
    return RespondNow(Error("ARC is not available for the current user"));
  }

  arc::ArcSessionManager* const arc_session_manager =
      arc::ArcSessionManager::Get();
  if (!arc_session_manager) {
    return RespondNow(Error("Could not find ARC session manager"));
  }

  const base::Time now_time = base::Time::Now();
  const base::TimeTicks now_ticks = base::TimeTicks::Now();
  const base::TimeTicks pre_start_time = arc_session_manager->pre_start_time();
  const base::TimeTicks start_time = arc_session_manager->start_time();

  arc_state.provisioned = arc::IsArcProvisioned(profile);
  arc_state.tos_needed = arc::IsArcTermsOfServiceNegotiationNeeded(profile);
  arc_state.pre_start_time = pre_start_time.is_null()
                                 ? 0
                                 : (now_time - (now_ticks - pre_start_time))
                                       .InMillisecondsFSinceUnixEpoch();
  arc_state.start_time = start_time.is_null()
                             ? 0
                             : (now_time - (now_ticks - start_time))
                                   .InMillisecondsFSinceUnixEpoch();

  return RespondNow(WithArguments(arc_state.ToValue()));
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
    play_store_state.enabled = arc::IsArcPlayStoreEnabledForProfile(profile);
    play_store_state.managed =
        arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile);
  }
  return RespondNow(WithArguments(play_store_state.ToValue()));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetPlayStoreEnabledFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetPlayStoreEnabledFunction::
    ~AutotestPrivateSetPlayStoreEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetPlayStoreEnabledFunction::Run() {
  std::optional<api::autotest_private::SetPlayStoreEnabled::Params> params =
      api::autotest_private::SetPlayStoreEnabled::Params::Create(args());
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
    // SetAllowedPref. At this moment, we don't distinguish the actual
    // values and set kArcLocationServiceEnabled to true and leave
    // kArcBackupRestoreEnabled unmodified, which is acceptable for autotests
    // currently.
    profile->GetPrefs()->SetBoolean(arc::prefs::kArcLocationServiceEnabled,
                                    true);
    // Since we are settings location to enabled, we don't have to sync this
    // settings from android.
    if (base::FeatureList::IsEnabled(ash::features::kCrosPrivacyHub)) {
      profile->GetPrefs()->SetBoolean(
          arc::prefs::kArcInitialLocationSettingSyncRequired, false);
    }
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
  std::optional<api::autotest_private::IsAppShown::Params> params =
      api::autotest_private::IsAppShown::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateIsAppShownFunction " << params->app_id;

  ChromeShelfController* const controller = ChromeShelfController::instance();
  if (!controller) {
    return RespondNow(Error("Controller not available"));
  }

  const ash::ShelfItem* item =
      controller->GetItem(ash::ShelfID(params->app_id));
  // App must be running and not pending in deferred launch.
  const bool window_attached =
      item && item->status == ash::ShelfItemStatus::STATUS_RUNNING &&
      !controller->GetShelfSpinnerController()->HasApp(params->app_id);
  return RespondNow(WithArguments(window_attached));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateIsAppShownFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateIsArcProvisionedFunction::
    ~AutotestPrivateIsArcProvisionedFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateIsArcProvisionedFunction::Run() {
  DVLOG(1) << "AutotestPrivateIsArcProvisionedFunction";
  return RespondNow(WithArguments(
      arc::IsArcProvisioned(Profile::FromBrowserContext(browser_context()))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetLacrosInfoFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetLacrosInfoFunction::~AutotestPrivateGetLacrosInfoFunction() =
    default;

// static
api::autotest_private::LacrosState
AutotestPrivateGetLacrosInfoFunction::ToLacrosState(
    crosapi::BrowserManager::State state) {
  switch (state) {
    case crosapi::BrowserManager::State::NOT_INITIALIZED:
      return api::autotest_private::LacrosState::kNotInitialized;
    case crosapi::BrowserManager::State::MOUNTING:
      return api::autotest_private::LacrosState::kMounting;
    case crosapi::BrowserManager::State::UNAVAILABLE:
      return api::autotest_private::LacrosState::kUnavailable;
    case crosapi::BrowserManager::State::STOPPED:
      return api::autotest_private::LacrosState::kStopped;
    case crosapi::BrowserManager::State::PREPARING_FOR_LAUNCH:
      return api::autotest_private::LacrosState::kPreparingForLaunch;
    case crosapi::BrowserManager::State::STARTING:
      return api::autotest_private::LacrosState::kStarting;
    case crosapi::BrowserManager::State::RUNNING:
      return api::autotest_private::LacrosState::kRunning;
    case crosapi::BrowserManager::State::WAITING_FOR_MOJO_DISCONNECTED:
      return api::autotest_private::LacrosState::kWaitingForMojoDisconnected;
    case crosapi::BrowserManager::State::WAITING_FOR_PROCESS_TERMINATED:
      return api::autotest_private::LacrosState::kWaitingForProcessTerminated;
  }
}

// static
api::autotest_private::LacrosMode
AutotestPrivateGetLacrosInfoFunction::ToLacrosMode(bool is_enabled) {
  return is_enabled ? api::autotest_private::LacrosMode::kOnly
                    : api::autotest_private::LacrosMode::kDisabled;
}

ExtensionFunction::ResponseAction AutotestPrivateGetLacrosInfoFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetLacrosInfoFunction";
  auto* browser_manager = crosapi::BrowserManager::Get();
  return RespondNow(WithArguments(
      base::Value::Dict()
          .Set("state", api::autotest_private::ToString(
                            ToLacrosState(browser_manager->state_)))
          .Set("isKeepAlive", browser_manager->IsKeepAliveEnabled())
          // TODO(neis): Rename lacrosPath to avoid confusion, or make it be the
          // binary path. Either requires changes in tast-tests.
          .Set("lacrosPath",
               browser_manager->lacros_path().empty()
                   ? ""
                   : browser_manager->lacros_path().DirName().MaybeAsASCII())
          .Set("mode", api::autotest_private::ToString(ToLacrosMode(
                           crosapi::browser_util::IsLacrosEnabled())))
          .Set("isEnabled", crosapi::browser_util::IsLacrosEnabled())));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetArcAppFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetArcAppFunction::~AutotestPrivateGetArcAppFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateGetArcAppFunction::Run() {
  std::optional<api::autotest_private::GetArcApp::Params> params =
      api::autotest_private::GetArcApp::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateGetArcAppFunction " << params->app_id;

  ArcAppListPrefs* const prefs =
      ArcAppListPrefs::Get(Profile::FromBrowserContext(browser_context()));
  if (!prefs) {
    return RespondNow(Error("ARC is not available"));
  }

  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(params->app_id);
  if (!app_info) {
    return RespondNow(Error("App is not available"));
  }

  return RespondNow(WithArguments(
      base::Value::Dict()
          .Set("name", std::move(app_info->name))
          .Set("packageName", std::move(app_info->package_name))
          .Set("activity", std::move(app_info->activity))
          .Set("intentUri", std::move(app_info->intent_uri))
          .Set("iconResourceId", std::move(app_info->icon_resource_id))
          .Set("lastLaunchTime",
               app_info->last_launch_time.InMillisecondsFSinceUnixEpoch())
          .Set("installTime",
               app_info->install_time.InMillisecondsFSinceUnixEpoch())
          .Set("sticky", app_info->sticky)
          .Set("notificationsEnabled", app_info->notifications_enabled)
          .Set("ready", app_info->ready)
          .Set("suspended", app_info->suspended)
          .Set("showInLauncher", app_info->show_in_launcher)
          .Set("shortcut", app_info->shortcut)
          .Set("launchable", app_info->launchable)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetArcAppKillsFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetArcAppKillsFunction::
    ~AutotestPrivateGetArcAppKillsFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateGetArcAppKillsFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetArcAppKillsFunction";

  arc::ArcServiceManager* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    return RespondNow(Error("ARC service manager is not available"));
  }

  arc::ArcBridgeService* arc_bridge_service =
      arc_service_manager->arc_bridge_service();

  if (!arc_bridge_service) {
    return RespondNow(Error("ARC bridge service is not available"));
  }

  arc::mojom::ProcessInstance* process_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service->process(), RequestLowMemoryKillCounts);

  if (!process_instance) {
    return RespondNow(Error("ARC process service is not available"));
  }

  process_instance->RequestLowMemoryKillCounts(base::BindOnce(
      &AutotestPrivateGetArcAppKillsFunction::OnKillCounts, this));

  return RespondLater();
}

void AutotestPrivateGetArcAppKillsFunction::OnKillCounts(
    arc::mojom::LowMemoryKillCountsPtr counts) {
  api::autotest_private::ArcAppKillsDict result;
  result.oom = counts->guest_oom;
  result.lmkd_foreground = counts->lmkd_foreground;
  result.lmkd_perceptible = counts->lmkd_perceptible;
  result.lmkd_cached = counts->lmkd_cached;
  result.pressure_foreground = counts->pressure_foreground;
  result.pressure_perceptible = counts->pressure_perceptible;
  result.pressure_cached = counts->pressure_cached;
  Respond(WithArguments(result.ToValue()));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetArcPackageFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetArcPackageFunction::~AutotestPrivateGetArcPackageFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateGetArcPackageFunction::Run() {
  std::optional<api::autotest_private::GetArcPackage::Params> params =
      api::autotest_private::GetArcPackage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateGetArcPackageFunction " << params->package_name;

  ArcAppListPrefs* const prefs =
      ArcAppListPrefs::Get(Profile::FromBrowserContext(browser_context()));
  if (!prefs) {
    return RespondNow(Error("ARC is not available"));
  }

  base::Value::Dict package_value;
  {
    const std::unique_ptr<ArcAppListPrefs::PackageInfo> package_info =
        prefs->GetPackage(params->package_name);
    if (!package_info) {
      return RespondNow(Error("Package is not available"));
    }

    package_value.Set("packageName", std::move(package_info->package_name));
    package_value.Set("packageVersion", package_info->package_version);
    package_value.Set(
        "lastBackupAndroidId",
        base::NumberToString(package_info->last_backup_android_id));
    package_value.Set("lastBackupTime",
                      base::Time::FromDeltaSinceWindowsEpoch(
                          base::Microseconds(package_info->last_backup_time))
                          .InMillisecondsFSinceUnixEpoch());
    package_value.Set("shouldSync", package_info->should_sync);
    package_value.Set("vpnProvider", package_info->vpn_provider);
  }
  return RespondNow(WithArguments(std::move(package_value)));
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
  ash::SystemWebAppManager* swa_manager =
      ash::SystemWebAppManager::Get(profile);

  if (!swa_manager) {
    return RespondNow(Error("System Web Apps are not available for profile."));
  }

  swa_manager->on_apps_synchronized().Post(
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
  ash::SystemWebAppManager* swa_manager =
      ash::SystemWebAppManager::Get(profile);

  if (!swa_manager) {
    return RespondNow(Error("System Web Apps are not available for profile."));
  }

  swa_manager->on_apps_synchronized().Post(
      FROM_HERE,
      base::BindOnce(&AutotestPrivateGetRegisteredSystemWebAppsFunction::
                         OnSystemWebAppsInstalled,
                     this));
  return RespondLater();
}

void AutotestPrivateGetRegisteredSystemWebAppsFunction::
    OnSystemWebAppsInstalled() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  ash::SystemWebAppManager* swa_manager =
      ash::SystemWebAppManager::Get(profile);
  std::vector<api::autotest_private::SystemWebApp> result;
  for (const auto& type_and_info : swa_manager->system_app_delegates()) {
    api::autotest_private::SystemWebApp system_web_app;
    ash::SystemWebAppDelegate* delegate = type_and_info.second.get();
    system_web_app.internal_name = delegate->GetInternalName();
    system_web_app.url =
        delegate->GetInstallUrl().DeprecatedGetOriginAsURL().spec();
    system_web_app.name = base::UTF16ToUTF8(delegate->GetWebAppInfo()->title);

    std::optional<webapps::AppId> app_id =
        swa_manager->GetAppIdForSystemApp(type_and_info.first);
    if (app_id) {
      system_web_app.start_url =
          ash::SystemWebAppManager::GetWebAppProvider(profile)
              ->registrar_unsafe()
              .GetAppLaunchUrl(*app_id)
              .spec();
    }
    result.push_back(std::move(system_web_app));
  }

  Respond(ArgumentList(
      api::autotest_private::GetRegisteredSystemWebApps::Results::Create(
          result)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateIsSystemWebAppOpenFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateIsSystemWebAppOpenFunction::
    AutotestPrivateIsSystemWebAppOpenFunction() = default;

AutotestPrivateIsSystemWebAppOpenFunction::
    ~AutotestPrivateIsSystemWebAppOpenFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateIsSystemWebAppOpenFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  ash::SystemWebAppManager* swa_manager =
      ash::SystemWebAppManager::Get(profile);

  if (!swa_manager) {
    return RespondNow(Error("System web Apps are not available for profile."));
  }

  std::optional<api::autotest_private::IsSystemWebAppOpen::Params> params =
      api::autotest_private::IsSystemWebAppOpen::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateIsSystemWebAppOpenFunction " << params->app_id;

  swa_manager->on_apps_synchronized().Post(
      FROM_HERE,
      base::BindOnce(
          &AutotestPrivateIsSystemWebAppOpenFunction::OnSystemWebAppsInstalled,
          this));
  return RespondLater();
}

void AutotestPrivateIsSystemWebAppOpenFunction::OnSystemWebAppsInstalled() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  std::optional<api::autotest_private::IsSystemWebAppOpen::Params> params =
      api::autotest_private::IsSystemWebAppOpen::Params::Create(args());
  std::optional<ash::SystemWebAppType> app_type =
      ash::GetSystemWebAppTypeForAppId(profile, params->app_id);
  if (!app_type) {
    Respond(Error("No system web app is found by given app id."));
    return;
  }

  Respond(WithArguments(ash::FindSystemWebAppBrowser(profile, *app_type) !=
                        nullptr));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateLaunchAppFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateLaunchAppFunction::~AutotestPrivateLaunchAppFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateLaunchAppFunction::Run() {
  std::optional<api::autotest_private::LaunchApp::Params> params =
      api::autotest_private::LaunchApp::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateLaunchAppFunction " << params->app_id;

  ChromeShelfController* const controller = ChromeShelfController::instance();
  if (!controller) {
    return RespondNow(Error("Controller not available"));
  }
  controller->LaunchApp(ash::ShelfID(params->app_id),
                        ash::ShelfLaunchSource::LAUNCH_FROM_INTERNAL,
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
  std::optional<api::autotest_private::LaunchSystemWebApp::Params> params =
      api::autotest_private::LaunchSystemWebApp::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateLaunchSystemWebAppFunction name: "
           << params->app_name << " url: " << params->url;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  ash::SystemWebAppManager* swa_manager =
      ash::SystemWebAppManager::Get(profile);

  if (!swa_manager) {
    return RespondNow(Error("System Web Apps are not available for profile."));
  }

  swa_manager->on_apps_synchronized().Post(
      FROM_HERE,
      base::BindOnce(
          &AutotestPrivateLaunchSystemWebAppFunction::OnSystemWebAppsInstalled,
          this));
  return RespondLater();
}

void AutotestPrivateLaunchSystemWebAppFunction::OnSystemWebAppsInstalled() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  ash::SystemWebAppManager* swa_manager =
      ash::SystemWebAppManager::Get(profile);

  std::optional<api::autotest_private::LaunchSystemWebApp::Params> params =
      api::autotest_private::LaunchSystemWebApp::Params::Create(args());
  std::optional<ash::SystemWebAppType> app_type;

  for (const auto& type_and_info : swa_manager->system_app_delegates()) {
    if (type_and_info.second->GetInternalName() == params->app_name) {
      app_type = type_and_info.first;
      break;
    }
  }
  if (!app_type.has_value()) {
    Respond(Error("No mapped system web app found"));
    return;
  }

  ash::SystemAppLaunchParams swa_params;
  swa_params.url = GURL(params->url);
  ash::LaunchSystemWebAppAsync(profile, *app_type, swa_params);

  Respond(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateLaunchFilesAppToPathFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateLaunchFilesAppToPathFunction::
    ~AutotestPrivateLaunchFilesAppToPathFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateLaunchFilesAppToPathFunction::Run() {
  std::optional<api::autotest_private::LaunchFilesAppToPath::Params> params =
      api::autotest_private::LaunchFilesAppToPath::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FilePath absolute_path(params->absolute_path);
  if (!absolute_path.IsAbsolute()) {
    return RespondNow(Error("Supplied path is not absolute"));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  file_manager::util::ShowItemInFolder(
      profile, std::move(absolute_path),
      base::BindOnce(
          &AutotestPrivateLaunchFilesAppToPathFunction::OnShowItemInFolder,
          this));

  return RespondLater();
}

void AutotestPrivateLaunchFilesAppToPathFunction::OnShowItemInFolder(
    platform_util::OpenOperationResult result) {
  if (result != platform_util::OpenOperationResult::OPEN_SUCCEEDED) {
    DVLOG(1) << "Failed navigating to folder with error: " << result;
    Respond(Error("Failed trying to open the supplied path"));
    return;
  }

  Respond(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateCloseAppFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateCloseAppFunction::~AutotestPrivateCloseAppFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateCloseAppFunction::Run() {
  std::optional<api::autotest_private::CloseApp::Params> params =
      api::autotest_private::CloseApp::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateCloseAppFunction " << params->app_id;

  ChromeShelfController* const controller = ChromeShelfController::instance();
  if (!controller) {
    return RespondNow(Error("Controller not available"));
  }
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
      ui::EndpointType::kDefault, {.notify_if_restricted = false});
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data_dst, &data);
  return RespondNow(WithArguments(data));
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
  std::optional<api::autotest_private::SetClipboardTextData::Params> params =
      api::autotest_private::SetClipboardTextData::Params::Create(args());
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
  std::optional<api::autotest_private::SetCrostiniEnabled::Params> params =
      api::autotest_private::SetCrostiniEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetCrostiniEnabledFunction " << params->enabled;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile)) {
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));
  }

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
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile)) {
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));
  }

  // Run GUI installer which will install crostini vm / container and
  // start terminal app on completion.  After starting the installer,
  // we call RestartCrostini and we will be put in the pending restarters
  // queue and be notified on success/otherwise of installation.
  ash::CrostiniInstallerDialog::Show(
      profile,
      base::BindOnce([](base::WeakPtr<ash::CrostiniInstallerUI> installer_ui) {
        installer_ui->ClickInstallForTesting();
      }));
  crostini::CrostiniManager::GetForProfile(profile)->RestartCrostini(
      crostini::DefaultContainerId(),
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
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile)) {
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));
  }

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
  if (result == crostini::CrostiniResult::SUCCESS) {
    Respond(NoArguments());
  } else {
    Respond(Error("Error uninstalling crostini"));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateExportCrostiniFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateExportCrostiniFunction::
    ~AutotestPrivateExportCrostiniFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateExportCrostiniFunction::Run() {
  std::optional<api::autotest_private::ExportCrostini::Params> params =
      api::autotest_private::ExportCrostini::Params::Create(args());
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

  crostini::CrostiniExportImportFactory::GetForProfile(profile)
      ->ExportContainer(
          crostini::DefaultContainerId(),
          file_manager::util::GetDownloadsFolderForProfile(profile).Append(
              path),
          base::BindOnce(
              &AutotestPrivateExportCrostiniFunction::CrostiniExported, this));

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
  std::optional<api::autotest_private::ImportCrostini::Params> params =
      api::autotest_private::ImportCrostini::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateImportCrostiniFunction " << params->path;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (!crostini::CrostiniFeatures::Get()->IsAllowedNow(profile) ||
      !crostini::CrostiniFeatures::Get()->IsExportImportUIAllowed(profile)) {
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));
  }

  base::FilePath path(params->path);
  if (path.ReferencesParent()) {
    return RespondNow(Error("Invalid import path must not reference parent"));
  }
  crostini::CrostiniExportImportFactory::GetForProfile(profile)
      ->ImportContainer(
          crostini::DefaultContainerId(),
          file_manager::util::GetDownloadsFolderForProfile(profile).Append(
              path),
          base::BindOnce(
              &AutotestPrivateImportCrostiniFunction::CrostiniImported, this));

  return RespondLater();
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateCouldAllowCrostiniFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateCouldAllowCrostiniFunction::
    ~AutotestPrivateCouldAllowCrostiniFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateCouldAllowCrostiniFunction::Run() {
  DVLOG(1) << "AutotestPrivateCouldAllowCrostiniFunction";

  Profile* profile = Profile::FromBrowserContext(browser_context());
  return RespondNow(WithArguments(
      crostini::CrostiniFeatures::Get()->CouldBeAllowed(profile)));
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
  std::optional<api::autotest_private::SetPluginVMPolicy::Params> params =
      api::autotest_private::SetPluginVMPolicy::Params::Create(args());
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
  plugin_vm::ShowPluginVmInstallerView(profile);

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateInstallBorealisFunction
///////////////////////////////////////////////////////////////////////////////

class AutotestPrivateInstallBorealisFunction::InstallationObserver
    : public borealis::BorealisInstaller::Observer {
 public:
  InstallationObserver(
      Profile* profile,
      base::OnceCallback<void(std::string)> completion_callback)
      : observation_(this),
        completion_callback_(std::move(completion_callback)) {
    observation_.Observe(
        &borealis::BorealisServiceFactory::GetForProfile(profile)->Installer());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](Profile* profile) {
              borealis::BorealisServiceFactory::GetForProfile(profile)
                  ->Installer()
                  .Start();
            },
            profile));
  }

  void OnProgressUpdated(double fraction_complete) override {}

  void OnStateUpdated(
      borealis::BorealisInstaller::InstallingState new_state) override {}

  void OnInstallationEnded(borealis::mojom::InstallResult result,
                           const std::string& error_description) override {
    std::move(completion_callback_)
        .Run(result == borealis::mojom::InstallResult::kSuccess
                 ? ""
                 : "Failed to install Borealis: " + error_description);
  }

  void OnCancelInitiated() override {}

 private:
  base::ScopedObservation<borealis::BorealisInstaller,
                          borealis::BorealisInstaller::Observer>
      observation_;
  base::OnceCallback<void(std::string)> completion_callback_;
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

void AutotestPrivateInstallBorealisFunction::Complete(
    std::string error_or_empty) {
  if (error_or_empty.empty()) {
    Respond(NoArguments());
  } else {
    Respond(Error(error_or_empty));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRegisterComponentFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRegisterComponentFunction::
    ~AutotestPrivateRegisterComponentFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRegisterComponentFunction::Run() {
  std::optional<api::autotest_private::RegisterComponent::Params> params =
      api::autotest_private::RegisterComponent::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateRegisterComponentFunction " << params->name
           << ", " << params->path;

  g_browser_process->platform_part()
      ->component_manager_ash()
      ->RegisterCompatiblePath(params->name,
                               component_updater::CompatibleComponentInfo(
                                   base::FilePath(params->path), std::nullopt));

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
  Respond(WithArguments(GetPngDataAsString(png_data)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateTakeScreenshotForDisplayFunction
///////////////////////////////////////////////////////////////////////////////
AutotestPrivateTakeScreenshotForDisplayFunction::
    ~AutotestPrivateTakeScreenshotForDisplayFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateTakeScreenshotForDisplayFunction::Run() {
  std::optional<api::autotest_private::TakeScreenshotForDisplay::Params>
      params = api::autotest_private::TakeScreenshotForDisplay::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateTakeScreenshotForDisplayFunction "
           << params->display_id;
  int64_t target_display_id;
  base::StringToInt64(params->display_id, &target_display_id);
  auto grabber = std::make_unique<ui::ScreenshotGrabber>();

  for (aura::Window* const window : ash::Shell::GetAllRootWindows()) {
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
  Respond(WithArguments(GetPngDataAsString(png_data)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetPrinterListFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetPrinterListFunction::AutotestPrivateGetPrinterListFunction() =
    default;

AutotestPrivateGetPrinterListFunction::
    ~AutotestPrivateGetPrinterListFunction() {
  DCHECK(!printers_manager_);
}

ExtensionFunction::ResponseAction AutotestPrivateGetPrinterListFunction::Run() {
  // |printers_manager_| should be created on UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DVLOG(1) << "AutotestPrivateGetPrinterListFunction";

  Profile* profile = Profile::FromBrowserContext(browser_context());
  printers_manager_ = ash::CupsPrintersManager::Create(profile);
  printers_manager_->AddObserver(this);

  // Set up a timer to finish waiting after 10 seconds
  timeout_timer_.Start(
      FROM_HERE, base::Seconds(10),
      base::BindOnce(
          &AutotestPrivateGetPrinterListFunction::RespondWithTimeoutError,
          this));

  return RespondLater();
}

void AutotestPrivateGetPrinterListFunction::DestroyPrintersManager() {
  // |printers_manager_| should be destroyed on UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!printers_manager_) {
    return;
  }

  printers_manager_->RemoveObserver(this);
  printers_manager_.reset();
}

void AutotestPrivateGetPrinterListFunction::RespondWithTimeoutError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (did_respond()) {
    return;
  }

  DestroyPrintersManager();
  Respond(
      Error("Timeout occurred before Enterprise printers were initialized"));
}

void AutotestPrivateGetPrinterListFunction::RespondWithSuccess() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (did_respond()) {
    return;
  }

  timeout_timer_.AbandonAndStop();
  DestroyPrintersManager();
  Respond(WithArguments(std::move(results_)));
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
      results_.Append(base::Value::Dict()
                          .Set("printerName", printer.display_name())
                          .Set("printerId", printer.id())
                          .Set("printerType", GetPrinterType(type)));
    }
  }
  // We have to respond in separate task on the same thread, because it will
  // cause a destruction of CupsPrintersManager which needs to happen after
  // we return and on the same thread.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
  std::optional<api::autotest_private::UpdatePrinter::Params> params =
      api::autotest_private::UpdatePrinter::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateUpdatePrinterFunction";

  const api::autotest_private::Printer& js_printer = params->printer;
  chromeos::Printer printer(js_printer.printer_id ? *js_printer.printer_id
                                                  : "");
  printer.set_display_name(js_printer.printer_name);
  if (js_printer.printer_desc) {
    printer.set_description(*js_printer.printer_desc);
  }

  if (js_printer.printer_make_and_model) {
    printer.set_make_and_model(*js_printer.printer_make_and_model);
  }

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
    if (ppd.is_valid()) {
      printer.mutable_ppd_reference()->user_supplied_ppd_url = ppd.spec();
    } else {
      LOG(ERROR) << "Invalid ppd path: " << *js_printer.printer_ppd;
    }
  }
  auto* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(browser_context());
  printers_manager->SavePrinter(printer);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRemovePrinterFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRemovePrinterFunction::~AutotestPrivateRemovePrinterFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateRemovePrinterFunction::Run() {
  std::optional<api::autotest_private::RemovePrinter::Params> params =
      api::autotest_private::RemovePrinter::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateRemovePrinterFunction " << params->printer_id;

  auto* printers_manager =
      ash::CupsPrintersManagerFactory::GetForBrowserContext(browser_context());
  printers_manager->RemoveSavedPrinter(params->printer_id);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateLoadSmartDimComponentFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateLoadSmartDimComponentFunction::
    AutotestPrivateLoadSmartDimComponentFunction() = default;
AutotestPrivateLoadSmartDimComponentFunction::
    ~AutotestPrivateLoadSmartDimComponentFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateLoadSmartDimComponentFunction::Run() {
  DVLOG(1) << "AutotestPrivateLoadSmartDimComponentFunction";

  if (ash::power::ml::SmartDimMlAgent::GetInstance()->IsDownloadWorkerReady()) {
    return RespondNow(NoArguments());
  }

  const std::string crx_id =
      component_updater::SmartDimComponentInstallerPolicy::GetExtensionId();
  g_browser_process->component_updater()->GetOnDemandUpdater().OnDemandUpdate(
      crx_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce(&AutotestPrivateLoadSmartDimComponentFunction::
                         OnComponentUpdatedCallback,
                     this));

  timer_.Start(
      FROM_HERE, base::Seconds(5),
      base::BindRepeating(
          &AutotestPrivateLoadSmartDimComponentFunction::TryRespond, this));

  return RespondLater();
}

void AutotestPrivateLoadSmartDimComponentFunction::OnComponentUpdatedCallback(
    update_client::Error error) {
  if (error != update_client::Error::NONE &&
      error != update_client::Error::UPDATE_IN_PROGRESS) {
    Respond(Error(base::StringPrintf(
        "On demand update of the SmartDim component failed with error: %d.",
        static_cast<int>(error))));
  }
}

void AutotestPrivateLoadSmartDimComponentFunction::TryRespond() {
  ++timer_triggered_count_;
  if (did_respond()) {
    return;
  }

  if (ash::power::ml::SmartDimMlAgent::GetInstance()->IsDownloadWorkerReady()) {
    Respond(NoArguments());
  } else if (timer_triggered_count_ >= 48 /* 48 * 5 sec == 4 minutes */) {
    Respond(Error("Timeout occurred before SmartDim component was loaded."));
  } else {
    timer_.Reset();
  }
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

  std::optional<api::autotest_private::SetAssistantEnabled::Params> params =
      api::autotest_private::SetAssistantEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const std::string& err_msg =
      SetAllowedPref(profile, ash::assistant::prefs::kAssistantEnabled,
                     base::Value(params->enabled));
  if (!err_msg.empty()) {
    return RespondNow(Error(err_msg));
  }

  // Any state that's not |NOT_READY| would be considered a ready state.
  const bool not_ready = (ash::AssistantState::Get()->assistant_status() ==
                          ash::assistant::AssistantStatus::NOT_READY);
  const bool success = (params->enabled != not_ready);
  if (success) {
    return RespondNow(NoArguments());
  }

  // Assistant service has not responded yet, set up a delayed timer to wait for
  // it and holder a reference to |this|. Also make sure we stop and respond
  // when timeout.
  enabled_ = params->enabled;
  timeout_timer_.Start(
      FROM_HERE, base::Milliseconds(params->timeout_ms),
      base::BindOnce(&AutotestPrivateSetAssistantEnabledFunction::Timeout,
                     this));
  return RespondLater();
}

void AutotestPrivateSetAssistantEnabledFunction::OnAssistantStatusChanged(
    ash::assistant::AssistantStatus status) {
  // Must check if the Optional contains value first to avoid possible
  // segmentation fault caused by Respond() below being called before
  // RespondLater() in Run(). This will happen due to AddObserver() call
  // in the constructor will trigger this function immediately.
  if (!enabled_.has_value()) {
    return;
  }

  const bool not_ready = (status == ash::assistant::AssistantStatus::NOT_READY);
  const bool success = (enabled_.value() != not_ready);
  if (!success) {
    return;
  }

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
    ~AutotestPrivateEnableAssistantAndWaitForReadyFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateEnableAssistantAndWaitForReadyFunction::Run() {
  DVLOG(1) << "AutotestPrivateEnableAssistantAndWaitForReadyFunction";

  if (ash::AssistantState::Get()->assistant_status() ==
      ash::assistant::AssistantStatus::READY) {
    return RespondNow(Error("Assistant is already enabled."));
  }

  // We can set this callback only when assistant status is NOT_READY. We should
  // call this before we try to enable Assistant to avoid causing some timing
  // issue.
  ash::assistant::AssistantManagerServiceImpl::
      SetInitializedInternalCallbackForTesting(base::BindOnce(
          &AutotestPrivateEnableAssistantAndWaitForReadyFunction::
              OnInitializedInternal,
          this));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const std::string& err_msg = SetAllowedPref(
      profile, ash::assistant::prefs::kAssistantEnabled, base::Value(true));
  if (!err_msg.empty()) {
    return RespondNow(Error(err_msg));
  }

  return RespondLater();
}

void AutotestPrivateEnableAssistantAndWaitForReadyFunction::
    OnInitializedInternal() {
  Respond(NoArguments());
}

// AssistantInteractionHelper is a helper class used to interact with Assistant
// server and store interaction states for tests. It is shared by
// |AutotestPrivateSendAssistantTextQueryFunction| and
// |AutotestPrivateWaitForAssistantQueryStatusFunction|.
class AssistantInteractionHelper
    : public ash::assistant::AssistantInteractionSubscriber {
 public:
  using OnInteractionFinishedCallback =
      base::OnceCallback<void(const std::optional<std::string>& error)>;

  AssistantInteractionHelper() = default;

  AssistantInteractionHelper(const AssistantInteractionHelper&) = delete;
  AssistantInteractionHelper& operator=(const AssistantInteractionHelper&) =
      delete;

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
        query, ash::assistant::AssistantQuerySource::kUnspecified, allow_tts);

    query_status_.Set("queryText", query);
  }

  base::Value::Dict GetQueryStatus() { return std::move(query_status_); }

  ash::assistant::Assistant* GetAssistant() {
    auto* assistant_service = ash::assistant::AssistantService::Get();
    return assistant_service ? assistant_service->GetAssistant() : nullptr;
  }

 private:
  // ash::assistant::AssistantInteractionSubscriber:
  using AssistantSuggestion = ash::assistant::AssistantSuggestion;
  using AssistantInteractionMetadata =
      ash::assistant::AssistantInteractionMetadata;
  using AssistantInteractionResolution =
      ash::assistant::AssistantInteractionResolution;

  void OnInteractionStarted(
      const AssistantInteractionMetadata& metadata) override {
    const bool is_voice_interaction =
        ash::assistant::AssistantInteractionType::kVoice == metadata.type;
    query_status_.Set("isMicOpen", is_voice_interaction);
    interaction_in_progress_ = true;
  }

  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override {
    // If you send an Assistant text query while another query is already
    // running, OnInteractionFinished can be called for it. We have to subscribe
    // assistant interactions before sending a text query as it can trigger
    // OnInteractionStarted.
    //
    // e.g.
    // 1. autotestPrivate.sendAssistantTextQuery("your query", ...).
    // 2. AutoTestPrivate starts listening Assistant interactions.
    // 3. AutoTestPrivate sends the text query to Assistant.
    // 4. Assistant cancels the on-going query. -> OnInteractionFinished
    // 5. Assistant starts the new query. -> OnInteractionStarted
    if (!interaction_in_progress_) {
      DVLOG(1) << "Ignoring an uninterested OnInteractionFinished call";
      return;
    }

    interaction_in_progress_ = false;

    CHECK(on_interaction_finished_callback_)
        << "on_interaction_finished_callback_ is not set.";

    if (resolution == AssistantInteractionResolution::kError) {
      SendErrorResponse(
          base::StringPrintf("Interaction closed with resolution %s",
                             ResolutionToString(resolution).c_str()));
      return;
    }

    // Only invoke the callback when |result_| is not empty to avoid an early
    // return before the entire session is completed. This happens when
    // sending queries to modify device settings, e.g. "turn on bluetooth",
    // which results in a round trip due to the need to fetch device state
    // on the client and return that to the server as part of a follow-up
    // interaction.
    if (result_.empty()) {
      return;
    }

    if (resolution != AssistantInteractionResolution::kNormal) {
      SendErrorResponse(
          base::StringPrintf("Interaction closed with resolution %s",
                             ResolutionToString(resolution).c_str()));
      return;
    }

    query_status_.Set("queryResponse", std::move(result_));
    SendSuccessResponse();
  }

  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override {
    result_.Set("htmlResponse", response);
    CheckResponseIsValid(__FUNCTION__);
  }

  void OnTextResponse(const std::string& response) override {
    result_.Set("text", response);
    CheckResponseIsValid(__FUNCTION__);
  }

  void OnOpenUrlResponse(const ::GURL& url, bool in_background) override {
    result_.Set("openUrl", url.possibly_invalid_spec());
  }

  void OnOpenAppResponse(
      const ash::assistant::AndroidAppInfo& app_info) override {
    result_.Set("openAppResponse", app_info.package_name);
    CheckResponseIsValid(__FUNCTION__);
  }

  void OnSpeechRecognitionFinalResult(
      const std::string& final_result) override {
    query_status_.Set("queryText", final_result);
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
    std::move(on_interaction_finished_callback_).Run(std::nullopt);
  }

  void SendErrorResponse(const std::string& error) {
    std::move(on_interaction_finished_callback_).Run(error);
  }

  base::Value::Dict query_status_;
  base::Value::Dict result_;
  bool interaction_in_progress_ = false;

  // Callback triggered when interaction finished with non-empty response.
  OnInteractionFinishedCallback on_interaction_finished_callback_;
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

  std::optional<api::autotest_private::SendAssistantTextQuery::Params> params =
      api::autotest_private::SendAssistantTextQuery::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  ash::assistant::AssistantAllowedState allowed_state =
      assistant::IsAssistantAllowedForProfile(profile);
  if (allowed_state != ash::assistant::AssistantAllowedState::ALLOWED) {
    return RespondNow(Error(base::StringPrintf(
        "Assistant not allowed - state: %d", allowed_state)));
  }

  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  if (session_state != session_manager::SessionState::ACTIVE) {
    // tast side code matches with this error string, i.e. update both if you
    // change this.
    return RespondNow(
        Error("Session state must be ACTIVE to send a text query. Session "
              "state was *",
              ToString(session_state)));
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
      FROM_HERE, base::Milliseconds(params->timeout_ms),
      base::BindOnce(&AutotestPrivateSendAssistantTextQueryFunction::Timeout,
                     this));

  return RespondLater();
}

void AutotestPrivateSendAssistantTextQueryFunction::
    OnInteractionFinishedCallback(const std::optional<std::string>& error) {
  DCHECK(!did_respond());
  if (error) {
    Respond(Error(error.value()));
  } else {
    Respond(WithArguments(interaction_helper_->GetQueryStatus()));
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

std::string AutotestPrivateSendAssistantTextQueryFunction::ToString(
    session_manager::SessionState session_state) {
  switch (session_state) {
    case session_manager::SessionState::UNKNOWN:
      return "UNKNOWN";
    case session_manager::SessionState::OOBE:
      return "OOBE";
    case session_manager::SessionState::LOGIN_PRIMARY:
      return "LOGIN_PRIMARY";
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
      return "LOGGED_IN_NOT_ACTIVE";
    case session_manager::SessionState::ACTIVE:
      return "ACTIVE";
    case session_manager::SessionState::LOCKED:
      return "LOCKED";
    case session_manager::SessionState::LOGIN_SECONDARY:
      return "LOGIN_SECONDARY";
    case session_manager::SessionState::RMA:
      return "RMA";
  }
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

  std::optional<api::autotest_private::WaitForAssistantQueryStatus::Params>
      params =
          api::autotest_private::WaitForAssistantQueryStatus::Params::Create(
              args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  ash::assistant::AssistantAllowedState allowed_state =
      assistant::IsAssistantAllowedForProfile(profile);
  if (allowed_state != ash::assistant::AssistantAllowedState::ALLOWED) {
    return RespondNow(Error(base::StringPrintf(
        "Assistant not allowed - state: %d", allowed_state)));
  }

  interaction_helper_->Init(
      base::BindOnce(&AutotestPrivateWaitForAssistantQueryStatusFunction::
                         OnInteractionFinishedCallback,
                     this));

  // Start waiting for the response before time out.
  timeout_timer_.Start(
      FROM_HERE, base::Seconds(params->timeout_s),
      base::BindOnce(
          &AutotestPrivateWaitForAssistantQueryStatusFunction::Timeout, this));
  return RespondLater();
}

void AutotestPrivateWaitForAssistantQueryStatusFunction::
    OnInteractionFinishedCallback(const std::optional<std::string>& error) {
  DCHECK(!did_respond());
  if (error) {
    Respond(Error(error.value()));
  } else {
    Respond(WithArguments(interaction_helper_->GetQueryStatus()));
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

  return RespondNow(WithArguments(prefs->package_list_initial_refreshed()));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetAllowedPrefFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetAllowedPrefFunction::
    ~AutotestPrivateSetAllowedPrefFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateSetAllowedPrefFunction::Run() {
  DVLOG(1) << "AutotestPrivateSetAllowedPrefFunction";

  std::optional<api::autotest_private::SetAllowedPref::Params> params =
      api::autotest_private::SetAllowedPref::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& pref_name = params->pref_name;
  const base::Value& value = params->value;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const std::string& err_msg = SetAllowedPref(profile, pref_name, value);

  if (!err_msg.empty()) {
    return RespondNow(Error(err_msg));
  }

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateClearAllowedPrefFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateClearAllowedPrefFunction::
    ~AutotestPrivateClearAllowedPrefFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateClearAllowedPrefFunction::Run() {
  std::optional<api::autotest_private::ClearAllowedPref::Params> params =
      api::autotest_private::ClearAllowedPref::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!ClearAllowedPref(Profile::FromBrowserContext(browser_context()),
                        params->pref_name)) {
    return RespondNow(
        Error("Cannot clear pref absent in allowlist: " + params->pref_name));
  }
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetWhitelistedPrefFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetWhitelistedPrefFunction::
    ~AutotestPrivateSetWhitelistedPrefFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetWhitelistedPrefFunction::Run() {
  DVLOG(1) << "AutotestPrivateSetWhitelistedPrefFunction";

  std::optional<api::autotest_private::SetAllowedPref::Params> params =
      api::autotest_private::SetAllowedPref::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string& pref_name = params->pref_name;
  const base::Value& value = params->value;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  const std::string& err_msg = SetAllowedPref(profile, pref_name, value);

  if (!err_msg.empty()) {
    return RespondNow(Error(err_msg));
  }

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetCrostiniAppScaledFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetCrostiniAppScaledFunction::
    ~AutotestPrivateSetCrostiniAppScaledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetCrostiniAppScaledFunction::Run() {
  std::optional<api::autotest_private::SetCrostiniAppScaled::Params> params =
      api::autotest_private::SetCrostiniAppScaled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetCrostiniAppScaledFunction " << params->app_id
           << " " << params->scaled;

  ChromeShelfController* const controller = ChromeShelfController::instance();
  if (!controller) {
    return RespondNow(Error("Controller not available"));
  }

  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(
          controller->profile());
  if (!registry_service) {
    return RespondNow(Error("Crostini registry not available"));
  }

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
  return RespondNow(WithArguments(scale_factor));
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
      WithArguments(display::Screen::GetScreen()->InTabletMode()));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetTabletModeEnabledFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetTabletModeEnabledFunction::
    ~AutotestPrivateSetTabletModeEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetTabletModeEnabledFunction::Run() {
  DVLOG(1) << "AutotestPrivateSetTabletModeEnabledFunction";

  std::optional<api::autotest_private::SetTabletModeEnabled::Params> params =
      api::autotest_private::SetTabletModeEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (display::Screen::GetScreen()->InTabletMode() == params->enabled) {
    return RespondNow(
        WithArguments(display::Screen::GetScreen()->InTabletMode()));
  }

  ash::TabletMode::Waiter waiter(params->enabled);
  if (!ash::TabletMode::Get()->ForceUiTabletModeState(params->enabled)) {
    return RespondNow(Error("failed to switch the tablet mode state"));
  }
  waiter.Wait();
  return RespondNow(
      WithArguments(display::Screen::GetScreen()->InTabletMode()));
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
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  std::vector<api::autotest_private::App> installed_apps;
  proxy->AppRegistryCache().ForEachApp(
      [&installed_apps](const apps::AppUpdate& update) {
        if (!apps_util::IsInstalled(update.Readiness())) {
          return;
        }

        api::autotest_private::App app;
        app.app_id = update.AppId();

        // Assume that when `switches::kForceDirectionRTL` is enabled, the
        // system language still follows the left-to-right fashion. Because the
        // app names carried by `update` are adapted to RTL by inserting extra
        // characters that indicate the text direction, we should recover the
        // original app names before returning them as the result.
        if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                switches::kForceUIDirection) == switches::kForceDirectionRTL) {
          std::u16string name = base::UTF8ToUTF16(update.Name());
          base::i18n::UnadjustStringForLocaleDirection(&name);
          app.name = base::UTF16ToUTF8(name);
        } else {
          app.name = update.Name();
        }

        app.short_name = update.ShortName();
        app.publisher_id = update.PublisherId();
        app.additional_search_terms = update.AdditionalSearchTerms();
        app.type = GetAppType(update.AppType());
        app.install_source = GetAppInstallSource(update.InstallReason());
        app.readiness = GetAppReadiness(update.Readiness());
        app.show_in_launcher = update.ShowInLauncher();
        app.show_in_search = update.ShowInSearch();
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

  ChromeShelfController* const controller = ChromeShelfController::instance();
  if (!controller) {
    return RespondNow(Error("Controller not available"));
  }

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
    result_item.pin_state_forced_by_type = item.pin_state_forced_by_type;
    result_item.has_notification = item.has_notification;
    result_items.emplace_back(std::move(result_item));
  }

  return RespondNow(ArgumentList(
      api::autotest_private::GetShelfItems::Results::Create(result_items)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetLauncherSearchBoxStateFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetLauncherSearchBoxStateFunction::
    AutotestPrivateGetLauncherSearchBoxStateFunction() = default;

AutotestPrivateGetLauncherSearchBoxStateFunction::
    ~AutotestPrivateGetLauncherSearchBoxStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetLauncherSearchBoxStateFunction::Run() {
  DVLOG(1) << "AutotestPrivateGetLauncherSearchBoxStateFunction";

  api::autotest_private::LauncherSearchBoxState launcher_search_box_state;
  launcher_search_box_state.ghost_text = ash::GetSearchBoxGhostTextForTest();

  return RespondNow(WithArguments(launcher_search_box_state.ToValue()));
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

  std::optional<api::autotest_private::GetShelfAutoHideBehavior::Params>
      params = api::autotest_private::GetShelfAutoHideBehavior::Params::Create(
          args());
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
  return RespondNow(WithArguments(std::move(str_behavior)));
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

  std::optional<api::autotest_private::SetShelfAutoHideBehavior::Params>
      params = api::autotest_private::SetShelfAutoHideBehavior::Params::Create(
          args());
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

  std::optional<api::autotest_private::GetShelfAlignment::Params> params =
      api::autotest_private::GetShelfAlignment::Params::Create(args());
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
      alignment_type = api::autotest_private::ShelfAlignmentType::kBottom;
      break;
    case ash::ShelfAlignment::kLeft:
      alignment_type = api::autotest_private::ShelfAlignmentType::kLeft;
      break;
    case ash::ShelfAlignment::kRight:
      alignment_type = api::autotest_private::ShelfAlignmentType::kRight;
      break;
    case ash::ShelfAlignment::kBottomLocked:
      // ShelfAlignment::kBottomLocked not supported by
      // shelf_prefs.cc
      return RespondNow(Error("ShelfAlignment::kBottomLocked not supported"));
  }
  return RespondNow(
      WithArguments(api::autotest_private::ToString(alignment_type)));
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

  std::optional<api::autotest_private::SetShelfAlignment::Params> params =
      api::autotest_private::SetShelfAlignment::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::ShelfAlignment alignment;
  switch (params->alignment) {
    case api::autotest_private::ShelfAlignmentType::kBottom:
      alignment = ash::ShelfAlignment::kBottom;
      break;
    case api::autotest_private::ShelfAlignmentType::kLeft:
      alignment = ash::ShelfAlignment::kLeft;
      break;
    case api::autotest_private::ShelfAlignmentType::kRight:
      alignment = ash::ShelfAlignment::kRight;
      break;
    case api::autotest_private::ShelfAlignmentType::kNone:
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
  std::optional<api::autotest_private::WaitForOverviewState::Params> params =
      api::autotest_private::WaitForOverviewState::Params::Create(args());
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
// AutotestPrivateSendArcOverlayColorFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSendArcOverlayColorFunction::
    ~AutotestPrivateSendArcOverlayColorFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSendArcOverlayColorFunction::Run() {
  DVLOG(1) << "AutotestPrivateSendArcOverlayColorFunction";
  std::optional<api::autotest_private::SendArcOverlayColor::Params> params =
      api::autotest_private::SendArcOverlayColor::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  arc::ArcSystemUIBridge* const system_ui =
      arc::ArcSystemUIBridge::GetForBrowserContext(browser_context());
  if (!system_ui) {
    return RespondNow(Error("No ARC System UI Bridge is available."));
  }
  const bool result = system_ui->SendOverlayColor(
      params->color, ToThemeStyleType(params->theme));
  return RespondNow(WithArguments(result));
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
  std::optional<api::autotest_private::SetOverviewModeState::Params> params =
      api::autotest_private::SetOverviewModeState::Params::Create(args());
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
  auto arg = WithArguments(finished);
  // On starting the overview animation, it needs to wait for 1 extra second
  // to trigger the occlusion tracker.
  if (for_start) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AutotestPrivateSetOverviewModeStateFunction::Respond,
                       this, std::move(arg)),
        base::Seconds(1));
  } else {
    Respond(std::move(arg));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetDefaultPinnedAppIdsFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetDefaultPinnedAppIdsFunction::
    AutotestPrivateGetDefaultPinnedAppIdsFunction() = default;

AutotestPrivateGetDefaultPinnedAppIdsFunction::
    ~AutotestPrivateGetDefaultPinnedAppIdsFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetDefaultPinnedAppIdsFunction::Run() {
  std::vector<std::string> default_pinned_app_ids;
  for (const char* default_app_id :
       GetDefaultPinnedAppsForFormFactor(browser_context())) {
    default_pinned_app_ids.emplace_back(default_app_id);
  }

  return RespondNow(ArgumentList(
      api::autotest_private::GetDefaultPinnedAppIds::Results::Create(
          default_pinned_app_ids)));
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
  if (!ash::IMEBridge::Get() ||
      !ash::IMEBridge::Get()->GetInputContextHandler() ||
      !ash::IMEBridge::Get()->GetInputContextHandler()->GetInputMethod()) {
    return RespondNow(NoArguments());
  }

  ash::IMEBridge::Get()
      ->GetInputContextHandler()
      ->GetInputMethod()
      ->SetVirtualKeyboardVisibilityIfEnabled(true);
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
  if (!tracing) {
    return RespondNow(Error("No ARC performance tracing is available."));
  }

  if (!tracing->StartCustomTracing()) {
    return RespondNow(Error("Failed to start custom tracing."));
  }

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
  if (!tracing) {
    return RespondNow(Error("No ARC performance tracing is available."));
  }

  return RespondNow(WithArguments(tracing->StopCustomTracing()));
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
  std::optional<api::autotest_private::SetArcAppWindowFocus::Params> params =
      api::autotest_private::SetArcAppWindowFocus::Params::Create(args());
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

  std::optional<api::autotest_private::WaitForDisplayRotation::Params> params =
      api::autotest_private::WaitForDisplayRotation::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  if (!base::StringToInt64(params->display_id, &display_id_)) {
    return RespondNow(Error(base::StrCat(
        {"Invalid display_id; expected string with numbers only, got ",
         params->display_id})));
  }

  if (params->rotation == api::autotest_private::RotationType::kRotateAny) {
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
  if (result) {
    return RespondNow(std::move(*result));
  }
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
  Respond(WithArguments(display.is_valid() &&
                        (!target_rotation_.has_value() ||
                         display.rotation() == *target_rotation_)));
  self_.reset();
}

void AutotestPrivateWaitForDisplayRotationFunction::
    OnUserRotationLockChanged() {
  auto* screen_orientation_controller =
      ash::Shell::Get()->screen_orientation_controller();
  if (screen_orientation_controller->user_rotation_locked()) {
    return;
  }
  screen_orientation_controller->RemoveObserver(this);
  self_.reset();
  target_rotation_.reset();
  auto result = CheckScreenRotationAnimation();
  // Wait for the rotation if unlocking causes rotation.
  if (result) {
    Respond(std::move(*result));
  }
}

std::optional<ExtensionFunction::ResponseValue>
AutotestPrivateWaitForDisplayRotationFunction::CheckScreenRotationAnimation() {
  auto* root_controller =
      ash::Shell::GetRootWindowControllerWithDisplayId(display_id_);
  if (!root_controller || !root_controller->GetScreenRotationAnimator()) {
    return Error(base::StringPrintf(
        "Invalid display_id; no root window found for the display id %" PRId64,
        display_id_));
  }
  auto* animator = root_controller->GetScreenRotationAnimator();
  if (!animator->IsRotating()) {
    display::Display display;
    display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id_,
                                                          &display);
    // This should never fail.
    DCHECK(display.is_valid());
    return WithArguments(!target_rotation_.has_value() ||
                         display.rotation() == *target_rotation_);
  }
  self_ = this;

  animator->AddObserver(this);
  return std::nullopt;
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

  std::optional<ash::OverviewInfo> overview_info =
      ash::OverviewTestApi().GetOverviewInfo();

  auto window_list = ash::GetAppWindowList();
  std::vector<api::autotest_private::AppWindowInfo> result_list;

  for (aura::Window* window : window_list) {
    if (window->GetId() == aura::Window::kInitialId) {
      window->SetId(id_count--);
    }
    api::autotest_private::AppWindowInfo window_info;
    window_info.id = window->GetId();
    window_info.name = window->GetName();
    window_info.window_type =
        GetAppWindowType(window->GetProperty(chromeos::kAppTypeKey));
    window_info.state_type =
        ToWindowStateType(window->GetProperty(chromeos::kWindowStateTypeKey));
    window_info.bounds_in_root =
        ToBoundsDictionary(window->GetBoundsInRootWindow());
    window_info.target_bounds = ToBoundsDictionary(window->GetTargetBounds());
    window_info.display_id = base::NumberToString(
        display::Screen::GetScreen()->GetDisplayNearestWindow(window).id());
    window_info.title = base::UTF16ToUTF8(window->GetTitle());
    // Check for window hiding animations separately because they pertain to
    // layers detached from the window.
    window_info.is_animating =
        window->layer()->GetAnimator()->is_animating() ||
        window->GetProperty(wm::kWindowHidingAnimationCountKey) > 0;
    window_info.is_visible = window->IsVisible();
    window_info.target_visibility = window->TargetVisibility();
    window_info.can_focus = window->CanFocus();
    window_info.has_focus = window->HasFocus();
    window_info.on_active_desk =
        chromeos::DesksHelper::Get(window)->BelongsToActiveDesk(window);
    window_info.is_active = wm::IsActiveWindow(window);
    window_info.has_capture = window->HasCapture();
    window_info.can_resize =
        (window->GetProperty(aura::client::kResizeBehaviorKey) &
         aura::client::kResizeBehaviorCanResize) != 0;

    window_info.stacking_order = -1;
    // Find the window's stacking order among its siblings.
    if (auto* parent = window->parent()) {
      const auto& children = parent->children();
      auto it = std::find(children.rbegin(), children.rend(), window);
      if (it != children.rend()) {
        window_info.stacking_order = it - children.rbegin();
      }
    }

    if (window->GetProperty(chromeos::kAppTypeKey) ==
        chromeos::AppType::ARC_APP) {
      std::string* package_name = window->GetProperty(ash::kArcPackageNameKey);
      if (package_name) {
        window_info.arc_package_name = *package_name;
      } else {
        LOG(ERROR) << "The package name for window " << window->GetTitle()
                   << " (ID: " << window->GetId()
                   << ") isn't available even though it is an ARC window.";
      }
    }
    std::string* full_restore_window_app_id =
        window->GetProperty(app_restore::kAppIdKey);
    if (full_restore_window_app_id) {
      window_info.full_restore_window_app_id = *full_restore_window_app_id;
    }
    std::string* app_id = window->GetProperty(ash::kAppIDKey);
    if (app_id) {
      window_info.app_id = *app_id;
    }

    auto* widget = views::Widget::GetWidgetForNativeWindow(window);
    // Frame information
    auto* immersive_controller =
        chromeos::ImmersiveFullscreenController::Get(widget);

    // The widget that hosts the immersive frame can be different from the
    // application's widget itself. Use the widget from the immersive
    // controller to obtain the FrameHeader.
    if (immersive_controller) {
      widget = immersive_controller->widget();
    }

    if (immersive_controller && immersive_controller->IsEnabled()) {
      window_info.frame_mode = api::autotest_private::FrameMode::kImmersive;
      window_info.is_frame_visible = immersive_controller->IsRevealed();
    } else {
      window_info.frame_mode = api::autotest_private::FrameMode::kNormal;
      window_info.is_frame_visible = IsFrameVisible(widget);
    }

    auto* frame_header = chromeos::FrameHeader::Get(widget);
    if (frame_header) {
      window_info.caption_height = frame_header->GetHeaderHeight();

      const chromeos::CaptionButtonModel* button_model =
          frame_header->GetCaptionButtonModel();
      int caption_button_enabled_status = 0;
      int caption_button_visible_status = 0;

      constexpr views::CaptionButtonIcon all_button_icons[] = {
          views::CAPTION_BUTTON_ICON_MINIMIZE,
          views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
          views::CAPTION_BUTTON_ICON_CLOSE,
          views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
          views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
          views::CAPTION_BUTTON_ICON_BACK,
          views::CAPTION_BUTTON_ICON_LOCATION,
          views::CAPTION_BUTTON_ICON_MENU,
          views::CAPTION_BUTTON_ICON_ZOOM};

      for (const auto button : all_button_icons) {
        if (button_model->IsEnabled(button)) {
          caption_button_enabled_status |= (1 << button);
        }
        if (button_model->IsVisible(button)) {
          caption_button_visible_status |= (1 << button);
        }
      }
      window_info.caption_button_enabled_status = caption_button_enabled_status;
      window_info.caption_button_visible_status = caption_button_visible_status;
    } else {
      auto* no_frame_header_widget =
          views::Widget::GetWidgetForNativeWindow(window);
      // All widgets for app windows in chromeos should have a frame. Non app
      // windows may not have a frame and frame mode will be NONE.
      DCHECK(!no_frame_header_widget ||
             no_frame_header_widget->GetNativeWindow()->GetType() !=
                 aura::client::WINDOW_TYPE_NORMAL);
      window_info.frame_mode = api::autotest_private::FrameMode::kNone;
      window_info.is_frame_visible = false;
    }

    // Overview info.
    if (overview_info.has_value()) {
      auto it = overview_info->find(window);
      if (it != overview_info->end()) {
        window_info.overview_info.emplace();
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
  std::optional<api::autotest_private::SetAppWindowState::Params> params =
      api::autotest_private::SetAppWindowState::Params::Create(args());
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
      return RespondNow(WithArguments(
          api::autotest_private::ToString(ToWindowStateType(expected_state))));
    }
  }

  const bool wait = params->wait && *params->wait;

  if (wait) {
    window_state_observer_ = std::make_unique<WindowStateChangeObserver>(
        window, expected_state,
        base::BindOnce(
            &AutotestPrivateSetAppWindowStateFunction::WindowStateChanged, this,
            expected_state));
  }

  if (params->change.event_type ==
          api::autotest_private::WMEventType::kWmeventSnapPrimary ||
      params->change.event_type ==
          api::autotest_private::WMEventType::kWmeventSnapSecondary) {
    const ash::WindowSnapWMEvent event(
        ToWMEventType(params->change.event_type));
    ash::WindowState::Get(window)->OnWMEvent(&event);
  } else {
    const ash::WMEvent event(ToWMEventType(params->change.event_type));
    ash::WindowState::Get(window)->OnWMEvent(&event);
  }

  if (!wait) {
    return RespondNow(WithArguments(
        api::autotest_private::ToString(ToWindowStateType(expected_state))));
  }

  return RespondLater();
}

void AutotestPrivateSetAppWindowStateFunction::WindowStateChanged(
    chromeos::WindowStateType expected_type,
    bool success) {
  if (!success) {
    Respond(Error(
        "The app window was destroyed while waiting for its state change! "));
  } else {
    Respond(WithArguments(
        api::autotest_private::ToString(ToWindowStateType(expected_type))));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateActivateAppWindowFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateActivateAppWindowFunction::
    ~AutotestPrivateActivateAppWindowFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateActivateAppWindowFunction::Run() {
  std::optional<api::autotest_private::ActivateAppWindow::Params> params =
      api::autotest_private::ActivateAppWindow::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateActivateAppWindowFunction " << params->id;

  auto* window = FindAppWindowById(params->id);
  if (!window) {
    return RespondNow(Error(
        base::StringPrintf("No app window was found : id=%d", params->id)));
  }
  ash::WindowState::Get(window)->Activate();

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateCloseAppWindowFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateCloseAppWindowFunction::
    ~AutotestPrivateCloseAppWindowFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateCloseAppWindowFunction::Run() {
  std::optional<api::autotest_private::CloseAppWindow::Params> params =
      api::autotest_private::CloseAppWindow::Params::Create(args());
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

// Used to notify when when a certain URL contains a PWA.
class AutotestPrivateInstallPWAForCurrentURLFunction::PWABannerObserver
    : public webapps::AppBannerManager::Observer {
 public:
  PWABannerObserver(webapps::AppBannerManager* manager,
                    base::OnceCallback<void()> callback)
      : callback_(std::move(callback)), app_banner_manager_(manager) {
    DCHECK(manager);
    observation_.Observe(manager);

    // If PWA is already loaded, call callback immediately.
    Installable installable =
        app_banner_manager_->GetInstallableWebAppCheckResult();
    if (installable == Installable::kYes_Promotable ||
        installable == Installable::kYes_ByUserRequest) {
      observation_.Reset();
      std::move(callback_).Run();
    }
  }

  PWABannerObserver(const PWABannerObserver&) = delete;
  PWABannerObserver& operator=(const PWABannerObserver&) = delete;

  ~PWABannerObserver() override {}

  void OnInstallableWebAppStatusUpdated(
      webapps::InstallableWebAppCheckResult result,
      const std::optional<webapps::WebAppBannerData>& data) override {
    switch (result) {
      case Installable::kNo:
        [[fallthrough]];
      case Installable::kNo_AlreadyInstalled:
        [[fallthrough]];
      case Installable::kUnknown:
        DCHECK(false) << "Unexpected AppBannerManager::Installable value (kNo "
                         "or kNoAlreadyInstalled or kUnknown)";
        break;

      case Installable::kYes_Promotable:
        [[fallthrough]];
      case Installable::kYes_ByUserRequest:
        observation_.Reset();
        std::move(callback_).Run();
        break;
    }
  }

 private:
  using Installable = webapps::InstallableWebAppCheckResult;

  base::ScopedObservation<webapps::AppBannerManager,
                          webapps::AppBannerManager::Observer>
      observation_{this};
  base::OnceCallback<void()> callback_;
  raw_ptr<webapps::AppBannerManager> app_banner_manager_;
};

// Used to notify when a PWA is installed.
class AutotestPrivateInstallPWAForCurrentURLFunction::PWAInstallManagerObserver
    : public web_app::WebAppInstallManagerObserver {
 public:
  PWAInstallManagerObserver(
      Profile* profile,
      base::OnceCallback<void(const webapps::AppId&)> callback)
      : provider_(web_app::WebAppProvider::GetForWebApps(profile)),
        callback_(std::move(callback)) {
    if (!provider_) {
      return;
    }
    provider_->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&AutotestPrivateInstallPWAForCurrentURLFunction::
                           PWAInstallManagerObserver::OnProviderReady,
                       weak_factory_.GetWeakPtr()));
  }

  PWAInstallManagerObserver(const PWAInstallManagerObserver&) = delete;
  PWAInstallManagerObserver& operator=(const PWAInstallManagerObserver&) =
      delete;

  ~PWAInstallManagerObserver() override {}

  void OnProviderReady() {
    observation_.Observe(&provider_->install_manager());
  }

  void OnWebAppInstalled(const webapps::AppId& app_id) override {
    observation_.Reset();
    std::move(callback_).Run(app_id);
  }

  void OnWebAppInstallManagerDestroyed() override { observation_.Reset(); }

 private:
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      observation_{this};
  raw_ptr<web_app::WebAppProvider> provider_;
  base::OnceCallback<void(const webapps::AppId&)> callback_;
  base::WeakPtrFactory<
      AutotestPrivateInstallPWAForCurrentURLFunction::PWAInstallManagerObserver>
      weak_factory_{this};
};

AutotestPrivateInstallPWAForCurrentURLFunction::
    AutotestPrivateInstallPWAForCurrentURLFunction() = default;
AutotestPrivateInstallPWAForCurrentURLFunction::
    ~AutotestPrivateInstallPWAForCurrentURLFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateInstallPWAForCurrentURLFunction::Run() {
  DVLOG(1) << "AutotestPrivateInstallPWAForCurrentURLFunction";

  std::optional<api::autotest_private::InstallPWAForCurrentURL::Params> params =
      api::autotest_private::InstallPWAForCurrentURL::Params::Create(args());
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
      FROM_HERE, base::Milliseconds(params->timeout_ms),
      base::BindOnce(
          &AutotestPrivateInstallPWAForCurrentURLFunction::PWATimeout, this));
  return RespondLater();
}

void AutotestPrivateInstallPWAForCurrentURLFunction::PWALoaded() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  Browser* browser = GetFirstRegularBrowser();

  install_mananger_observer_ = std::make_unique<PWAInstallManagerObserver>(
      profile,
      base::BindOnce(
          &AutotestPrivateInstallPWAForCurrentURLFunction::PWAInstalled, this));

  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  if (!chrome::ExecuteCommand(browser, IDC_INSTALL_PWA)) {
    return Respond(Error("Failed to execute INSTALL_PWA command"));
  }
}

void AutotestPrivateInstallPWAForCurrentURLFunction::PWAInstalled(
    const webapps::AppId& app_id) {
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(false);
  Respond(WithArguments(app_id));
  timeout_timer_.AbandonAndStop();
}

void AutotestPrivateInstallPWAForCurrentURLFunction::PWATimeout() {
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(false);
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
  std::optional<api::autotest_private::ActivateAccelerator::Params> params =
      api::autotest_private::ActivateAccelerator::Params::Create(args());
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
  accelerator_controller->ApplyAcceleratorForTesting(accelerator);  // IN-TEST

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
    return RespondNow(WithArguments(result));
  }
  bool result = accelerator_controller->Process(accelerator);
  return RespondNow(WithArguments(result));
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
  std::optional<api::autotest_private::WaitForLauncherState::Params> params =
      api::autotest_private::WaitForLauncherState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  auto target_state = ToAppListViewState(params->launcher_state);
  // The method is only implemented for fullscreen launcher, for bubble
  // launcher, tests use automation APIs to wait for launcher visibility
  // changes.
  // Exceptionally, allow waiting for kClosed state in clamshell mode, so tests
  // can wait for fullscreen launcher state change to finish when exiting tablet
  // mode.
  if (!display::Screen::GetScreen()->InTabletMode() &&
      target_state != ash::AppListViewState::kClosed) {
    return RespondNow(Error("Not supported for bubble launcher"));
  }

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
  return RespondNow(WithArguments(success));
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
  std::optional<api::autotest_private::ActivateDeskAtIndex::Params> params =
      api::autotest_private::ActivateDeskAtIndex::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!ash::AutotestDesksApi().ActivateDeskAtIndex(
          params->index,
          base::BindOnce(
              &AutotestPrivateActivateDeskAtIndexFunction::OnAnimationComplete,
              this))) {
    return RespondNow(WithArguments(false));
  }

  return RespondLater();
}

void AutotestPrivateActivateDeskAtIndexFunction::OnAnimationComplete() {
  Respond(WithArguments(true));
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
  // Check whether overview mode was active before removing the desk. In case of
  // split view, the desk removal may cause overview to end, but what matters is
  // whether overview mode was active before.
  const bool was_in_overview =
      ash::OverviewController::Get()->InOverviewSession();

  if (!ash::AutotestDesksApi().RemoveActiveDesk(base::BindOnce(
          &AutotestPrivateRemoveActiveDeskFunction::OnAnimationComplete,
          this))) {
    return RespondNow(WithArguments(false));
  }

  // In overview, the desk removal animation does not apply, so we should not
  // wait for it.
  if (was_in_overview) {
    return RespondNow(WithArguments(true));
  }

  return RespondLater();
}

void AutotestPrivateRemoveActiveDeskFunction::OnAnimationComplete() {
  Respond(WithArguments(true));
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
  std::optional<
      api::autotest_private::ActivateAdjacentDesksToTargetIndex::Params>
      params = api::autotest_private::ActivateAdjacentDesksToTargetIndex::
          Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!ash::AutotestDesksApi().ActivateAdjacentDesksToTargetIndex(
          params->index,
          base::BindOnce(
              &AutotestPrivateActivateAdjacentDesksToTargetIndexFunction::
                  OnAnimationComplete,
              this))) {
    return RespondNow(WithArguments(false));
  }

  return RespondLater();
}

void AutotestPrivateActivateAdjacentDesksToTargetIndexFunction::
    OnAnimationComplete() {
  Respond(WithArguments(true));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetDeskCountFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetDeskCountFunction::AutotestPrivateGetDeskCountFunction() =
    default;
AutotestPrivateGetDeskCountFunction::~AutotestPrivateGetDeskCountFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateGetDeskCountFunction::Run() {
  return RespondNow(
      WithArguments(ash::AutotestDesksApi().GetDesksInfo().num_desks));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetDesksInfoFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetDesksInfoFunction::AutotestPrivateGetDesksInfoFunction() =
    default;
AutotestPrivateGetDesksInfoFunction::~AutotestPrivateGetDesksInfoFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateGetDesksInfoFunction::Run() {
  ash::AutotestDesksApi::DesksInfo desks_info =
      ash::AutotestDesksApi().GetDesksInfo();
  base::Value::Dict result;
  result.Set("activeDeskIndex", desks_info.active_desk_index);
  result.Set("numDesks", desks_info.num_desks);
  result.Set("isAnimating", desks_info.is_animating);

  base::Value::List desk_containers;
  for (std::string& desk_container : desks_info.desk_containers) {
    desk_containers.Append(std::move(desk_container));
  }
  result.Set("deskContainers", std::move(desk_containers));

  return RespondNow(WithArguments(std::move(result)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateMouseClickFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateMouseClickFunction::AutotestPrivateMouseClickFunction() =
    default;

AutotestPrivateMouseClickFunction::~AutotestPrivateMouseClickFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateMouseClickFunction::Run() {
  std::optional<api::autotest_private::MouseClick::Params> params =
      api::autotest_private::MouseClick::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* env = aura::Env::GetInstance();
  if (env->mouse_button_flags() != 0) {
    return RespondNow(Error(base::StringPrintf("Already pressed; flags %d",
                                               env->mouse_button_flags())));
  }

  int64_t display_id = ash::Shell::Get()->cursor_manager()->GetDisplay().id();
  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  if (!root_window) {
    return RespondNow(Error("Failed to find the root window"));
  }

  gfx::PointF location_in_host(env->last_mouse_location().x(),
                               env->last_mouse_location().y());
  wm::ConvertPointFromScreen(root_window, &location_in_host);
  ConvertPointToHost(root_window, &location_in_host);

  int flags = GetMouseEventFlags(params->button);
  event_generator_ = std::make_unique<EventGenerator>(
      root_window->GetHost(),
      base::BindOnce(&AutotestPrivateMouseClickFunction::Respond, this,
                     NoArguments()));
  event_generator_->ScheduleMouseEvent(ui::EventType::kMousePressed,
                                       location_in_host, flags);
  event_generator_->ScheduleMouseEvent(ui::EventType::kMouseReleased,
                                       location_in_host, flags);
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
  std::optional<api::autotest_private::MousePress::Params> params =
      api::autotest_private::MousePress::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* env = aura::Env::GetInstance();
  int input_flags = GetMouseEventFlags(params->button);
  if ((input_flags | env->mouse_button_flags()) == env->mouse_button_flags()) {
    return RespondNow(NoArguments());
  }

  int64_t display_id = ash::Shell::Get()->cursor_manager()->GetDisplay().id();
  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  if (!root_window) {
    return RespondNow(Error("Failed to find the root window"));
  }

  gfx::PointF location_in_host(env->last_mouse_location().x(),
                               env->last_mouse_location().y());
  wm::ConvertPointFromScreen(root_window, &location_in_host);
  ConvertPointToHost(root_window, &location_in_host);

  event_generator_ = std::make_unique<EventGenerator>(
      root_window->GetHost(),
      base::BindOnce(&AutotestPrivateMousePressFunction::Respond, this,
                     NoArguments()));
  event_generator_->ScheduleMouseEvent(ui::EventType::kMousePressed,
                                       location_in_host, input_flags);
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
  std::optional<api::autotest_private::MouseRelease::Params> params =
      api::autotest_private::MouseRelease::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* env = aura::Env::GetInstance();

  int input_flags = GetMouseEventFlags(params->button);
  if ((env->mouse_button_flags() & (~input_flags)) ==
      env->mouse_button_flags()) {
    return RespondNow(NoArguments());
  }

  int64_t display_id = ash::Shell::Get()->cursor_manager()->GetDisplay().id();
  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  if (!root_window) {
    return RespondNow(Error("Failed to find the root window"));
  }

  gfx::PointF location_in_host(env->last_mouse_location().x(),
                               env->last_mouse_location().y());
  wm::ConvertPointFromScreen(root_window, &location_in_host);
  ConvertPointToHost(root_window, &location_in_host);

  event_generator_ = std::make_unique<EventGenerator>(
      root_window->GetHost(),
      base::BindOnce(&AutotestPrivateMouseReleaseFunction::Respond, this,
                     NoArguments()));
  event_generator_->ScheduleMouseEvent(ui::EventType::kMouseReleased,
                                       location_in_host, input_flags);
  event_generator_->Run();

  return RespondLater();
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateMouseMoveFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateMouseMoveFunction::AutotestPrivateMouseMoveFunction() = default;
AutotestPrivateMouseMoveFunction::~AutotestPrivateMouseMoveFunction() = default;
ExtensionFunction::ResponseAction AutotestPrivateMouseMoveFunction::Run() {
  std::optional<api::autotest_private::MouseMove::Params> params =
      api::autotest_private::MouseMove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t display_id = ash::Shell::Get()->cursor_manager()->GetDisplay().id();
  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  if (!root_window) {
    return RespondNow(Error("Failed to find the root window"));
  }

  gfx::Point location_in_screen(params->location.x, params->location.y);
  auto* env = aura::Env::GetInstance();
  const gfx::Point last_mouse_location(env->last_mouse_location());
  if (last_mouse_location == location_in_screen) {
    return RespondNow(NoArguments());
  }

  event_generator_ = std::make_unique<EventGenerator>(
      root_window->GetHost(),
      base::BindOnce(&AutotestPrivateMouseMoveFunction::Respond, this,
                     NoArguments()));

  int64_t steps = std::max(
      base::ClampFloor<int64_t>(params->duration_in_ms /
                                event_generator_->interval().InMillisecondsF()),
      static_cast<int64_t>(1));
  int flags = env->mouse_button_flags();
  for (int64_t i = 1; i <= steps; ++i) {
    double progress = static_cast<double>(i) / static_cast<double>(steps);
    gfx::PointF point(
        gfx::Tween::FloatValueBetween(progress, last_mouse_location.x(),
                                      location_in_screen.x()),
        gfx::Tween::FloatValueBetween(progress, last_mouse_location.y(),
                                      location_in_screen.y()));
    event_generator_->ScheduleMouseEvent(ui::EventType::kMouseMoved, point,
                                         flags);
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
  std::optional<api::autotest_private::SetMetricsEnabled::Params> params =
      api::autotest_private::SetMetricsEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  VLOG(1) << "AutotestPrivateSetMetricsEnabledFunction " << std::boolalpha
          << params->enabled;

  target_value_ = params->enabled;

  Profile* profile = Profile::FromBrowserContext(browser_context());

  bool value;
  if (ash::CrosSettings::Get()->GetBoolean(ash::kStatsReportingPref, &value) &&
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
  if (!ash::CrosSettings::Get()->GetBoolean(ash::kStatsReportingPref,
                                            &actual)) {
    NOTREACHED_IN_MIGRATION() << "AutotestPrivateSetMetricsEnabledFunction: "
                              << "kStatsReportingPref should be set";
    Respond(Error(base::StrCat({"Failed to set metrics consent: ",
                                ash::kStatsReportingPref, " is not set."})));
    return;
  }
  VLOG(1) << "AutotestPrivateSetMetricsEnabledFunction: actual: "
          << std::boolalpha << actual << " and expected: " << std::boolalpha
          << target_value_;
  if (actual == target_value_) {
    Respond(NoArguments());
  } else {
    Respond(Error(base::StrCat(
        {"Failed to set metrics consent: ", ash::kStatsReportingPref,
         " has wrong value."})));
  }
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
  std::optional<api::autotest_private::SetArcTouchMode::Params> params =
      api::autotest_private::SetArcTouchMode::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetArcTouchModeFunction " << params->enabled;

  if (!arc::SetTouchMode(params->enabled)) {
    return RespondNow(Error("Could not send intent to ARC."));
  }

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
  std::optional<api::autotest_private::PinShelfIcon::Params> params =
      api::autotest_private::PinShelfIcon::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivatePinShelfIconFunction " << params->app_id;

  ChromeShelfController* const controller = ChromeShelfController::instance();
  if (!controller) {
    return RespondNow(Error("Controller not available"));
  }

  PinAppWithIDToShelf(params->app_id);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetShelfIconPinFunction
////////////////////////////////////////////////////////////////////////////////
AutotestPrivateSetShelfIconPinFunction::
    AutotestPrivateSetShelfIconPinFunction() = default;
AutotestPrivateSetShelfIconPinFunction::
    ~AutotestPrivateSetShelfIconPinFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetShelfIconPinFunction::Run() {
  std::optional<api::autotest_private::SetShelfIconPin::Params> params =
      api::autotest_private::SetShelfIconPin::Params::Create(args());

  ChromeShelfController* const controller = ChromeShelfController::instance();
  if (!controller) {
    return RespondNow(Error("Controller not available"));
  }

  const std::vector<api::autotest_private::ShelfIconPinUpdateParam>&
      update_params = params->update_params;

  // Save the app IDs causing errors.
  std::vector<std::string> problematic_app_ids;

  for (const auto& update_param : update_params) {
    const std::string& app_id = update_param.app_id;
    if (!controller->AllowedToSetAppPinState(app_id, update_param.pinned)) {
      problematic_app_ids.push_back(app_id);
    }
  }

  if (!problematic_app_ids.empty()) {
    return RespondNow(
        Error(base::StrCat({"Unable to update pin state: ",
                            base::JoinString(problematic_app_ids, ",")})));
  }

  // Save the ids of the apps whose pin states are updated. Note that the apps
  // which reach the target pin states before api function execution are not
  // included in `updated_apps`.
  std::vector<std::string> updated_apps;

  for (const auto& update_param : update_params) {
    const std::string& app_id = update_param.app_id;

    // Already reach the target pin state. No op.
    if (update_param.pinned == controller->IsAppPinned(app_id)) {
      continue;
    }

    if (update_param.pinned) {
      PinAppWithIDToShelf(app_id);
    } else {
      UnpinAppWithIDFromShelf(app_id);
    }
    updated_apps.push_back(app_id);
  }

  return RespondNow(ArgumentList(
      api::autotest_private::SetShelfIconPin::Results::Create(updated_apps)));
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
  std::optional<api::autotest_private::GetScrollableShelfInfoForState::Params>
      params =
          api::autotest_private::GetScrollableShelfInfoForState::Params::Create(
              args());

  ash::ShelfTestApi shelf_test_api;

  ash::ShelfState state;

  if (params->state.scroll_distance) {
    state.scroll_distance = *params->state.scroll_distance;
  }

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
    info.target_main_axis_offset = fetched_info.target_main_axis_offset;
  }

  return RespondNow(WithArguments(info.ToValue()));
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
  std::optional<api::autotest_private::GetShelfUIInfoForState::Params> params =
      api::autotest_private::GetShelfUIInfoForState::Params::Create(args());

  ash::ShelfState state;
  if (params->state.scroll_distance) {
    state.scroll_distance = *params->state.scroll_distance;
  }

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
    scrollable_shelf_ui_info.icons_under_animation =
        fetched_info.icons_under_animation;
    scrollable_shelf_ui_info.is_overflow = fetched_info.is_overflow;
    scrollable_shelf_ui_info.icons_bounds_in_screen =
        ToBoundsDictionaryList(fetched_info.icons_bounds_in_screen);
    scrollable_shelf_ui_info.is_shelf_widget_animating =
        fetched_info.is_shelf_widget_animating;

    if (state.scroll_distance) {
      scrollable_shelf_ui_info.target_main_axis_offset =
          fetched_info.target_main_axis_offset;
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
    hotseat_ui_info.is_auto_hidden = hotseat_info.is_auto_hidden;

    shelf_ui_info.hotseat_info = std::move(hotseat_ui_info);
  }

  return RespondNow(WithArguments(shelf_ui_info.ToValue()));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetWindowBoundsFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetWindowBoundsFunction::
    AutotestPrivateSetWindowBoundsFunction() = default;
AutotestPrivateSetWindowBoundsFunction::
    ~AutotestPrivateSetWindowBoundsFunction() = default;

namespace {

base::Value::Dict BuildSetWindowBoundsResult(const gfx::Rect& bounds_in_display,
                                             int64_t display_id) {
  base::Value::Dict result;
  result.Set("bounds", ToBoundsDictionary(bounds_in_display).ToValue());
  result.Set("displayId", base::NumberToString(display_id));
  return result;
}

}  // namespace

ExtensionFunction::ResponseAction
AutotestPrivateSetWindowBoundsFunction::Run() {
  std::optional<api::autotest_private::SetWindowBounds::Params> params =
      api::autotest_private::SetWindowBounds::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  aura::Window* window = FindAppWindowById(params->id);
  if (!window) {
    return RespondNow(Error(
        base::StringPrintf("No app window was found : id=%d", params->id)));
  }

  auto* state = ash::WindowState::Get(window);
  if (!state || chromeos::ToWindowShowState(state->GetStateType()) !=
                    ui::mojom::WindowShowState::kNormal) {
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
  if (!root_window) {
    return RespondNow(Error("Failed to find the root window"));
  }

  gfx::Rect to_bounds = ToRect(params->bounds);

  if (window->GetBoundsInRootWindow() == to_bounds &&
      state->GetDisplay().id() == display_id) {
    return RespondNow(
        WithArguments(BuildSetWindowBoundsResult(to_bounds, display_id)));
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
    Respond(WithArguments(
        BuildSetWindowBoundsResult(bounds_in_display, display_id)));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStartSmoothnessTrackingFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateStartSmoothnessTrackingFunction::
    ~AutotestPrivateStartSmoothnessTrackingFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateStartSmoothnessTrackingFunction::Run() {
  auto params =
      api::autotest_private::StartSmoothnessTracking::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t display_id;
  if (!GetDisplayIdFromOptionalArg(params->display_id, &display_id)) {
    return RespondNow(
        Error(base::StrCat({"Invalid display id: ", *params->display_id})));
  }

  auto* trackers = GetDisplaySmoothnessTrackers();
  if (trackers->find(display_id) != trackers->end()) {
    return RespondNow(
        Error(base::StrCat({"Smoothness already tracked for display: ",
                            base::NumberToString(display_id)})));
  }

  base::TimeDelta throughput_interval = kDefaultThroughputInterval;
  if (params->throughput_interval_ms) {
    throughput_interval = base::Milliseconds(*params->throughput_interval_ms);
  }

  auto tracker = std::make_unique<DisplaySmoothnessTracker>();
  if (!tracker->Start(
          display_id, throughput_interval,
          base::BindOnce(&ForwardFrameRateDataAndReset, display_id))) {
    return RespondNow(Error(base::StrCat(
        {"Invalid display_id; no root window found for the display id ",
         base::NumberToString(display_id)})));
  }
  (*trackers)[display_id] = std::move(tracker);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStopSmoothnessTrackingFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateStopSmoothnessTrackingFunction::
    ~AutotestPrivateStopSmoothnessTrackingFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateStopSmoothnessTrackingFunction::Run() {
  auto params =
      api::autotest_private::StopSmoothnessTracking::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t display_id;
  if (!GetDisplayIdFromOptionalArg(params->display_id, &display_id)) {
    return RespondNow(
        Error(base::StrCat({"Invalid display id: ", *params->display_id})));
  }

  auto* trackers = GetDisplaySmoothnessTrackers();
  auto it = trackers->find(display_id);
  if (it == trackers->end()) {
    return RespondNow(
        Error(base::StrCat({"Smoothness is not tracked for display: ",
                            base::NumberToString(display_id)})));
  }

  auto& [_, tracker] = *it;
  if (tracker->stopping()) {
    return RespondNow(Error(
        base::StrCat({"stopSmoothnessTracking already called for display: ",
                      base::NumberToString(display_id)})));
  }

  const bool has_error = tracker->has_error();

#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
  // Use a longer report timeout for sanitizers. See http://crbug.com/41491890.
  constexpr base::TimeDelta kReportTimeout = base::Seconds(20);
#else
  constexpr base::TimeDelta kReportTimeout = base::Seconds(5);
#endif

  // DisplaySmoothnessTracker::Stop does not invoke the report callback when
  // gpu-process crashes and has no valid data to report. Start a timer to
  // handle this case.
  timeout_timer_.Start(
      FROM_HERE, kReportTimeout,
      base::BindOnce(&AutotestPrivateStopSmoothnessTrackingFunction::OnTimeOut,
                     this, display_id));

  if (!tracker->Stop(base::BindOnce(
          &AutotestPrivateStopSmoothnessTrackingFunction::OnReportData, this,
          tracker->start_time()))) {
    timeout_timer_.AbandonAndStop();
    trackers->erase(it);
    return RespondNow(
        Error("No smoothness report, GPU process may have crashed"));
  }

  // Trigger a repaint after ThroughputTracker::Stop() to generate a frame to
  // ensure the tracker report will be sent back.
  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  root_window->GetHost()->compositor()->ScheduleFullRedraw();

  if (has_error) {
    return RespondNow(Error(base::StrCat(
        {"Error happened during smoothness collection for display: ",
         base::NumberToString(display_id)})));
  }

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutotestPrivateStopSmoothnessTrackingFunction::OnReportData(
    base::TimeTicks start_time,
    const cc::FrameSequenceMetrics::CustomReportData& frame_data,
    std::vector<int>&& throughput) {
  if (did_respond()) {
    return;
  }

  timeout_timer_.AbandonAndStop();

  std::vector<int> jank_timestamps;  // In milliseconds.
  std::vector<int> jank_durations;   // In milliseconds.
  jank_timestamps.reserve(frame_data.janks.size());
  jank_durations.reserve(frame_data.janks.size());

  for (auto jank : frame_data.janks) {
    jank_timestamps.emplace_back(
        (jank.start_time - start_time).InMilliseconds());
    jank_durations.emplace_back(jank.duration.InMilliseconds());
  }

  api::autotest_private::DisplaySmoothnessData result_data;
  result_data.frames_expected = frame_data.frames_expected_v3;
  result_data.frames_produced =
      frame_data.frames_expected_v3 - frame_data.frames_dropped_v3;
  result_data.jank_count = frame_data.jank_count_v3;
  result_data.throughput = std::move(throughput);
  result_data.jank_timestamps = std::move(jank_timestamps);
  result_data.jank_durations = std::move(jank_durations);

  Respond(ArgumentList(
      api::autotest_private::StopSmoothnessTracking::Results::Create(
          result_data)));
}

void AutotestPrivateStopSmoothnessTrackingFunction::OnTimeOut(
    int64_t display_id) {
  if (did_respond()) {
    return;
  }

  // Clean up the non-functional tracker.
  auto* trackers = GetDisplaySmoothnessTrackers();
  auto it = trackers->find(display_id);
  if (it == trackers->end()) {
    return;
  }
  it->second->CancelReport();
  trackers->erase(it);

  Respond(Error("Smoothness is not available"));
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
  std::optional<api::autotest_private::WaitForAmbientPhotoAnimation::Params>
      params =
          api::autotest_private::WaitForAmbientPhotoAnimation::Params::Create(
              args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Wait for photo transition animation completed in ambient mode.
  ash::AutotestAmbientApi().WaitForPhotoTransitionAnimationCompleted(
      params->num_completions, base::Seconds(params->timeout),
      /*on_complete=*/
      base::BindOnce(&AutotestPrivateWaitForAmbientPhotoAnimationFunction::
                         OnPhotoTransitionAnimationCompleted,
                     this),
      /*on_timeout=*/
      base::BindOnce(
          &AutotestPrivateWaitForAmbientPhotoAnimationFunction::Timeout, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutotestPrivateWaitForAmbientPhotoAnimationFunction::
    OnPhotoTransitionAnimationCompleted() {
  if (did_respond()) {
    return;
  }

  Respond(NoArguments());
}

void AutotestPrivateWaitForAmbientPhotoAnimationFunction::Timeout() {
  if (did_respond()) {
    return;
  }

  Respond(Error("Not enough animations completed before time out."));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateWaitForAmbientVideoFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateWaitForAmbientVideoFunction::
    AutotestPrivateWaitForAmbientVideoFunction() = default;

AutotestPrivateWaitForAmbientVideoFunction::
    ~AutotestPrivateWaitForAmbientVideoFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateWaitForAmbientVideoFunction::Run() {
  std::optional<api::autotest_private::WaitForAmbientVideo::Params> params =
      api::autotest_private::WaitForAmbientVideo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // Wait for video playback to start in ambient mode.
  ash::AutotestAmbientApi().WaitForVideoToStart(
      base::Seconds(params->timeout),
      /*on_complete=*/
      base::BindOnce(
          &AutotestPrivateWaitForAmbientVideoFunction::RespondWithSuccess,
          this),
      /*on_error=*/
      base::BindOnce(
          &AutotestPrivateWaitForAmbientVideoFunction::RespondWithError, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutotestPrivateWaitForAmbientVideoFunction::RespondWithSuccess() {
  Respond(NoArguments());
}

void AutotestPrivateWaitForAmbientVideoFunction::RespondWithError(
    std::string error_message) {
  Respond(Error(std::move(error_message)));
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
  // This disables accessibility for Chrome Views.
  AutomationManagerAura::GetInstance()->Disable();

  // This disables accessibility for all other accessibility trees including
  // ARC++, Ash web contents.
  AutomationEventRouter::GetInstance()
      ->UnregisterAllListenersWithDesktopPermission();
  AutomationEventRouter::GetInstance()->NotifyAllAutomationExtensionsGone();

  // Finally, this disables accessibility in Lacros.
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->automation_ash()
      ->AllAutomationExtensionsGone();

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
  g_last_start_throughput_data_collection_tick = base::TimeTicks::Now();
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
  result_data.reserve(collected_data.size());
  for (const auto& data : collected_data) {
    api::autotest_private::ThroughputTrackerAnimationData animation_data;
    animation_data.start_offset_ms =
        (data.start_tick - g_last_start_throughput_data_collection_tick)
            .InMilliseconds();
    animation_data.stop_offset_ms =
        (data.stop_tick - g_last_start_throughput_data_collection_tick)
            .InMilliseconds();
    animation_data.frames_expected = data.smoothness_data.frames_expected_v3;
    animation_data.frames_produced = data.smoothness_data.frames_expected_v3 -
                                     data.smoothness_data.frames_dropped_v3;
    animation_data.jank_count = data.smoothness_data.jank_count_v3;
    result_data.emplace_back(std::move(animation_data));
  }
  return RespondNow(
      ArgumentList(api::autotest_private::StopThroughputTrackerDataCollection::
                       Results::Create(result_data)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetThroughputTrackerDataFunction
///////////////////////////////////////////////////////////////////////////////
AutotestPrivateGetThroughputTrackerDataFunction::
    AutotestPrivateGetThroughputTrackerDataFunction() = default;

AutotestPrivateGetThroughputTrackerDataFunction::
    ~AutotestPrivateGetThroughputTrackerDataFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetThroughputTrackerDataFunction::Run() {
  auto collected_data = ash::metrics_util::GetCollectedData();
  std::vector<api::autotest_private::ThroughputTrackerAnimationData>
      result_data;
  result_data.reserve(collected_data.size());
  for (const auto& data : collected_data) {
    api::autotest_private::ThroughputTrackerAnimationData animation_data;
    animation_data.start_offset_ms =
        (data.start_tick - g_last_start_throughput_data_collection_tick)
            .InMilliseconds();
    animation_data.stop_offset_ms =
        (data.stop_tick - g_last_start_throughput_data_collection_tick)
            .InMilliseconds();
    animation_data.frames_expected = data.smoothness_data.frames_expected_v3;
    animation_data.frames_produced = data.smoothness_data.frames_expected_v3 -
                                     data.smoothness_data.frames_dropped_v3;
    animation_data.jank_count = data.smoothness_data.jank_count_v3;
    result_data.emplace_back(std::move(animation_data));
  }
  return RespondNow(ArgumentList(
      api::autotest_private::GetThroughputTrackerData::Results::Create(
          result_data)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetDisplaySmoothnessFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetDisplaySmoothnessFunction::
    AutotestPrivateGetDisplaySmoothnessFunction() = default;

AutotestPrivateGetDisplaySmoothnessFunction::
    ~AutotestPrivateGetDisplaySmoothnessFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetDisplaySmoothnessFunction::Run() {
  auto params =
      api::autotest_private::GetDisplaySmoothness::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t display_id;
  if (!GetDisplayIdFromOptionalArg(params->display_id, &display_id)) {
    return RespondNow(
        Error(base::StrCat({"Invalid display id: ", *params->display_id})));
  }

  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id);
  const uint32_t smoothness =
      100 - root_window->GetHost()->compositor()->GetPercentDroppedFrames();
  return RespondNow(
      ArgumentList(api::autotest_private::GetDisplaySmoothness::Results::Create(
          smoothness)));
}

////////////////////////////////////////////////////////////////////////////////
// AutotestPrivateResetHoldingSpaceFunction
////////////////////////////////////////////////////////////////////////////////

AutotestPrivateResetHoldingSpaceFunction::
    AutotestPrivateResetHoldingSpaceFunction() = default;

AutotestPrivateResetHoldingSpaceFunction::
    ~AutotestPrivateResetHoldingSpaceFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateResetHoldingSpaceFunction::Run() {
  auto params =
      api::autotest_private::ResetHoldingSpace::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());

  ash::HoldingSpaceKeyedService* service =
      ash::HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile);

  if (service == nullptr) {
    return RespondNow(Error("Failed to get `HoldingSpaceKeyedService`."));
  }

  service->RemoveAll();

  PrefService* prefs = profile->GetPrefs();
  ash::holding_space_prefs::ResetProfilePrefsForTesting(prefs);

  if (!ash::holding_space_prefs::MarkTimeOfFirstAvailability(prefs)) {
    return RespondNow(
        Error("Failed to call `MarkTimeOfFirstAvailability()` after clearing "
              "prefs."));
  }

  if (!params->options || !params->options->mark_time_of_first_add) {
    return RespondNow(NoArguments());
  }

  if (!ash::holding_space_prefs::MarkTimeOfFirstAdd(prefs)) {
    return RespondNow(
        Error("Failed to call `MarkTimeOfFirstAdd()` after clearing prefs."));
  }

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStartLoginEventRecorderDataCollectionFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateStartLoginEventRecorderDataCollectionFunction::
    AutotestPrivateStartLoginEventRecorderDataCollectionFunction() = default;

AutotestPrivateStartLoginEventRecorderDataCollectionFunction::
    ~AutotestPrivateStartLoginEventRecorderDataCollectionFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateStartLoginEventRecorderDataCollectionFunction::Run() {
  ash::LoginEventRecorder::Get()
      ->PrepareEventCollectionForTesting();  // IN-TEST
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetLoginEventRecorderLoginEventsFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetLoginEventRecorderLoginEventsFunction::
    AutotestPrivateGetLoginEventRecorderLoginEventsFunction() = default;

AutotestPrivateGetLoginEventRecorderLoginEventsFunction::
    ~AutotestPrivateGetLoginEventRecorderLoginEventsFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetLoginEventRecorderLoginEventsFunction::Run() {
  const auto& collected_data =
      ash::LoginEventRecorder::Get()
          ->GetCollectedLoginEventsForTesting();  // IN-TEST
  std::vector<api::autotest_private::LoginEventRecorderData> result_data;
  for (const auto& data : collected_data) {
    api::autotest_private::LoginEventRecorderData event_data;
    event_data.name = data.name();
    event_data.microsecnods_since_unix_epoch =
        (data.time() - base::TimeTicks::UnixEpoch()).InMicroseconds();
    result_data.emplace_back(std::move(event_data));
  }

  return RespondNow(ArgumentList(
      api::autotest_private::GetLoginEventRecorderLoginEvents::Results::Create(
          result_data)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateAddLoginEventForTestingFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateAddLoginEventForTestingFunction::
    AutotestPrivateAddLoginEventForTestingFunction() = default;

AutotestPrivateAddLoginEventForTestingFunction::
    ~AutotestPrivateAddLoginEventForTestingFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateAddLoginEventForTestingFunction::Run() {
  ash::LoginEventRecorder::Get()->AddLoginTimeMarker(
      /*marker_name=*/"AutotestPrivateTestMarker",
      /*send_to_uma=*/false,
      /*write_to_file=*/false);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateForceAutoThemeModeFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateForceAutoThemeModeFunction::
    AutotestPrivateForceAutoThemeModeFunction() = default;

AutotestPrivateForceAutoThemeModeFunction::
    ~AutotestPrivateForceAutoThemeModeFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateForceAutoThemeModeFunction::Run() {
  DVLOG(1) << "AutotestPrivateForceAutoThemeModeFunction";

  std::optional<api::autotest_private::ForceAutoThemeMode::Params> params =
      api::autotest_private::ForceAutoThemeMode::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ash::DarkLightModeControllerImpl* dark_light_mode_controller =
      ash::Shell::Get()->dark_light_mode_controller();
  DCHECK(dark_light_mode_controller);

  dark_light_mode_controller->SetAutoScheduleEnabled(false);
  dark_light_mode_controller->SetDarkModeEnabledForTest(
      params->dark_mode_enabled);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetAccessTokenFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetAccessTokenFunction::AutotestPrivateGetAccessTokenFunction() =
    default;

AutotestPrivateGetAccessTokenFunction::
    ~AutotestPrivateGetAccessTokenFunction() = default;

ExtensionFunction::ResponseAction AutotestPrivateGetAccessTokenFunction::Run() {
  // Require a command line switch to avoid crashing on accident.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kGetAccessTokenForTest)) {
    return RespondNow(
        Error("* switch is not set", ash::switches::kGetAccessTokenForTest));
  }
  // This API is available only on test images.
  base::SysInfo::CrashIfChromeOSNonTestImage();

  auto params = api::autotest_private::GetAccessToken::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  timeout_timer_.Start(
      FROM_HERE,
      base::Milliseconds(params->access_token_params.timeout_ms
                             ? *params->access_token_params.timeout_ms
                             : 90000),
      base::BindOnce(
          &AutotestPrivateGetAccessTokenFunction::RespondWithTimeoutError,
          this));

  Profile* profile = Profile::FromBrowserContext(browser_context());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  OAuth2AccessTokenManager::ScopeSet scopes(
      params->access_token_params.scopes.begin(),
      params->access_token_params.scopes.end());
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      identity_manager
          ->FindExtendedAccountInfoByEmailAddress(
              params->access_token_params.email)
          .account_id,
      /*oauth_consumer_name=*/"cros_autotest_private", scopes,
      base::BindOnce(&AutotestPrivateGetAccessTokenFunction::OnAccessToken,
                     this),
      signin::AccessTokenFetcher::Mode::kImmediate);
  return RespondLater();
}

void AutotestPrivateGetAccessTokenFunction::RespondWithTimeoutError() {
  if (did_respond()) {
    return;
  }
  Respond(Error("Timed out fetching access token"));
  access_token_fetcher_.reset();
}

void AutotestPrivateGetAccessTokenFunction::OnAccessToken(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  access_token_fetcher_.reset();
  timeout_timer_.AbandonAndStop();
  if (did_respond()) {
    return;
  }
  if (error.state() != GoogleServiceAuthError::NONE) {
    Respond(Error("Failed to get access token: *", error.ToString()));
    return;
  }
  base::Value::Dict token_dict;
  token_dict.Set("accessToken", token_info.token);
  token_dict.Set(
      "expirationTimeUnixMs",
      base::Int64ToValue((token_info.expiration_time - base::Time::UnixEpoch())
                             .InMilliseconds()));
  Respond(WithArguments(std::move(token_dict)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateIsInputMethodReadyForTestingFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateIsInputMethodReadyForTestingFunction::
    AutotestPrivateIsInputMethodReadyForTestingFunction() = default;

AutotestPrivateIsInputMethodReadyForTestingFunction::
    ~AutotestPrivateIsInputMethodReadyForTestingFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateIsInputMethodReadyForTestingFunction::Run() {
  ash::TextInputMethod* engine =
      ash::IMEBridge::Get()->GetCurrentEngineHandler();
  return RespondNow(
      WithArguments(engine && engine->IsReadyForTesting()));  // IN-TEST
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateOverrideOrcaResponseForTestingFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateOverrideOrcaResponseForTestingFunction::
    AutotestPrivateOverrideOrcaResponseForTestingFunction() = default;

AutotestPrivateOverrideOrcaResponseForTestingFunction::
    ~AutotestPrivateOverrideOrcaResponseForTestingFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateOverrideOrcaResponseForTestingFunction::Run() {
  std::optional<api::autotest_private::OverrideOrcaResponseForTesting::Params>
      params =
          api::autotest_private::OverrideOrcaResponseForTesting::Params::Create(
              args());

  EXTENSION_FUNCTION_VALIDATE(params);

  ash::input_method::EditorMediator* editor_mediator =
      ash::input_method::EditorMediatorFactory::GetInstance()->GetForProfile(
          ash::ProfileHelper::Get()->GetProfileByUser(
              user_manager::UserManager::Get()->GetActiveUser()));

  return RespondNow(
      WithArguments(editor_mediator->SetTextQueryProviderResponseForTesting(
          params->array.responses)));  // IN-TEST
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateMakeFuseboxTempDirFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateMakeFuseboxTempDirFunction::
    ~AutotestPrivateMakeFuseboxTempDirFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateMakeFuseboxTempDirFunction::Run() {
  fusebox::Server* server = fusebox::Server::GetInstance();
  if (!server) {
    return RespondNow(Error("Fusebox server instance not available"));
  }
  server->MakeTempDir(base::BindOnce(
      &AutotestPrivateMakeFuseboxTempDirFunction::OnMakeTempDir, this));
  return RespondLater();
}

void AutotestPrivateMakeFuseboxTempDirFunction::OnMakeTempDir(
    const std::string& error_message,
    const std::string& fusebox_file_path,
    const std::string& underlying_file_path) {
  if (!error_message.empty()) {
    Respond(Error(error_message));
    return;
  }
  base::Value::Dict dict;
  dict.Set("fuseboxFilePath", fusebox_file_path);
  dict.Set("underlyingFilePath", underlying_file_path);
  Respond(WithArguments(std::move(dict)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRemoveFuseboxTempDirFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRemoveFuseboxTempDirFunction::
    ~AutotestPrivateRemoveFuseboxTempDirFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRemoveFuseboxTempDirFunction::Run() {
  std::optional<api::autotest_private::RemoveFuseboxTempDir::Params> params =
      api::autotest_private::RemoveFuseboxTempDir::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  fusebox::Server* server = fusebox::Server::GetInstance();
  if (!server) {
    return RespondNow(Error("Fusebox server instance not available"));
  }
  server->RemoveTempDir(params->fusebox_file_path);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRemoveComponentExtension
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRemoveComponentExtensionFunction::
    ~AutotestPrivateRemoveComponentExtensionFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRemoveComponentExtensionFunction::Run() {
  std::optional<api::autotest_private::RemoveComponentExtension::Params>
      params = api::autotest_private::RemoveComponentExtension::Params::Create(
          args());
  EXTENSION_FUNCTION_VALIDATE(params);

  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser_context())->extension_service();
  extension_service->component_loader()->Remove(params->extension_id);

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStartFrameCountingFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateStartFrameCountingFunction::
    AutotestPrivateStartFrameCountingFunction() = default;

AutotestPrivateStartFrameCountingFunction::
    ~AutotestPrivateStartFrameCountingFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateStartFrameCountingFunction::Run() {
  std::optional<api::autotest_private::StartFrameCounting::Params> params =
      api::autotest_private::StartFrameCounting::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->bucket_size_in_seconds <= 0) {
    return RespondNow(
        Error("Param bucketSizeInSeconds must be greater than 0s"));
  }

  // "viz.mojom.FrameCountingPerSinkData" uses uint16 to store frame counts.
  // Limit the max bucket size so that the max frame count does not go beyond
  // uint16 max. 500s is safe even for a 120fps system.
  constexpr int kMaxBucketSizeInSeconds = 500;
  if (params->bucket_size_in_seconds > kMaxBucketSizeInSeconds) {
    return RespondNow(
        Error("Param bucketSizeInSeconds must be less than 500s"));
  }

  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->GetFrameSinksMetricsRecorderForTest()  // IN-TEST
      .StartFrameCounting(base::TimeTicks::Now(),
                          base::Seconds(params->bucket_size_in_seconds));
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStopFrameCountingFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateStopFrameCountingFunction::
    AutotestPrivateStopFrameCountingFunction() = default;

AutotestPrivateStopFrameCountingFunction::
    ~AutotestPrivateStopFrameCountingFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateStopFrameCountingFunction::Run() {
  auto callback = base::BindOnce(
      &AutotestPrivateStopFrameCountingFunction::OnDataReceived, this);
  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->GetFrameSinksMetricsRecorderForTest()  // IN-TEST
      .StopFrameCounting(std::move(callback));
  return RespondLater();
}

void AutotestPrivateStopFrameCountingFunction::OnDataReceived(
    viz::mojom::FrameCountingDataPtr data_ptr) {
  if (!data_ptr || data_ptr->per_sink_data.empty()) {
    Respond(Error("No frame counting data"));
    return;
  }

  // The data to fill in buckets where there is no data points in collected
  // frame data, i.e. before the frame sink's creation and after the frame
  // sink's destruction.
  constexpr int kNotAvailable = -1;

  // Get the max size of frame sink data. The frame sink data that does not
  // have enough data points (e.g. frame sinks destroyed before the end) will
  // have kNotAvailable appended at the end.
  size_t size = 0;
  for (const auto& per_sink_data : data_ptr->per_sink_data) {
    const size_t per_sink_data_size =
        per_sink_data->start_bucket + per_sink_data->presented_frames.size();
    if (per_sink_data_size > size) {
      size = per_sink_data_size;
    }
  }

  std::vector<api::autotest_private::FrameCountingPerSinkData> result;
  for (const auto& per_sink_data : data_ptr->per_sink_data) {
    // Skip frame sinks with no data points.
    if (per_sink_data->presented_frames.empty()) {
      continue;
    }

    api::autotest_private::FrameCountingPerSinkData result_per_sink_data;
    result_per_sink_data.sink_type =
        CompositorFrameSinkTypeToString(per_sink_data->type);
    result_per_sink_data.is_root = per_sink_data->is_root;
    result_per_sink_data.debug_label = per_sink_data->debug_label;

    if (per_sink_data->start_bucket != 0) {
      result_per_sink_data.presented_frames.resize(per_sink_data->start_bucket,
                                                   kNotAvailable);
    }

    std::copy(per_sink_data->presented_frames.begin(),
              per_sink_data->presented_frames.end(),
              std::back_inserter(result_per_sink_data.presented_frames));

    if (result_per_sink_data.presented_frames.size() < size) {
      result_per_sink_data.presented_frames.resize(size, kNotAvailable);
    }

    result.emplace_back(std::move(result_per_sink_data));
  }

  Respond(ArgumentList(
      api::autotest_private::StopFrameCounting::Results::Create(result)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStartOverdrawTrackingFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateStartOverdrawTrackingFunction::
    AutotestPrivateStartOverdrawTrackingFunction() = default;

AutotestPrivateStartOverdrawTrackingFunction::
    ~AutotestPrivateStartOverdrawTrackingFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateStartOverdrawTrackingFunction::Run() {
  std::optional<api::autotest_private::StartOverdrawTracking::Params> params =
      api::autotest_private::StartOverdrawTracking::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t target_display_id;
  if (!GetDisplayIdFromOptionalArg(params->display_id, &target_display_id)) {
    return RespondNow(
        Error(base::StrCat({"Invalid displayId: ", *params->display_id})));
  }

  DVLOG(1) << "AutotestPrivateStopOverdrawTrackingFunction displayId:"
           << target_display_id;

  // Validate display id.
  bool found_display = false;
  for (aura::Window* const window : ash::Shell::GetAllRootWindows()) {
    const int64_t display_id =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
    if (display_id == target_display_id) {
      found_display = true;
    }
  }

  if (!found_display) {
    return RespondNow(Error(base::StringPrintf(
        "Invalid displayId; no display found for the display id %" PRId64,
        target_display_id)));
  }

  if (params->bucket_size_in_seconds <= 0) {
    return RespondNow(
        Error("Invalid bucketSizeInSeconds; must be greater than 0s"));
  }

  const ui::Compositor* compositor =
      ash::Shell::GetRootWindowForDisplayId(target_display_id)
          ->layer()
          ->GetCompositor();

  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->GetFrameSinksMetricsRecorderForTest()  // IN-TEST
      .StartOverdrawTracking(compositor->frame_sink_id(),
                             base::Seconds(params->bucket_size_in_seconds));

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateStopOverdrawTrackingFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateStopOverdrawTrackingFunction::
    AutotestPrivateStopOverdrawTrackingFunction() = default;

AutotestPrivateStopOverdrawTrackingFunction::
    ~AutotestPrivateStopOverdrawTrackingFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateStopOverdrawTrackingFunction::Run() {
  std::optional<api::autotest_private::StopOverdrawTracking::Params> params =
      api::autotest_private::StopOverdrawTracking::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  int64_t target_display_id;
  if (!GetDisplayIdFromOptionalArg(params->display_id, &target_display_id)) {
    return RespondNow(
        Error(base::StrCat({"Invalid displayId: ", *params->display_id})));
  }

  DVLOG(1) << "AutotestPrivateStopOverdrawTrackingFunction displayId:"
           << target_display_id;

  // Validate display id.
  bool found_display = false;
  for (aura::Window* const window : ash::Shell::GetAllRootWindows()) {
    const int64_t display_id =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
    if (display_id == target_display_id) {
      found_display = true;
    }
  }

  if (!found_display) {
    return RespondNow(Error(base::StringPrintf(
        "Invalid displayId; no display found for the display id %" PRId64,
        target_display_id)));
  }

  const ui::Compositor* compositor =
      ash::Shell::GetRootWindowForDisplayId(target_display_id)
          ->layer()
          ->GetCompositor();

  auto callback = base::BindOnce(
      &AutotestPrivateStopOverdrawTrackingFunction::OnDataReceived, this);
  aura::Env::GetInstance()
      ->context_factory()
      ->GetHostFrameSinkManager()
      ->GetFrameSinksMetricsRecorderForTest()  // IN-TEST
      .StopOverdrawTracking(compositor->frame_sink_id(), std::move(callback));

  return RespondLater();
}

void AutotestPrivateStopOverdrawTrackingFunction::OnDataReceived(
    viz::mojom::OverdrawDataPtr data_ptr) {
  // Data can be missing if gpu process is restarted in middle of test
  // and test scripts still calls `stopOverdrawTracking`.
  if (!data_ptr || data_ptr->average_overdraws.empty()) {
    Respond(
        Error("No overdraw data; maybe forgot to call startOverdrawTracking or "
              "no UI changes between start and stop calls"));
    return;
  }

  api::autotest_private::OverdrawData result;
  result.average_overdraws.reserve(data_ptr->average_overdraws.size());

  std::copy(data_ptr->average_overdraws.begin(),
            data_ptr->average_overdraws.end(),
            std::back_inserter(result.average_overdraws));

  Respond(ArgumentList(
      api::autotest_private::StopOverdrawTracking::Results::Create(result)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateBruschettaInstallFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateInstallBruschettaFunction::
    AutotestPrivateInstallBruschettaFunction() = default;

AutotestPrivateInstallBruschettaFunction::
    ~AutotestPrivateInstallBruschettaFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateInstallBruschettaFunction::Run() {
  // This API is available only on test images.
  base::SysInfo::CrashIfChromeOSNonTestImage();

  std::optional<api::autotest_private::InstallBruschetta::Params> params =
      api::autotest_private::InstallBruschetta::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());

  BruschettaInstallerView::Show(profile,
                                bruschetta::MakeBruschettaId(params->vm_name));

  auto* view = BruschettaInstallerView::GetActiveViewForTesting();
  if (!view) {
    return RespondNow(Error("Couldn't open BruschettaInstallerView"));
  }

  view->set_finish_callback_for_testing(base::BindOnce(
      &AutotestPrivateInstallBruschettaFunction::OnInstallerFinish, this));

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AutotestPrivateInstallBruschettaFunction::ClickAccept, this));

  return RespondLater();
}

void AutotestPrivateInstallBruschettaFunction::ClickAccept() {
  auto* view = BruschettaInstallerView::GetActiveViewForTesting();
  if (view) {
    view->Accept();
  } else {
    Respond(Error("BruschettaInstallerView was closed unexpectedly"));
  }
}

void AutotestPrivateInstallBruschettaFunction::OnInstallerFinish(
    bruschetta::BruschettaInstallResult result) {
  if (result == bruschetta::BruschettaInstallResult::kSuccess) {
    Respond(NoArguments());
  } else {
    Respond(Error(base::UTF16ToUTF8(std::u16string_view(
        bruschetta::BruschettaInstallResultString(result)))));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateBruschettaRemoveFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateRemoveBruschettaFunction::
    AutotestPrivateRemoveBruschettaFunction() = default;

AutotestPrivateRemoveBruschettaFunction::
    ~AutotestPrivateRemoveBruschettaFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRemoveBruschettaFunction::Run() {
  // This API is available only on test images.
  base::SysInfo::CrashIfChromeOSNonTestImage();

  std::optional<api::autotest_private::RemoveBruschetta::Params> params =
      api::autotest_private::RemoveBruschetta::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());

  auto* service = bruschetta::BruschettaServiceFactory::GetForProfile(profile);
  if (!service) {
    return RespondNow(Error("Couldn't get BruschettaService instance"));
  }

  service->RemoveVm(
      bruschetta::MakeBruschettaId(params->vm_name),
      base::BindOnce(&AutotestPrivateRemoveBruschettaFunction::OnRemoveVm,
                     this));

  return RespondLater();
}

void AutotestPrivateRemoveBruschettaFunction::OnRemoveVm(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error("Failed to uninstall bruschetta"));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateIsFeatureEnabledFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateIsFeatureEnabledFunction::
    AutotestPrivateIsFeatureEnabledFunction() = default;

AutotestPrivateIsFeatureEnabledFunction::
    ~AutotestPrivateIsFeatureEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateIsFeatureEnabledFunction::Run() {
  std::optional<api::autotest_private::IsFeatureEnabled::Params> params =
      api::autotest_private::IsFeatureEnabled::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // base::FeatureList does not allow lookup by string name. Use an allowlist
  // of features instead.
  static const base::Feature* const kAllowList[] = {
      // clang-format off
      &ash::features::kFeatureManagementVideoConference,
      &ash::features::kSavedDeskUiRevamp,
      &chromeos::features::kJelly,
      &kDisabledFeatureForTest,
      &kEnabledFeatureForTest,
      // clang-format on
  };
  auto* const* it = base::ranges::find(kAllowList, params->feature_name,
                                       &base::Feature::name);
  if (it == std::end(kAllowList)) {
    std::string error = base::StrCat(
        {"feature ", params->feature_name,
         " is not on allowlist, see "
         "AutotestPrivateIsFeatureEnabledFunction::Run() to update the list"});
    return RespondNow(Error(error));
  }
  bool enabled = base::FeatureList::IsEnabled(**it);
  return RespondNow(WithArguments(enabled));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetCurrentInputMethodDescriptorFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetCurrentInputMethodDescriptorFunction::
    AutotestPrivateGetCurrentInputMethodDescriptorFunction() = default;

AutotestPrivateGetCurrentInputMethodDescriptorFunction::
    ~AutotestPrivateGetCurrentInputMethodDescriptorFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetCurrentInputMethodDescriptorFunction::Run() {
  auto* manager = ash::input_method::InputMethodManager::Get();
  ash::input_method::InputMethodDescriptor descriptor =
      manager->GetActiveIMEState()->GetCurrentInputMethod();

  base::Value::Dict dict;
  dict.Set("keyboardLayout", descriptor.keyboard_layout());
  return RespondNow(WithArguments(std::move(dict)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetArcInteractiveStateFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetArcInteractiveStateFunction::
    AutotestPrivateSetArcInteractiveStateFunction() = default;

AutotestPrivateSetArcInteractiveStateFunction::
    ~AutotestPrivateSetArcInteractiveStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetArcInteractiveStateFunction::Run() {
  std::optional<api::autotest_private::SetArcInteractiveState::Params> params =
      api::autotest_private::SetArcInteractiveState::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  arc::ArcServiceManager* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    return RespondNow(Error("ARC service manager is not available"));
  }

  arc::ArcBridgeService* arc_bridge_service =
      arc_service_manager->arc_bridge_service();

  if (!arc_bridge_service) {
    return RespondNow(Error("ARC bridge service is not available"));
  }

  arc::mojom::PowerInstance* power_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service->power(), SetIdleState);

  if (!power_instance) {
    return RespondNow(Error("ARC power service is not available"));
  }

  power_instance->SetIdleState(params->enabled
                                   ? arc::mojom::IdleState::ACTIVE
                                   : arc::mojom::IdleState::INACTIVE);

  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateIsFieldTrialActiveFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateIsFieldTrialActiveFunction::
    AutotestPrivateIsFieldTrialActiveFunction() = default;

AutotestPrivateIsFieldTrialActiveFunction::
    ~AutotestPrivateIsFieldTrialActiveFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateIsFieldTrialActiveFunction::Run() {
  std::optional<api::autotest_private::IsFieldTrialActive::Params> params =
      api::autotest_private::IsFieldTrialActive::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);

  bool found = false;
  for (base::FieldTrial::ActiveGroup field_trial : active_groups) {
    if (field_trial.trial_name == params->trial_name &&
        field_trial.group_name == params->group_name) {
      found = true;
      break;
    }
  }

  return RespondNow(WithArguments(found));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetArcWakefulnessModeFunction
//////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetArcWakefulnessModeFunction::
    AutotestPrivateGetArcWakefulnessModeFunction() = default;

AutotestPrivateGetArcWakefulnessModeFunction::
    ~AutotestPrivateGetArcWakefulnessModeFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetArcWakefulnessModeFunction::Run() {
  arc::ArcServiceManager* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    return RespondNow(Error("ARC service manager is not available"));
  }

  arc::ArcBridgeService* arc_bridge_service =
      arc_service_manager->arc_bridge_service();

  if (!arc_bridge_service) {
    return RespondNow(Error(
        "ARC service manager exist, but ARC bridge service is not available"));
  }

  arc::mojom::PowerInstance* power_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service->power(), SetIdleState);

  if (!power_instance) {
    return RespondNow(
        Error("ARC bridge exist, but ARC power service is not available"));
  }

  power_instance->GetWakefulnessMode(
      base::BindOnce(&AutotestPrivateGetArcWakefulnessModeFunction::
                         OnGetWakefulnessStateRespond,
                     this));

  return RespondLater();
}

void AutotestPrivateGetArcWakefulnessModeFunction::OnGetWakefulnessStateRespond(
    arc::mojom::WakefulnessMode mode) {
  return Respond(
      WithArguments(api::autotest_private::ToString(GetWakefulnessMode(mode))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetDeviceLanguageFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetDeviceLanguageFunction::
    AutotestPrivateSetDeviceLanguageFunction() = default;

AutotestPrivateSetDeviceLanguageFunction::
    ~AutotestPrivateSetDeviceLanguageFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetDeviceLanguageFunction::Run() {
  std::optional<api::autotest_private::SetDeviceLanguage::Params> params =
      api::autotest_private::SetDeviceLanguage::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetDeviceLanguageFunction " << params->locale;

  Profile* const profile = Profile::FromBrowserContext(browser_context());
  // Note that this only change the prefs, a restart would be required for the
  // change to take effect.
  profile->ChangeAppLocale(params->locale,
                           Profile::APP_LOCALE_CHANGED_VIA_SETTINGS);
  return RespondNow(NoArguments());
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetDeviceEventLogFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetDeviceEventLogFunction::
    AutotestPrivateGetDeviceEventLogFunction() = default;

AutotestPrivateGetDeviceEventLogFunction::
    ~AutotestPrivateGetDeviceEventLogFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetDeviceEventLogFunction::Run() {
  std::optional<api::autotest_private::GetDeviceEventLog::Params> params =
      api::autotest_private::GetDeviceEventLog::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateGetDeviceEventLogFunction " << params->type;

  std::string logs = device_event_log::GetAsString(
      device_event_log::OLDEST_FIRST, "time,file,type", params->type,
      device_event_log::LOG_LEVEL_DEBUG, 0);

  return RespondNow(WithArguments(base::UTF8ToUTF16(std::move(logs))));
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
    : browser_context_(context), test_mode_(false) {
  clipboard_observation_.Observe(ui::ClipboardMonitor::GetInstance());
}

AutotestPrivateAPI::~AutotestPrivateAPI() = default;

void AutotestPrivateAPI::OnClipboardDataChanged() {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  std::unique_ptr<Event> event(
      new Event(events::AUTOTESTPRIVATE_ON_CLIPBOARD_DATA_CHANGED,
                api::autotest_private::OnClipboardDataChanged::kEventName,
                base::Value::List()));
  event_router->BroadcastEvent(std::move(event));
}

}  // namespace extensions
