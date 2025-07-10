// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/manual_filling_utils.h"

#include <utility>
#include <vector>

namespace autofill {

AccessorySheetData CreateAccessorySheetData(
    AccessoryTabType type,
    std::u16string user_info_title,
    std::u16string plus_address_title,
    std::vector<UserInfo> user_info,
    std::vector<FooterCommand> footer_commands) {
  AccessorySheetData data(type, std::move(user_info_title),
                          std::move(plus_address_title));
  for (UserInfo& user_info_item : std::move(user_info)) {
    data.add_user_info(std::move(user_info_item));
  }

  // TODO(crbug.com/40601211): Generalize options (both adding to footer, and
  // handling selection).
  for (FooterCommand& footer_command : std::move(footer_commands)) {
    data.add_footer_command(std::move(footer_command));
  }

  return data;
}

}  // namespace autofill
