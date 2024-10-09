// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/windows_util.h"

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace windows_util {

bool GetControllerFromWindowID(ExtensionFunction* function,
                               int window_id,
                               extensions::WindowController::TypeFilter filter,
                               extensions::WindowController** out_controller,
                               std::string* error) {
  DCHECK(out_controller);
  DCHECK(error);

  *out_controller = nullptr;
  if (window_id == extension_misc::kCurrentWindowId) {
    // If there is a window controller associated with this extension, use that.
    if (extensions::WindowController* window_controller =
            function->dispatcher()->GetExtensionWindowController()) {
      *out_controller = window_controller;
      return true;
    }

    // Otherwise get the focused or most recently added window.
    if (extensions::WindowController* window_controller =
            extensions::WindowControllerList::GetInstance()
                ->CurrentWindowForFunctionWithFilter(function, filter)) {
      *out_controller = window_controller;
      return true;
    }

    *error = extensions::ExtensionTabUtil::kNoCurrentWindowError;
    return false;
  } else {
    if (extensions::WindowController* window_controller =
            extensions::WindowControllerList::GetInstance()
                ->FindWindowForFunctionByIdWithFilter(function, window_id,
                                                      filter)) {
      *out_controller = window_controller;
      return true;
    }

    *error = extensions::ErrorUtils::FormatErrorMessage(
        extensions::ExtensionTabUtil::kWindowNotFoundError,
        base::NumberToString(window_id));
    return false;
  }
}

bool CanOperateOnWindow(const ExtensionFunction* function,
                        const extensions::WindowController* controller,
                        extensions::WindowController::TypeFilter filter) {
  if (filter && !controller->MatchesFilter(filter))
    return false;

  // TODO(crbug.com/41367902): Remove this.
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
  return profile->HasPrimaryOTRProfile() &&
         profile->GetPrimaryOTRProfile(/*create_if_needed=*/true) ==
             controller->profile();
}

IncognitoResult ShouldOpenIncognitoWindow(Profile* profile,
                                          std::optional<bool> incognito,
                                          std::vector<GURL>* urls,
                                          std::string* error) {
  const policy::IncognitoModeAvailability incognito_availability =
      IncognitoModePrefs::GetAvailability(profile->GetPrefs());
  bool incognito_result = false;
  if (incognito.has_value()) {
    incognito_result = incognito.value();
    if (incognito_result && incognito_availability ==
                                policy::IncognitoModeAvailability::kDisabled) {
      *error = extensions::tabs_constants::kIncognitoModeIsDisabled;
      return IncognitoResult::kError;
    }
    if (!incognito_result &&
        incognito_availability == policy::IncognitoModeAvailability::kForced) {
      *error = extensions::tabs_constants::kIncognitoModeIsForced;
      return IncognitoResult::kError;
    }
  } else if (incognito_availability ==
             policy::IncognitoModeAvailability::kForced) {
    // If incognito argument is not specified explicitly, we default to
    // incognito when forced so by policy.
    incognito_result = true;
  }

  // Remove all URLs that are not allowed in an incognito session. Note that a
  // ChromeOS guest session is not considered incognito in this case.
  if (incognito_result && !profile->IsGuestSession()) {
    std::string first_url_erased;
    for (size_t i = 0; i < urls->size();) {
      if (IsURLAllowedInIncognito((*urls)[i], profile)) {
        i++;
      } else {
        if (first_url_erased.empty())
          first_url_erased = (*urls)[i].spec();
        urls->erase(urls->begin() + i);
      }
    }
    if (urls->empty() && !first_url_erased.empty()) {
      *error = extensions::ErrorUtils::FormatErrorMessage(
          extensions::tabs_constants::kURLsNotAllowedInIncognitoError,
          first_url_erased);
      return IncognitoResult::kError;
    }
  }
  return incognito_result ? IncognitoResult::kIncognito
                          : IncognitoResult::kRegular;
}

}  // namespace windows_util
