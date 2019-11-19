// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/external_data_handlers/crostini_ansible_playbook_external_data_handler.h"

#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_service.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

namespace policy {

CrostiniAnsiblePlaybookExternalDataHandler::
    CrostiniAnsiblePlaybookExternalDataHandler(
        chromeos::CrosSettings* cros_settings,
        DeviceLocalAccountPolicyService* policy_service)
    : crostini_ansible_observer_(cros_settings,
                                 policy_service,
                                 key::kCrostiniAnsiblePlaybook,
                                 this) {
  crostini_ansible_observer_.Init();
}

CrostiniAnsiblePlaybookExternalDataHandler::
    ~CrostiniAnsiblePlaybookExternalDataHandler() = default;

void CrostiniAnsiblePlaybookExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByAccountId(
      GetAccountId(user_id));
  if (!profile) {
    LOG(ERROR) << "No profile for user is specified";
    return;
  }
  profile->GetPrefs()->ClearPref(
      crostini::prefs::kCrostiniAnsiblePlaybookFilePath);
}

void CrostiniAnsiblePlaybookExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByAccountId(
      GetAccountId(user_id));
  if (!profile) {
    LOG(ERROR) << "No profile for user is specified";
    return;
  }
  profile->GetPrefs()->SetFilePath(
      crostini::prefs::kCrostiniAnsiblePlaybookFilePath, file_path);
}

void CrostiniAnsiblePlaybookExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id) {
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (!profile) {
    LOG(ERROR) << "No profile for user is specified";
    return;
  }
  profile->GetPrefs()->ClearPref(
      crostini::prefs::kCrostiniAnsiblePlaybookFilePath);
}

}  // namespace policy
