// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/supervised/supervised_icon_string.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model//system_tray_model.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"

using base::UTF8ToUTF16;

namespace ash {

const gfx::VectorIcon& GetSupervisedUserIcon() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();

  if (session_controller->IsUserChild())
    return kSystemMenuSupervisedUserIcon;

  return kSystemMenuLegacySupervisedUserIcon;
}

std::u16string GetSupervisedUserMessage() {
  // When the fake model is in used, the model might mock that user is in a
  // child session when in fact it is not, so we just use a test message for
  // this.
  if (Shell::Get()->system_tray_model()->IsFakeModel()) {
    return u"Test supervised user message";
  }

  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  DCHECK(session_controller->IsUserChild());
  DCHECK(session_controller->IsActiveUserSessionStarted());

  // Get the active user session.
  const UserSession* const user_session = session_controller->GetUserSession(0);
  DCHECK(user_session);

  std::u16string first_custodian = UTF8ToUTF16(user_session->custodian_email);
  std::u16string second_custodian =
      UTF8ToUTF16(user_session->second_custodian_email);

  if (second_custodian.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_CHILD_USER_IS_MANAGED_BY_ONE_PARENT_NOTICE, first_custodian);
  }
  return l10n_util::GetStringFUTF16(
      IDS_ASH_CHILD_USER_IS_MANAGED_BY_TWO_PARENTS_NOTICE, first_custodian,
      second_custodian);
}

}  // namespace ash
