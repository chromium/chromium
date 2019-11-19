// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/external_data_handlers/cloud_external_data_policy_handler.h"

#include "components/user_manager/known_user.h"

namespace policy {

CloudExternalDataPolicyHandler::CloudExternalDataPolicyHandler() = default;

// static
AccountId CloudExternalDataPolicyHandler::GetAccountId(
    const std::string& user_id) {
  return user_manager::known_user::GetAccountId(user_id, std::string() /* id */,
                                                AccountType::UNKNOWN);
}

}  // namespace policy
