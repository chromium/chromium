// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_TYPES_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_TYPES_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/image/image.h"

namespace apps {

// Describes the possible ways for the intent picker to be closed.
enum class IntentPickerCloseReason {
  // An error occurred in the intent picker before it could be displayed.
  ERROR_BEFORE_PICKER,

  // An error occurred in the intent picker after it was displayed.
  ERROR_AFTER_PICKER,

  // The user dismissed the picker without making a choice.
  DIALOG_DEACTIVATED,

  // A preferred app was found for launch.
  PREFERRED_APP_FOUND,

  // The user chose to stay in Chrome.
  STAY_IN_CHROME,

  // The user chose to open an app.
  OPEN_APP,
};

// Describes what's the preferred platform for this navigation, if any.
enum class PreferredPlatform {
  // Either there was an error or there is no preferred app at all.
  NONE,

  // The preferred app is Chrome.
  NATIVE_CHROME,

  // The preferred app is an ARC app.
  ARC,

  // TODO(crbug.com/826982) Not needed until app registry is in use.
  // The preferred app is a PWA app.
  PWA,
};

enum class AppsNavigationAction {
  // The current navigation should be cancelled.
  CANCEL,

  // The current navigation should resume.
  RESUME,
};

// This enum backs an UMA histogram and must be treated as append-only.
enum class Source {
  kHttpOrHttps = 0,
  kExternalProtocol = 1,
  kMaxValue = kExternalProtocol
};

// The type of an entry in the intent picker for the user to choose from.
enum class PickerEntryType {
  kUnknown = 0,
  kArc,
  kWeb,
  kDevice,
  kMacNative,
};

// Represents the data required to display an app in a picker to the user.
struct IntentPickerAppInfo {
  IntentPickerAppInfo(PickerEntryType type,
                      const gfx::Image& icon,
                      const std::string& launch_name,
                      const std::string& display_name);

  IntentPickerAppInfo(IntentPickerAppInfo&& other);

  IntentPickerAppInfo& operator=(IntentPickerAppInfo&& other);

  // The type of app that this object represents.
  PickerEntryType type;

  // The icon to be displayed for this app in the picker.
  gfx::Image icon;

  // The string used to launch this app. Represents an Android package name when
  // |type| is kArc, and when |type| is kMacNative, it is the file path of the
  // native app to use.
  std::string launch_name;

  // The string shown to the user to identify this app in the intent picker.
  std::string display_name;

  DISALLOW_COPY_AND_ASSIGN(IntentPickerAppInfo);
};

// Callback to allow app-platform-specific code to asynchronously signal what
// action should be taken for the current navigation, and provide a list of apps
// which can handle the navigation.
using AppsNavigationCallback =
    base::OnceCallback<void(AppsNavigationAction action,
                            std::vector<IntentPickerAppInfo> apps)>;

// Callback to allow app-platform-specific code to asynchronously provide a list
// of apps which can handle the navigation.
using GetAppsCallback =
    base::OnceCallback<void(std::vector<IntentPickerAppInfo> apps)>;

}  // namespace apps

// Callback to pass the launch name and type of the app selected by the user,
// along with the reason why the Bubble was closed and whether the decision
// should be persisted. When the reason is ERROR or DIALOG_DEACTIVATED, the
// values of the launch name, app type, and persistence boolean are all ignored.
using IntentPickerResponse =
    base::OnceCallback<void(const std::string& launch_name,
                            apps::PickerEntryType entry_type,
                            apps::IntentPickerCloseReason close_reason,
                            bool should_persist)>;

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_TYPES_H_
