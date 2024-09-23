// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/manual_filling_utils.h"

#include <utility>
#include <vector>

namespace autofill {

AccessorySheetData CreateAccessorySheetData(
    AccessoryTabType type,
    std::u16string userInfoTitle,
    std::u16string plusAddressTitle,
    std::vector<UserInfo> user_info,
    std::vector<FooterCommand> footer_commands) {
  AccessorySheetData data(type, std::move(userInfoTitle),
                          std::move(plusAddressTitle));
  for (auto& i : user_info) {
    data.add_user_info(std::move(i));
  }

  // TODO(crbug.com/40601211): Generalize options (both adding to footer, and
  // handling selection).
  for (auto& footer_command : footer_commands) {
    data.add_footer_command(std::move(footer_command));
  }

  return data;
}

}  // namespace autofill
