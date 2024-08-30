// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_MANUAL_FILLING_UTILS_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_MANUAL_FILLING_UTILS_H_

#include <vector>

#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"

namespace autofill {

// Creates an AccessorySheetData defining the data to be shown in the filling
// UI.
AccessorySheetData CreateAccessorySheetData(
    AccessoryTabType type,
    std::u16string title,
    std::u16string plusAddressTitle,
    std::vector<UserInfo> user_info,
    std::vector<FooterCommand> footer_commands);

}  // namespace autofill

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_MANUAL_FILLING_UTILS_H_
