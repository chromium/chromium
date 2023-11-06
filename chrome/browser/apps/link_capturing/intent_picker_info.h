// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_INTENT_PICKER_INFO_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_INTENT_PICKER_INFO_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
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
                      ui::ImageModel icon_model,
                      std::string launch_name,
                      std::string display_name);

  IntentPickerAppInfo(const IntentPickerAppInfo&);
  IntentPickerAppInfo(IntentPickerAppInfo&&);
  IntentPickerAppInfo& operator=(const IntentPickerAppInfo&);
  IntentPickerAppInfo& operator=(IntentPickerAppInfo&&);

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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IntentPickerIconEvent {
  // The intent picker icon was shown in the Omnibox.
  kIconShown = 0,
  // The intent picker icon in the Omnibox was clicked.
  kIconClicked = 1,
  // The intent picker dialog automatically popped out. This has the same
  // effect as kIconClicked, but without the user interaction.
  kAutoPopOut = 2,
  kMaxValue = kAutoPopOut,
};

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

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_INTENT_PICKER_INFO_H_
