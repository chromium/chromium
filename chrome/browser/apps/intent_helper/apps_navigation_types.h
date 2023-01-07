// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_TYPES_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_APPS_NAVIGATION_TYPES_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "ui/base/models/image_model.h"

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

  // The preferred app is Chrome browser.
  CHROME_BROWSER,

  // The preferred app is an ARC app.
  ARC,

  // The preferred app is a PWA app.
  PWA,
};

enum class AppsNavigationAction {
  // The current navigation should be cancelled.
  CANCEL,

  // The current navigation should resume.
  RESUME,
};

// The type of an entry in the intent picker for the user to choose from.
enum class PickerEntryType {
  kUnknown = 0,
  kArc,
  kWeb,
  kDevice,
  kMacOs,
};

// Represents the data required to display an app in a picker to the user.
struct IntentPickerAppInfo {
  IntentPickerAppInfo(PickerEntryType type,
                      const ui::ImageModel& icon_model,
                      const std::string& launch_name,
                      const std::string& display_name);

  IntentPickerAppInfo(IntentPickerAppInfo&& other);

  IntentPickerAppInfo(const IntentPickerAppInfo&) = delete;
  IntentPickerAppInfo& operator=(const IntentPickerAppInfo&) = delete;
  IntentPickerAppInfo& operator=(IntentPickerAppInfo&& other);

  // The type of app that this object represents.
  PickerEntryType type;

  // The icon ImageModel to be displayed for this app in the picker.
  ui::ImageModel icon_model;

  // The string used to launch this app. Represents an Android package name when
  // |type| is kArc, and when |type| is kMacOs, it is the file path of the
  // native app to use.
  std::string launch_name;

  // The string shown to the user to identify this app in the intent picker.
  std::string display_name;
};

// The variant of the Intent Picker bubble to display. Used to customize some
// strings and behavior.
enum class IntentPickerBubbleType {
  // Used to select an app to handle http/https links.
  kLinkCapturing,
  // Used to select an app to handle external protocol links (e.g. sms:).
  kExternalProtocol,
  // Special case of kExternalProtocol for tel: links, which can also be handled
  // by Android devices.
  kClickToCall,
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
