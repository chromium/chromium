// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/crostini_ansible_playbook_external_data_handler.h"

#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

namespace policy {

CrostiniAnsiblePlaybookExternalDataHandler::
    CrostiniAnsiblePlaybookExternalDataHandler() = default;

CrostiniAnsiblePlaybookExternalDataHandler::
    ~CrostiniAnsiblePlaybookExternalDataHandler() = default;

void CrostiniAnsiblePlaybookExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByAccountId(
      CloudExternalDataPolicyObserver::GetAccountId(user_id));
  if (!profile) {
    LOG(ERROR) << "No profile for user is specified";
    return;
  }
  profile->GetPrefs()->ClearPref(
      crostini::prefs::kCrostiniAnsiblePlaybookFilePath);
  profile->GetPrefs()->ClearPref(
      crostini::prefs::kCrostiniDefaultContainerConfigured);
}

void CrostiniAnsiblePlaybookExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByAccountId(
      CloudExternalDataPolicyObserver::GetAccountId(user_id));
  if (!profile) {
    LOG(ERROR) << "No profile for user is specified";
    return;
  }
  profile->GetPrefs()->SetFilePath(
      crostini::prefs::kCrostiniAnsiblePlaybookFilePath, file_path);
  profile->GetPrefs()->SetBoolean(
      crostini::prefs::kCrostiniDefaultContainerConfigured, false);
}

void CrostiniAnsiblePlaybookExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id) {
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (!profile) {
    LOG(ERROR) << "No profile for user is specified";
    return;
  }
  profile->GetPrefs()->ClearPref(
      crostini::prefs::kCrostiniAnsiblePlaybookFilePath);
  profile->GetPrefs()->ClearPref(
      crostini::prefs::kCrostiniDefaultContainerConfigured);
}

}  // namespace policy
