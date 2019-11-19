// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/manual_filling_utils.h"

#include <utility>
#include <vector>

namespace autofill {

AccessorySheetData CreateAccessorySheetData(
    AccessoryTabType type,
    base::string16 title,
    std::vector<UserInfo> user_info,
    std::vector<FooterCommand> footer_commands) {
  AccessorySheetData data(type, std::move(title));
  for (auto& i : user_info) {
    data.add_user_info(std::move(i));
  }

  // TODO(crbug.com/902425): Generalize options (both adding to footer, and
  // handling selection).
  for (auto& footer_command : footer_commands) {
    data.add_footer_command(std::move(footer_command));
  }

  return data;
}

}  // namespace autofill
