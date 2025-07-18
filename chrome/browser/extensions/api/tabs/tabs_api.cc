// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/base_window.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/platform_util.h"
#endif

namespace extensions {

namespace tabs = api::tabs;
namespace windows = api::windows;

namespace tabs_internal {

bool ExtensionHasLockedFullscreenPermission(const Extension* extension) {
  return extension && extension->permissions_data()->HasAPIPermission(
                          mojom::APIPermissionID::kLockWindowFullscreenPrivate);
}

}  // namespace tabs_internal

ExtensionFunction::ResponseAction WindowsGetFunction::Run() {
  std::optional<windows::Get::Params> params =
      windows::Get::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tabs_internal::ApiParameterExtractor<windows::Get::Params> extractor(params);
  WindowController* window_controller = nullptr;
  std::string error;
  if (!windows_util::GetControllerFromWindowID(this, params->window_id,
                                               extractor.type_filters(),
                                               &window_controller, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  WindowController::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? WindowController::kPopulateTabs
                                : WindowController::kDontPopulateTabs;
  base::Value::Dict windows = window_controller->CreateWindowValueForExtension(
      extension(), populate_tab_behavior, source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetCurrentFunction::Run() {
  std::optional<windows::GetCurrent::Params> params =
      windows::GetCurrent::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tabs_internal::ApiParameterExtractor<windows::GetCurrent::Params> extractor(
      params);
  WindowController* window_controller = nullptr;
  std::string error;
  if (!windows_util::GetControllerFromWindowID(
          this, extension_misc::kCurrentWindowId, extractor.type_filters(),
          &window_controller, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  WindowController::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? WindowController::kPopulateTabs
                                : WindowController::kDontPopulateTabs;
  base::Value::Dict windows = window_controller->CreateWindowValueForExtension(
      extension(), populate_tab_behavior, source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetLastFocusedFunction::Run() {
  std::optional<windows::GetLastFocused::Params> params =
      windows::GetLastFocused::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tabs_internal::ApiParameterExtractor<windows::GetLastFocused::Params>
      extractor(params);

  BrowserWindowInterface* last_focused_browser = nullptr;
  std::vector<BrowserWindowInterface*> browsers_by_activation =
      GetBrowserWindowInterfacesOrderedByActivation();
  for (BrowserWindowInterface* browser : browsers_by_activation) {
    if (windows_util::CanOperateOnWindow(
            this, BrowserExtensionWindowController::From(browser),
            extractor.type_filters())) {
      last_focused_browser = browser;
      break;
    }
  }
  if (!last_focused_browser) {
    return RespondNow(Error(tabs_constants::kNoLastFocusedWindowError));
  }

  WindowController::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? WindowController::kPopulateTabs
                                : WindowController::kDontPopulateTabs;
  base::Value::Dict windows = ExtensionTabUtil::CreateWindowValueForExtension(
      *last_focused_browser, extension(), populate_tab_behavior,
      source_context_type());
  return RespondNow(WithArguments(std::move(windows)));
}

ExtensionFunction::ResponseAction WindowsGetAllFunction::Run() {
  std::optional<windows::GetAll::Params> params =
      windows::GetAll::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  tabs_internal::ApiParameterExtractor<windows::GetAll::Params> extractor(
      params);
  base::Value::List window_list;
  WindowController::PopulateTabBehavior populate_tab_behavior =
      extractor.populate_tabs() ? WindowController::kPopulateTabs
                                : WindowController::kDontPopulateTabs;
  for (WindowController* controller : *WindowControllerList::GetInstance()) {
    if (!controller->GetBrowserWindowInterface() ||
        !windows_util::CanOperateOnWindow(this, controller,
                                          extractor.type_filters())) {
      continue;
    }
    window_list.Append(ExtensionTabUtil::CreateWindowValueForExtension(
        *controller->GetBrowserWindowInterface(), extension(),
        populate_tab_behavior, source_context_type()));
  }

  return RespondNow(WithArguments(std::move(window_list)));
}

ExtensionFunction::ResponseAction WindowsRemoveFunction::Run() {
  std::optional<windows::Remove::Params> params =
      windows::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  WindowController* window_controller = nullptr;
  std::string error;
  if (!windows_util::GetControllerFromWindowID(
          this, params->window_id, WindowController::kNoWindowFilter,
          &window_controller, &error)) {
    return RespondNow(Error(std::move(error)));
  }

  // TODO(https://crbug.com/432056907): Determine if we need locked-fullscreen
  // support on desktop android.
#if !BUILDFLAG(IS_ANDROID)
  if (window_controller->GetBrowser() &&
      platform_util::IsBrowserLockedFullscreen(
          window_controller->GetBrowser()) &&
      !tabs_internal::ExtensionHasLockedFullscreenPermission(extension())) {
    return RespondNow(
        Error(tabs_internal::kMissingLockWindowFullscreenPrivatePermission));
  }
#endif

  WindowController::Reason reason;
  if (!window_controller->CanClose(&reason)) {
    return RespondNow(Error(reason == WindowController::REASON_NOT_EDITABLE
                                ? ExtensionTabUtil::kTabStripNotEditableError
                                : kUnknownErrorDoNotUse));
  }
  window_controller->window()->Close();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction TabsGetAllInWindowFunction::Run() {
  std::optional<tabs::GetAllInWindow::Params> params =
      tabs::GetAllInWindow::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;
  if (params->window_id) {
    window_id = *params->window_id;
  }

  std::string error;
  WindowController* window_controller =
      ExtensionTabUtil::GetControllerFromWindowID(
          ChromeExtensionFunctionDetails(this), window_id, &error);
  if (!window_controller) {
    return RespondNow(Error(std::move(error)));
  }

  return RespondNow(WithArguments(
      window_controller->CreateTabList(extension(), source_context_type())));
}

}  // namespace extensions
