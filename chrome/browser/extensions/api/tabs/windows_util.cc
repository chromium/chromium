// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/extensions/api/tabs/windows_util.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"

namespace windows_util {

bool GetBrowserFromWindowID(ExtensionFunction* function,
                            int window_id,
                            extensions::WindowController::TypeFilter filter,
                            Browser** browser,
                            std::string* error) {
  DCHECK(browser);
  DCHECK(error);

  *browser = nullptr;
  if (window_id == extension_misc::kCurrentWindowId) {
    // If there is a window controller associated with this extension, use that.
    extensions::WindowController* window_controller =
        function->dispatcher()->GetExtensionWindowController();
    if (!window_controller) {
      // Otherwise get the focused or most recently added window.
      window_controller =
          extensions::WindowControllerList::GetInstance()
              ->CurrentWindowForFunctionWithFilter(function, filter);
    }

    if (window_controller)
      *browser = window_controller->GetBrowser();

    if (!(*browser)) {
      *error = extensions::tabs_constants::kNoCurrentWindowError;
      return false;
    }
  } else {
    extensions::WindowController* window_controller =
        extensions::WindowControllerList::GetInstance()
            ->FindWindowForFunctionByIdWithFilter(function, window_id, filter);
    if (window_controller)
      *browser = window_controller->GetBrowser();

    if (!(*browser)) {
      *error = extensions::ErrorUtils::FormatErrorMessage(
          extensions::tabs_constants::kWindowNotFoundError,
          base::NumberToString(window_id));
      return false;
    }
  }
  DCHECK(*browser);
  return true;
}

bool CanOperateOnWindow(const ExtensionFunction* function,
                        const extensions::WindowController* controller,
                        extensions::WindowController::TypeFilter filter) {
  if (filter && !controller->MatchesFilter(filter))
    return false;

  // TODO(https://crbug.com/807313): Remove this.
  bool allow_dev_tools_windows = !!filter;
  if (function->extension() &&
      !controller->IsVisibleToTabsAPIForExtension(function->extension(),
                                                  allow_dev_tools_windows)) {
    return false;
  }

  if (function->browser_context() == controller->profile())
    return true;

  if (!function->include_incognito_information())
    return false;

  Profile* profile = Profile::FromBrowserContext(function->browser_context());
  return profile->HasOffTheRecordProfile() &&
         profile->GetOffTheRecordProfile() == controller->profile();
}

}  // namespace windows_util
