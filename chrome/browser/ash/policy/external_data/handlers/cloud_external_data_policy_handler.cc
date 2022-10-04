// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/cloud_external_data_policy_handler.h"

#include "chrome/browser/browser_process.h"
#include "components/user_manager/known_user.h"

namespace policy {

CloudExternalDataPolicyHandler::CloudExternalDataPolicyHandler() = default;

// static
AccountId CloudExternalDataPolicyHandler::GetAccountId(
    const std::string& user_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  return known_user.GetAccountId(user_id, std::string() /* id */,
                                 AccountType::UNKNOWN);
}

}  // namespace policy
