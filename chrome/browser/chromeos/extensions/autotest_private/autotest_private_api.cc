// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/autotest_private/autotest_private_api.h"

#include <set>
#include <sstream>
#include <utility>

#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/autotest_desks_api.h"
#include "ash/public/cpp/autotest_private_api_utils.h"
#include "ash/public/cpp/default_frame_header.h"
#include "ash/public/cpp/desks_helper.h"
#include "ash/public/cpp/frame_header.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/split_view_test_api.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/mojom/constants.mojom.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/shell.h"
#include "ash/wm/wm_event.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_installer.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/policy_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/assistant/assistant_client.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/crostini/crostini_installer_view.h"
#include "chrome/browser/ui/views/crostini/crostini_uninstaller_view.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_registrar_observer.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/autotest_private.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/policy/core/common/policy_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/filename_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator_history.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

namespace extensions {
namespace {

using chromeos::PrinterClass;

constexpr char kCrostiniNotAvailableForCurrentUserError[] =
    "Crostini is not available for the current user";

// Amount of time to give other processes to report their histograms.
constexpr base::TimeDelta kHistogramsRefreshTimeout =
    base::TimeDelta::FromSeconds(10);

int AccessArray(const volatile int arr[], const volatile int* index) {
  return arr[*index];
}

std::unique_ptr<base::ListValue> GetHostPermissions(const Extension* ext,
                                                    bool effective_perm) {
  const PermissionsData* permissions_data = ext->permissions_data();
  const URLPatternSet& pattern_set =
      effective_perm ? static_cast<const URLPatternSet&>(
                           permissions_data->GetEffectiveHostPermissions(
                               PermissionsData::EffectiveHostPermissionsMode::
                                   kIncludeTabSpecific))
                     : permissions_data->active_permissions().explicit_hosts();

  auto permissions = std::make_unique<base::ListValue>();
  for (URLPatternSet::const_iterator perm = pattern_set.begin();
       perm != pattern_set.end(); ++perm) {
    permissions->AppendString(perm->GetAsString());
  }

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
    case ash::TYPE_APP:
      return api::autotest_private::ShelfItemType::SHELF_ITEM_TYPE_APP;
    case ash::TYPE_PINNED_APP:
      return api::autotest_private::ShelfItemType::SHELF_ITEM_TYPE_PINNEDAPP;
    case ash::TYPE_BROWSER_SHORTCUT:
      return api::autotest_private::ShelfItemType::
          SHELF_ITEM_TYPE_BROWSERSHORTCUT;
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
    case apps::mojom::AppType::kWeb:
      return api::autotest_private::AppType::APP_TYPE_WEB;
    case apps::mojom::AppType::kUnknown:
      return api::autotest_private::AppType::APP_TYPE_NONE;
    case apps::mojom::AppType::kMacNative:
      return api::autotest_private::AppType::APP_TYPE_MACNATIVE;
  }
  NOTREACHED();
  return api::autotest_private::AppType::APP_TYPE_NONE;
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
    case apps::mojom::Readiness::kDisabledByBlacklist:
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
    case apps::mojom::Readiness::kUnknown:
      return api::autotest_private::AppReadiness::APP_READINESS_NONE;
  }
  NOTREACHED();
  return api::autotest_private::AppReadiness::APP_READINESS_NONE;
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
  if (pref_name == chromeos::assistant::prefs::kAssistantEnabled ||
      pref_name == chromeos::assistant::prefs::kAssistantHotwordEnabled) {
    DCHECK(value.is_bool());
    ash::mojom::AssistantAllowedState allowed_state =
        assistant::IsAssistantAllowedForProfile(profile);
    if (allowed_state != ash::mojom::AssistantAllowedState::ALLOWED) {
      return base::StringPrintf("Assistant not allowed - state: %d",
                                allowed_state);
    }
  } else if (pref_name == ash::prefs::kAccessibilityVirtualKeyboardEnabled) {
    DCHECK(value.is_bool());
  } else if (pref_name == prefs::kLanguagePreloadEngines) {
    DCHECK(value.is_string());
  } else {
    return "The pref " + pref_name + "is not whitelisted.";
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
ash::WindowStateType GetExpectedWindowState(
    api::autotest_private::WMEventType event_type) {
  switch (event_type) {
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTNORMAL:
      return ash::WindowStateType::kNormal;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTMAXIMIZE:
      return ash::WindowStateType::kMaximized;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTMINIMIZE:
      return ash::WindowStateType::kMinimized;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTFULLSCREEN:
      return ash::WindowStateType::kFullscreen;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTSNAPLEFT:
      return ash::WindowStateType::kLeftSnapped;
    case api::autotest_private::WMEventType::WM_EVENT_TYPE_WMEVENTSNAPRIGHT:
      return ash::WindowStateType::kRightSnapped;
    default:
      NOTREACHED();
      return ash::WindowStateType::kNormal;
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
    ash::WindowStateType state_type) {
  switch (state_type) {
    // Consider adding DEFAULT type to idl.
    case ash::WindowStateType::kDefault:
    case ash::WindowStateType::kNormal:
      return api::autotest_private::WindowStateType::WINDOW_STATE_TYPE_NORMAL;
    case ash::WindowStateType::kMinimized:
      return api::autotest_private::WindowStateType::
          WINDOW_STATE_TYPE_MINIMIZED;
    case ash::WindowStateType::kMaximized:
      return api::autotest_private::WindowStateType::
          WINDOW_STATE_TYPE_MAXIMIZED;
    case ash::WindowStateType::kFullscreen:
      return api::autotest_private::WindowStateType::
          WINDOW_STATE_TYPE_FULLSCREEN;
    case ash::WindowStateType::kLeftSnapped:
      return api::autotest_private::WindowStateType::
          WINDOW_STATE_TYPE_LEFTSNAPPED;
    case ash::WindowStateType::kRightSnapped:
      return api::autotest_private::WindowStateType::
          WINDOW_STATE_TYPE_RIGHTSNAPPED;
    case ash::WindowStateType::kPip:
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

ui::KeyboardCode StringToKeyCode(const std::string& str) {
  constexpr struct Map {
    const char* str;
    ui::KeyboardCode key_code;
  } map[] = {
      {"search", ui::VKEY_LWIN},
  };
  DCHECK(base::IsStringASCII(str));
  if (str.length() == 1) {
    char c = str[0];
    if (c >= 'a' || c <= 'z') {
      return static_cast<ui::KeyboardCode>(static_cast<int>(ui::VKEY_A) +
                                           (c - 'a'));
    }
    if (c >= '0' || c <= '9') {
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

}  // namespace

class WindowStateChangeObserver : public aura::WindowObserver {
 public:
  WindowStateChangeObserver(aura::Window* window,
                            ash::WindowStateType expected_type,
                            base::OnceCallback<void(bool)> callback)
      : expected_type_(expected_type), callback_(std::move(callback)) {
    DCHECK_NE(window->GetProperty(ash::kWindowStateTypeKey), expected_type_);
    scoped_observer_.Add(window);
  }
  ~WindowStateChangeObserver() override {}

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK(scoped_observer_.IsObserving(window));
    if (key == ash::kWindowStateTypeKey &&
        window->GetProperty(ash::kWindowStateTypeKey) == expected_type_) {
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
  ash::WindowStateType expected_type_;
  ScopedObserver<aura::Window, aura::WindowObserver> scoped_observer_{this};
  base::OnceCallback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(WindowStateChangeObserver);
};

class EventGenerator {
 public:
  EventGenerator(aura::WindowTreeHost* host, base::OnceClosure closure)
      : host_(host), closure_(std::move(closure)), weak_ptr_factory_(this) {}
  ~EventGenerator() = default;

  void ScheduleMouseEvent(ui::EventType type,
                          gfx::PointF location_in_host,
                          int flags) {
    int last_flags = (events_.empty())
                         ? aura::Env::GetInstance()->mouse_button_flags()
                         : events_.back()->flags();
    events_.push_back(std::make_unique<ui::MouseEvent>(
        type, location_in_host, location_in_host, base::TimeTicks::Now(), flags,
        flags ^ last_flags));
  }

  void Run() {
    last_event_timestamp_ = base::TimeTicks::Now();
    SendEvent();
  }

  base::TimeDelta GetInterval() const {
    return base::TimeDelta::FromSeconds(1) /
           host_->compositor()->refresh_rate();
  }

 private:
  void SendEvent() {
    if (events_.empty()) {
      std::move(closure_).Run();
      return;
    }
    auto event = ui::Event::Clone(*events_.front());
    events_.pop_front();
    {
      ui::Event::DispatcherApi api(event.get());
      api.set_time_stamp(base::TimeTicks::Now());
    }
    host_->SendEventToSink(event.get());
    base::TimeTicks current_timestamp = base::TimeTicks::Now();
    base::TimeDelta interval =
        last_event_timestamp_ + GetInterval() - current_timestamp;
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EventGenerator::SendEvent,
                       weak_ptr_factory_.GetWeakPtr()),
        interval);
    last_event_timestamp_ = current_timestamp;
  }

  base::TimeTicks last_event_timestamp_;
  aura::WindowTreeHost* const host_;
  base::OnceClosure closure_;
  std::deque<std::unique_ptr<ui::Event>> events_;

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
  return RespondNow(OneArgument(std::move(result)));
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

  base::Value all_policies_array = policy::DictionaryPolicyConversions()
                                       .WithBrowserContext(browser_context())
                                       .EnableDeviceLocalAccountPolicies(true)
                                       .EnableDeviceInfo(true)
                                       .ToValue();

  return RespondNow(OneArgument(
      base::Value::ToUniquePtrValue(std::move(all_policies_array))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateRefreshEnterprisePoliciesFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateRefreshEnterprisePoliciesFunction::
    ~AutotestPrivateRefreshEnterprisePoliciesFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateRefreshEnterprisePoliciesFunction::Run() {
  DVLOG(1) << "AutotestPrivateRefreshEnterprisePoliciesFunction";

  g_browser_process->policy_service()->RefreshPolicies(base::Bind(
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

    Manifest::Location location = extension->location();
    extension_value->SetBoolean("isComponent", location == Manifest::COMPONENT);
    extension_value->SetBoolean("isInternal", location == Manifest::INTERNAL);
    extension_value->SetBoolean("isUserInstalled",
                                location == Manifest::INTERNAL ||
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
  return RespondNow(OneArgument(std::move(return_value)));
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
  return RespondNow(OneArgument(std::move(values)));
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

  double start_time =
      (arc_session_manager->arc_start_time() - base::TimeTicks())
          .InMillisecondsF();
  return RespondNow(OneArgument(std::make_unique<base::Value>(start_time)));
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

  arc_state.provisioned = arc::IsArcProvisioned(profile);
  arc_state.tos_needed = arc::IsArcTermsOfServiceNegotiationNeeded(profile);
  return RespondNow(OneArgument(arc_state.ToValue()));
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
  return RespondNow(OneArgument(play_store_state.ToValue()));
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
// AutotestPrivateGetHistogramFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetHistogramFunction::~AutotestPrivateGetHistogramFunction() =
    default;

ExtensionFunction::ResponseAction AutotestPrivateGetHistogramFunction::Run() {
  std::unique_ptr<api::autotest_private::GetHistogram::Params> params(
      api::autotest_private::GetHistogram::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateGetHistogramFunction " << params->name;

  // Collect histogram data from other processes before responding. Otherwise,
  // we'd report stale data for histograms that are e.g. recorded by renderers.
  content::FetchHistogramsAsynchronously(
      base::ThreadTaskRunnerHandle::Get(),
      base::BindRepeating(
          &AutotestPrivateGetHistogramFunction::RespondOnHistogramsFetched,
          this, params->name),
      kHistogramsRefreshTimeout);
  return RespondLater();
}

void AutotestPrivateGetHistogramFunction::RespondOnHistogramsFetched(
    const std::string& name) {
  // Incorporate the data collected by content::FetchHistogramsAsynchronously().
  base::StatisticsRecorder::ImportProvidedHistograms();
  Respond(GetHistogram(name));
}

ExtensionFunction::ResponseValue
AutotestPrivateGetHistogramFunction::GetHistogram(const std::string& name) {
  const base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(name);
  if (!histogram)
    return Error(base::StrCat({"Histogram ", name, " not found"}));

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotSamples();
  api::autotest_private::Histogram result;

  for (std::unique_ptr<base::SampleCountIterator> it = samples->Iterator();
       !it->Done(); it->Next()) {
    base::HistogramBase::Sample min = 0;
    int64_t max = 0;
    base::HistogramBase::Count count = 0;
    it->Get(&min, &max, &count);

    api::autotest_private::HistogramBucket bucket;
    bucket.min = min;
    bucket.max = max;
    bucket.count = count;
    result.buckets.push_back(std::move(bucket));
  }

  return OneArgument(result.ToValue());
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
  return RespondNow(
      OneArgument(std::make_unique<base::Value>(window_attached)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateIsAppShownFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateIsArcProvisionedFunction::
    ~AutotestPrivateIsArcProvisionedFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateIsArcProvisionedFunction::Run() {
  DVLOG(1) << "AutotestPrivateIsArcProvisionedFunction";
  return RespondNow(OneArgument(std::make_unique<base::Value>(
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

  return RespondNow(OneArgument(std::move(app_value)));
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
  return RespondNow(OneArgument(std::move(package_value)));
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
  return RespondNow(OneArgument(std::make_unique<base::Value>(result)));
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
  base::string16 data;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &data);
  return RespondNow(
      OneArgument(base::Value::ToUniquePtrValue(base::Value(data))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetClipboardTextDataFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetClipboardTextDataFunction::
    ~AutotestPrivateSetClipboardTextDataFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetClipboardTextDataFunction::Run() {
  std::unique_ptr<api::autotest_private::SetClipboardTextData::Params> params(
      api::autotest_private::SetClipboardTextData::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const base::string16 data = base::UTF8ToUTF16(params->data);
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteText(data);

  return RespondNow(NoArguments());
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
  if (!crostini::CrostiniFeatures::Get()->IsUIAllowed(profile))
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
  if (!crostini::CrostiniFeatures::Get()->IsUIAllowed(profile))
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));

  // Run GUI installer which will install crostini vm / container and
  // start terminal app on completion.  After starting the installer,
  // we call RestartCrostini and we will be put in the pending restarters
  // queue and be notified on success/otherwise of installation.
  CrostiniInstallerView::Show(
      profile, crostini::CrostiniInstaller::GetForProfile(profile));
  CrostiniInstallerView::GetActiveViewForTesting()->Accept();
  crostini::CrostiniManager::GetForProfile(profile)->RestartCrostini(
      crostini::kCrostiniDefaultVmName, crostini::kCrostiniDefaultContainerName,
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
  if (!crostini::CrostiniFeatures::Get()->IsUIAllowed(profile))
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
  if (!crostini::CrostiniFeatures::Get()->IsUIAllowed(profile)) {
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));
  }

  base::FilePath path(params->path);
  if (path.ReferencesParent()) {
    return RespondNow(Error("Invalid export path must not reference parent"));
  }

  crostini::CrostiniExportImport::GetForProfile(profile)->ExportContainer(
      crostini::ContainerId(crostini::kCrostiniDefaultVmName,
                            crostini::kCrostiniDefaultContainerName),
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
  if (!crostini::CrostiniFeatures::Get()->IsUIAllowed(profile))
    return RespondNow(Error(kCrostiniNotAvailableForCurrentUserError));

  base::FilePath path(params->path);
  if (path.ReferencesParent()) {
    return RespondNow(Error("Invalid import path must not reference parent"));
  }
  crostini::CrostiniExportImport::GetForProfile(profile)->ImportContainer(
      crostini::ContainerId(crostini::kCrostiniDefaultVmName,
                            crostini::kCrostiniDefaultContainerName),
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
  // TODO(mash): Fix for mash, http://crbug.com/557397
  aura::Window* primary_root = ash::Shell::GetPrimaryRootWindow();
  // Pass the ScreenshotGrabber to the callback so that it stays alive for the
  // duration of the operation, it'll then get deallocated when the callback
  // completes.
  grabber->TakeScreenshot(
      primary_root, primary_root->bounds(),
      base::BindOnce(&AutotestPrivateTakeScreenshotFunction::ScreenshotTaken,
                     this, base::Passed(&grabber)));
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
  Respond(
      OneArgument(std::make_unique<base::Value>(GetPngDataAsString(png_data))));
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
      grabber->TakeScreenshot(
          window, window->bounds(),
          base::BindOnce(
              &AutotestPrivateTakeScreenshotForDisplayFunction::ScreenshotTaken,
              this, base::Passed(&grabber)));
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
  Respond(
      OneArgument(std::make_unique<base::Value>(GetPngDataAsString(png_data))));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetPrinterListFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetPrinterListFunction::AutotestPrivateGetPrinterListFunction()
    : results_(std::make_unique<base::Value>(base::Value::Type::LIST)) {}

AutotestPrivateGetPrinterListFunction::
    ~AutotestPrivateGetPrinterListFunction() {
  printers_manager_->RemoveObserver(this);
}

ExtensionFunction::ResponseAction AutotestPrivateGetPrinterListFunction::Run() {
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

void AutotestPrivateGetPrinterListFunction::RespondWithTimeoutError() {
  if (did_respond())
    return;
  Respond(
      Error("Timeout occurred before Enterprise printers were initialized"));
}

void AutotestPrivateGetPrinterListFunction::RespondWithSuccess() {
  if (did_respond())
    return;
  Respond(OneArgument(std::move(results_)));
  timeout_timer_.AbandonAndStop();
}

void AutotestPrivateGetPrinterListFunction::OnEnterprisePrintersInitialized() {
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
  // We have to respond in separate task, because it will cause a destruction of
  // CupsPrintersManager
  PostTask(
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

  if (js_printer.printer_uri)
    printer.set_uri(*js_printer.printer_uri);

  if (js_printer.printer_ppd) {
    const GURL ppd =
        net::FilePathToFileURL(base::FilePath(*js_printer.printer_ppd));
    if (ppd.is_valid())
      printer.mutable_ppd_reference()->user_supplied_ppd_url = ppd.spec();
    else
      LOG(ERROR) << "Invalid ppd path: " << *js_printer.printer_ppd;
  }
  auto printers_manager = chromeos::CupsPrintersManager::Create(
      Profile::FromBrowserContext(browser_context()));
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

  auto printers_manager = chromeos::CupsPrintersManager::Create(
      Profile::FromBrowserContext(browser_context()));
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
      ->LoadBuiltinModel(
          chromeos::machine_learning::mojom::BuiltinModelSpec::New(
              chromeos::machine_learning::mojom::BuiltinModelId::TEST_MODEL),
          mojo::MakeRequest(&model_),
          base::BindOnce(
              &AutotestPrivateBootstrapMachineLearningServiceFunction::
                  ModelLoaded,
              this));
  model_.set_connection_error_handler(base::BindOnce(
      &AutotestPrivateBootstrapMachineLearningServiceFunction::ConnectionError,
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

void AutotestPrivateBootstrapMachineLearningServiceFunction::ConnectionError() {
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
  const bool not_ready = (ash::AssistantState::Get()->assistant_state() ==
                          ash::mojom::AssistantState::NOT_READY);
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
    ash::mojom::AssistantState state) {
  // Must check if the Optional contains value first to avoid possible
  // segmentation fault caused by Respond() below being called before
  // RespondLater() in Run(). This will happen due to AddObserver() call
  // in the constructor will trigger this function immediately.
  if (!enabled_.has_value())
    return;

  const bool not_ready = (state == ash::mojom::AssistantState::NOT_READY);
  const bool success = (enabled_.value() != not_ready);
  if (!success)
    return;

  Respond(NoArguments());
  enabled_.reset();
  timeout_timer_.AbandonAndStop();
}

void AutotestPrivateSetAssistantEnabledFunction::Timeout() {
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
  PostTask(
      FROM_HERE, {base::CurrentThread()},
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
    OnAssistantStatusChanged(ash::mojom::AssistantState state) {
  if (state == ash::mojom::AssistantState::NEW_READY) {
    Respond(NoArguments());
    self_.reset();
  }
}

// AssistantInteractionHelper is a helper class used to interact with Assistant
// server and store interaction states for tests. It is shared by
// |AutotestPrivateSendAssistantTextQueryFunction| and
// |AutotestPrivateWaitForAssistantQueryStatusFunction|.
class AssistantInteractionHelper
    : public chromeos::assistant::mojom::AssistantInteractionSubscriber {
 public:
  using OnInteractionFinishedCallback = base::OnceCallback<void(bool)>;

  AssistantInteractionHelper()
      : query_status_(std::make_unique<base::DictionaryValue>()) {}

  ~AssistantInteractionHelper() override = default;

  void Init(OnInteractionFinishedCallback on_interaction_finished_callback) {
    // Bind to Assistant service interface.
    AssistantClient::Get()->BindAssistant(
        assistant_.BindNewPipeAndPassReceiver());

    // Subscribe to Assistant interaction events.
    assistant_->AddAssistantInteractionSubscriber(
        assistant_interaction_subscriber_receiver_.BindNewPipeAndPassRemote());

    on_interaction_finished_callback_ =
        std::move(on_interaction_finished_callback);
  }

  void SendTextQuery(const std::string& query, bool allow_tts) {
    // Start text interaction with Assistant server.
    assistant_->StartTextInteraction(
        query, chromeos::assistant::mojom::AssistantQuerySource::kUnspecified,
        allow_tts);

    query_status_->SetKey("queryText", base::Value(query));
  }

  std::unique_ptr<base::DictionaryValue> GetQueryStatus() {
    return std::move(query_status_);
  }

 private:
  // chromeos::assistant::mojom::AssistantInteractionSubscriber:
  using AssistantSuggestionPtr =
      chromeos::assistant::mojom::AssistantSuggestionPtr;
  using AssistantInteractionMetadataPtr =
      chromeos::assistant::mojom::AssistantInteractionMetadataPtr;
  using AssistantInteractionResolution =
      chromeos::assistant::mojom::AssistantInteractionResolution;

  void OnInteractionStarted(AssistantInteractionMetadataPtr metadata) override {
    const bool is_voice_interaction =
        chromeos::assistant::mojom::AssistantInteractionType::kVoice ==
        metadata->type;
    query_status_->SetKey("isMicOpen", base::Value(is_voice_interaction));
  }

  void OnInteractionFinished(
      AssistantInteractionResolution resolution) override {
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
      const bool success =
          (resolution == AssistantInteractionResolution::kNormal) ? true
                                                                  : false;
      std::move(on_interaction_finished_callback_).Run(success);
    }
  }

  void OnHtmlResponse(const std::string& response,
                      const std::string& fallback) override {
    result_.SetKey("htmlResponse", base::Value(response));
    result_.SetKey("htmlFallback", base::Value(fallback));
  }

  void OnTextResponse(const std::string& response) override {
    result_.SetKey("text", base::Value(response));
  }

  void OnSpeechRecognitionFinalResult(
      const std::string& final_result) override {
    query_status_->SetKey("queryText", base::Value(final_result));
  }

  void OnSuggestionsResponse(
      std::vector<AssistantSuggestionPtr> response) override {}
  void OnOpenUrlResponse(const GURL& url, bool in_background) override {}
  void OnOpenAppResponse(chromeos::assistant::mojom::AndroidAppInfoPtr app_info,
                         OnOpenAppResponseCallback callback) override {}
  void OnSpeechRecognitionStarted() override {}
  void OnSpeechRecognitionIntermediateResult(
      const std::string& high_confidence_text,
      const std::string& low_confidence_text) override {}
  void OnSpeechRecognitionEndOfUtterance() override {}
  void OnSpeechLevelUpdated(float speech_level) override {}
  void OnTtsStarted(bool due_to_error) override {}
  void OnWaitStarted() override {}

  mojo::Remote<chromeos::assistant::mojom::Assistant> assistant_;
  mojo::Receiver<chromeos::assistant::mojom::AssistantInteractionSubscriber>
      assistant_interaction_subscriber_receiver_{this};
  std::unique_ptr<base::DictionaryValue> query_status_;
  base::DictionaryValue result_;

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
  ash::mojom::AssistantAllowedState allowed_state =
      assistant::IsAssistantAllowedForProfile(profile);
  if (allowed_state != ash::mojom::AssistantAllowedState::ALLOWED) {
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
    OnInteractionFinishedCallback(bool success) {
  if (!success) {
    Respond(Error("Interaction ends abnormally."));
    timeout_timer_.AbandonAndStop();
    return;
  }

  Respond(OneArgument(interaction_helper_->GetQueryStatus()));
  timeout_timer_.AbandonAndStop();
}

void AutotestPrivateSendAssistantTextQueryFunction::Timeout() {
  Respond(Error("Assistant response timeout."));
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
  ash::mojom::AssistantAllowedState allowed_state =
      assistant::IsAssistantAllowedForProfile(profile);
  if (allowed_state != ash::mojom::AssistantAllowedState::ALLOWED) {
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
    OnInteractionFinishedCallback(bool success) {
  if (!success) {
    Respond(Error("Interaction ends abnormally."));
    timeout_timer_.AbandonAndStop();
    return;
  }

  Respond(OneArgument(interaction_helper_->GetQueryStatus()));
  timeout_timer_.AbandonAndStop();
}

void AutotestPrivateWaitForAssistantQueryStatusFunction::Timeout() {
  Respond(Error("No query response received before time out."));
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

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(
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
  return RespondNow(OneArgument(std::make_unique<base::Value>(scale_factor)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateIsTabletModeEnabledFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateIsTabletModeEnabledFunction::
    ~AutotestPrivateIsTabletModeEnabledFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateIsTabletModeEnabledFunction::Run() {
  DVLOG(1) << "AutotestPrivateIsTabletModeEnabledFunction";

  return RespondNow(OneArgument(
      std::make_unique<base::Value>(ash::TabletMode::Get()->InTabletMode())));
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
  ash::TabletMode::Waiter waiter(params->enabled);
  ash::TabletMode::Get()->SetEnabledForTest(params->enabled);
  waiter.Wait();
  return RespondNow(OneArgument(
      std::make_unique<base::Value>(ash::TabletMode::Get()->InTabletMode())));
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

  if (!proxy)
    return RespondNow(Error("App Service not available"));

  std::vector<api::autotest_private::App> installed_apps;
  proxy->AppRegistryCache().ForEachApp([&installed_apps](
                                           const apps::AppUpdate& update) {
    api::autotest_private::App app;
    app.app_id = update.AppId();
    app.name = update.Name();
    app.short_name = update.ShortName();
    app.additional_search_terms = update.AdditionalSearchTerms();
    app.type = GetAppType(update.AppType());
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
    case ash::ShelfAutoHideBehavior::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      str_behavior = "always";
      break;
    case ash::ShelfAutoHideBehavior::SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      str_behavior = "never";
      break;
    case ash::ShelfAutoHideBehavior::SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
      // SHELF_AUTO_HIDE_ALWAYS_HIDDEN not supported by shelf_prefs.cc
      return RespondNow(Error("SHELF_AUTO_HIDE_ALWAYS_HIDDEN not supported"));
  }
  return RespondNow(
      OneArgument(std::make_unique<base::Value>(std::move(str_behavior))));
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
    behavior = ash::ShelfAutoHideBehavior::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  } else if (params->behavior == "never") {
    behavior = ash::ShelfAutoHideBehavior::SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
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
    case ash::ShelfAlignment::SHELF_ALIGNMENT_BOTTOM:
      alignment_type = api::autotest_private::ShelfAlignmentType::
          SHELF_ALIGNMENT_TYPE_BOTTOM;
      break;
    case ash::ShelfAlignment::SHELF_ALIGNMENT_LEFT:
      alignment_type =
          api::autotest_private::ShelfAlignmentType::SHELF_ALIGNMENT_TYPE_LEFT;
      break;
    case ash::ShelfAlignment::SHELF_ALIGNMENT_RIGHT:
      alignment_type =
          api::autotest_private::ShelfAlignmentType::SHELF_ALIGNMENT_TYPE_RIGHT;
      break;
    case ash::ShelfAlignment::SHELF_ALIGNMENT_BOTTOM_LOCKED:
      // SHELF_ALIGNMENT_BOTTOM_LOCKED not supported by shelf_prefs.cc
      return RespondNow(Error("SHELF_ALIGNMENT_BOTTOM_LOCKED not supported"));
  }
  return RespondNow(OneArgument(std::make_unique<base::Value>(
      api::autotest_private::ToString(alignment_type))));
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
      alignment = ash::ShelfAlignment::SHELF_ALIGNMENT_BOTTOM;
      break;
    case api::autotest_private::ShelfAlignmentType::SHELF_ALIGNMENT_TYPE_LEFT:
      alignment = ash::ShelfAlignment::SHELF_ALIGNMENT_LEFT;
      break;
    case api::autotest_private::ShelfAlignmentType::SHELF_ALIGNMENT_TYPE_RIGHT:
      alignment = ash::ShelfAlignment::SHELF_ALIGNMENT_RIGHT;
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
  auto arg = OneArgument(std::make_unique<base::Value>(finished));
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
// AutotestPrivateGetArcAppWindowInfoFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetArcAppWindowInfoFunction::
    AutotestPrivateGetArcAppWindowInfoFunction() = default;
AutotestPrivateGetArcAppWindowInfoFunction::
    ~AutotestPrivateGetArcAppWindowInfoFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetArcAppWindowInfoFunction::Run() {
  std::unique_ptr<api::autotest_private::GetArcAppWindowInfo::Params> params(
      api::autotest_private::GetArcAppWindowInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateGetArcAppWindowInfoFunction "
           << params->package_name;

  aura::Window* arc_window = GetArcAppWindow(params->package_name);
  if (!arc_window) {
    return RespondNow(Error(base::StrCat(
        {"No ARC app window was found for ", params->package_name})));
  }

  const gfx::Rect bounds = arc_window->GetBoundsInRootWindow();
  const bool is_animating = arc_window->layer()->GetAnimator()->is_animating();
  const auto* screen = display::Screen::GetScreen();
  const int64_t display_id =
      !screen ? -1 : screen->GetDisplayNearestWindow(arc_window).id();

  api::autotest_private::ArcAppWindowInfo result;
  result.bounds = ToBoundsDictionary(bounds);
  result.display_id = base::NumberToString(display_id);
  result.is_animating = is_animating;
  result.is_visible = arc_window->IsVisible();

  return RespondNow(OneArgument(
      api::autotest_private::GetArcAppWindowInfo::Results::Create(result)));
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
  Respond(OneArgument(std::move(result)));
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateSetArcAppWindowStateFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateSetArcAppWindowStateFunction::
    AutotestPrivateSetArcAppWindowStateFunction() = default;
AutotestPrivateSetArcAppWindowStateFunction::
    ~AutotestPrivateSetArcAppWindowStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateSetArcAppWindowStateFunction::Run() {
  std::unique_ptr<api::autotest_private::SetArcAppWindowState::Params> params(
      api::autotest_private::SetArcAppWindowState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateSetArcAppWindowStateFunction "
           << params->package_name;

  aura::Window* arc_window = GetArcAppWindow(params->package_name);
  if (!arc_window) {
    return RespondNow(Error(base::StrCat(
        {"No ARC app window was found for ", params->package_name})));
  }

  ash::WindowStateType expected_state =
      GetExpectedWindowState(params->change.event_type);
  if (arc_window->GetProperty(ash::kWindowStateTypeKey) == expected_state) {
    if (params->change.fail_if_no_change &&
        *(params->change.fail_if_no_change)) {
      return RespondNow(Error(
          "The ARC app window is already in the expected window state! "));
    } else {
      return RespondNow(OneArgument(std::make_unique<base::Value>(
          api::autotest_private::ToString(ToWindowStateType(expected_state)))));
    }
  }

  window_state_observer_ = std::make_unique<WindowStateChangeObserver>(
      arc_window, expected_state,
      base::BindOnce(
          &AutotestPrivateSetArcAppWindowStateFunction::WindowStateChanged,
          this, expected_state));

  // TODO(crbug.com/990713): Make WMEvent trigger split view in tablet mode.
  if (ash::TabletMode::Get()->InTabletMode()) {
    if (expected_state == ash::WindowStateType::kLeftSnapped) {
      ash::SplitViewTestApi().SnapWindow(
          arc_window, ash::SplitViewTestApi::SnapPosition::LEFT);
    } else if (expected_state == ash::WindowStateType::kRightSnapped) {
      ash::SplitViewTestApi().SnapWindow(
          arc_window, ash::SplitViewTestApi::SnapPosition::RIGHT);
    }
    return RespondLater();
  }

  const ash::WMEvent event(ToWMEventType(params->change.event_type));
  ash::WindowState::Get(arc_window)->OnWMEvent(&event);

  return RespondLater();
}

void AutotestPrivateSetArcAppWindowStateFunction::WindowStateChanged(
    ash::WindowStateType expected_type,
    bool success) {
  if (!success) {
    Respond(Error(
        "ARC app window was destroyed while waiting for its state change! "));
  } else {
    Respond(OneArgument(std::make_unique<base::Value>(
        api::autotest_private::ToString(ToWindowStateType(expected_type)))));
  }
}

///////////////////////////////////////////////////////////////////////////////
// AutotestPrivateGetArcAppWindowStateFunction
///////////////////////////////////////////////////////////////////////////////

AutotestPrivateGetArcAppWindowStateFunction::
    AutotestPrivateGetArcAppWindowStateFunction() = default;
AutotestPrivateGetArcAppWindowStateFunction::
    ~AutotestPrivateGetArcAppWindowStateFunction() = default;

ExtensionFunction::ResponseAction
AutotestPrivateGetArcAppWindowStateFunction::Run() {
  std::unique_ptr<api::autotest_private::GetArcAppWindowState::Params> params(
      api::autotest_private::GetArcAppWindowState::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  DVLOG(1) << "AutotestPrivateGetArcAppWindowStateFunction "
           << params->package_name;

  aura::Window* arc_window = GetArcAppWindow(params->package_name);
  if (!arc_window) {
    return RespondNow(Error(base::StrCat(
        {"No ARC app window was found for ", params->package_name})));
  }

  return RespondNow(OneArgument(std::make_unique<base::Value>(
      api::autotest_private::ToString(ToWindowStateType(
          arc_window->GetProperty(ash::kWindowStateTypeKey))))));
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
  target_rotation_ = ToRotation(params->rotation);

  if (!base::StringToInt64(params->display_id, &display_id_)) {
    return RespondNow(Error(base::StrCat(
        {"Invalid display_id; expected string with numbers only, got ",
         params->display_id})));
  }
  auto* root_window = ash::Shell::GetRootWindowForDisplayId(display_id_);
  if (!root_window) {
    return RespondNow(Error(base::StrCat(
        {"Invalid display_id; no root window found for the display id ",
         params->display_id})));
  }
  auto* animator = ash::ScreenRotationAnimator::GetForRootWindow(root_window);
  if (!animator->IsRotating()) {
    display::Display display;
    display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id_,
                                                          &display);
    // This should never fail.
    DCHECK(display.is_valid());
    return RespondNow(OneArgument(
        std::make_unique<base::Value>(display.rotation() == target_rotation_)));
  }
  self_ = this;

  animator->AddObserver(this);
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
  Respond(OneArgument(std::make_unique<base::Value>(
      display.is_valid() && display.rotation() == target_rotation_)));
  self_.reset();
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
        ToWindowStateType(window->GetProperty(ash::kWindowStateTypeKey));
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

    if (window->GetProperty(aura::client::kAppType) ==
        static_cast<int>(ash::AppType::ARC_APP)) {
      window_info.arc_package_name = std::make_unique<std::string>(
          *window->GetProperty(ash::kArcPackageNameKey));
    }

    // Frame information
    auto* immersive_controller = ash::ImmersiveFullscreenController::Get(
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
      auto* frame_header = ash::FrameHeader::Get(widget);
      window_info.caption_height = frame_header->GetHeaderHeight();

      const ash::CaptionButtonModel* button_model =
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

  ash::WindowStateType expected_state =
      GetExpectedWindowState(params->change.event_type);
  if (window->GetProperty(ash::kWindowStateTypeKey) == expected_state) {
    if (params->change.fail_if_no_change &&
        *(params->change.fail_if_no_change)) {
      return RespondNow(
          Error("The app window was already in the expected window state! "));
    } else {
      return RespondNow(OneArgument(std::make_unique<base::Value>(
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
    if (expected_state == ash::WindowStateType::kLeftSnapped) {
      ash::SplitViewTestApi().SnapWindow(
          window, ash::SplitViewTestApi::SnapPosition::LEFT);
    } else if (expected_state == ash::WindowStateType::kRightSnapped) {
      ash::SplitViewTestApi().SnapWindow(
          window, ash::SplitViewTestApi::SnapPosition::RIGHT);
    }
    return RespondLater();
  }

  const ash::WMEvent event(ToWMEventType(params->change.event_type));
  ash::WindowState::Get(window)->OnWMEvent(&event);

  return RespondLater();
}

void AutotestPrivateSetAppWindowStateFunction::WindowStateChanged(
    ash::WindowStateType expected_type,
    bool success) {
  if (!success) {
    Respond(Error(
        "The app window was destroyed while waiting for its state change! "));
  } else {
    Respond(OneArgument(std::make_unique<base::Value>(
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
    : public banners::AppBannerManager::Observer {
 public:
  PWABannerObserver(banners::AppBannerManager* manager,
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

  void OnAppBannerManagerChanged(
      banners::AppBannerManager* new_manager) override {
    observer_.RemoveAll();
    observer_.Add(new_manager);
    app_banner_manager_ = new_manager;
  }

  void OnInstallableWebAppStatusUpdated() override {
    Installable installable =
        app_banner_manager_->GetInstallableWebAppCheckResultForTesting();
    switch (installable) {
      case Installable::kNo:
        FALLTHROUGH;
      case Installable::kUnknown:
        DCHECK(false) << "Unexpected AppBannerManager::Installable value (kNo "
                         "or kUnknown)";
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
  using Installable = banners::AppBannerManager::InstallableWebAppCheckResult;

  ScopedObserver<banners::AppBannerManager, banners::AppBannerManager::Observer>
      observer_{this};
  base::OnceCallback<void()> callback_;
  banners::AppBannerManager* app_banner_manager_;

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

  banners::AppBannerManager* app_banner_manager =
      banners::AppBannerManagerDesktop::FromWebContents(web_contents);
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
  Respond(OneArgument(std::make_unique<base::Value>(app_id)));
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
    return RespondNow(OneArgument(std::make_unique<base::Value>(result)));
  }
  bool result = accelerator_controller->Process(accelerator);
  return RespondNow(OneArgument(std::make_unique<base::Value>(result)));
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
          base::Bind(&AutotestPrivateWaitForLauncherStateFunction::Done,
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
  return RespondNow(OneArgument(std::make_unique<base::Value>(success)));
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
    return RespondNow(OneArgument(std::make_unique<base::Value>(false)));
  }

  return RespondLater();
}

void AutotestPrivateActivateDeskAtIndexFunction::OnAnimationComplete() {
  Respond(OneArgument(std::make_unique<base::Value>(true)));
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
    return RespondNow(OneArgument(std::make_unique<base::Value>(false)));
  }

  return RespondLater();
}

void AutotestPrivateRemoveActiveDeskFunction::OnAnimationComplete() {
  Respond(OneArgument(std::make_unique<base::Value>(true)));
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
                                       0);
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
  int flags = env->mouse_button_flags() | GetMouseEventFlags(params->button);
  if (flags == env->mouse_button_flags())
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
                                       flags);
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

  int flags = env->mouse_button_flags() & (~GetMouseEventFlags(params->button));
  if (flags == env->mouse_button_flags())
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
                                       flags);
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

  auto* host = root_window->GetHost();
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
      host, base::BindOnce(&AutotestPrivateMouseMoveFunction::Respond, this,
                           NoArguments()));
  gfx::PointF start_in_host(last_mouse_location.x(), last_mouse_location.y());
  wm::ConvertPointFromScreen(root_window, &start_in_host);
  ConvertPointToHost(root_window, &start_in_host);

  int64_t steps =
      std::max(base::TimeDelta::FromMilliseconds(params->duration_in_ms) /
                   event_generator_->GetInterval(),
               static_cast<int64_t>(1));
  int flags = env->mouse_button_flags();
  ui::EventType type = (flags == 0) ? ui::ET_MOUSE_MOVED : ui::ET_MOUSE_DRAGGED;
  for (int64_t i = 1; i <= steps; ++i) {
    double progress = double(i) / double(steps);
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
