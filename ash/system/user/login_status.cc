// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/login_status.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace user {

std::u16string GetLocalizedSignOutStringForStatus(LoginStatus status,
                                                  bool multiline) {
  int message_id;
  switch (status) {
    case LoginStatus::GUEST:
      message_id = IDS_ASH_STATUS_TRAY_EXIT_GUEST;
      break;
    case LoginStatus::PUBLIC:
      message_id = IDS_ASH_STATUS_TRAY_EXIT_PUBLIC;
      break;
    default:
      message_id =
          Shell::Get()->session_controller()->NumberOfLoggedInUsers() > 1
              ? IDS_ASH_STATUS_TRAY_SIGN_OUT_ALL
              : IDS_ASH_STATUS_TRAY_SIGN_OUT;
      break;
  }
  std::u16string message =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
  // Desirable line breaking points are marked using \n. As the resource
  // framework does not evaluate escape sequences, the \n need to be explicitly
  // handled. Depending on the value of |multiline|, actual line breaks or
  // spaces are substituted.
  std::u16string newline = multiline ? u"\n" : u" ";
  base::ReplaceSubstringsAfterOffset(&message, 0, u"\\n", newline);
  return message;
}

}  // namespace user
}  // namespace ash
