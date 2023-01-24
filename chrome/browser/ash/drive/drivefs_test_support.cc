// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/drivefs_test_support.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/hash/md5.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "components/account_id/account_id.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace drive {

const char FakeDriveFsHelper::kPredefinedProfileSalt[] = "salt";
const char FakeDriveFsHelper::kDefaultUserEmail[] = "testuser@gmail.com";
const char FakeDriveFsHelper::kDefaultGaiaId[] = "123456";

FakeDriveFsHelper::FakeDriveFsHelper(Profile* profile,
                                     const base::FilePath& mount_path)
    : mount_path_(mount_path), fake_drivefs_(mount_path_) {
  profile->GetPrefs()->SetString(
      drive::prefs::kDriveFsProfileSalt,
      drive::FakeDriveFsHelper::kPredefinedProfileSalt);
  fake_drivefs_.RegisterMountingForAccountId(
      base::BindLambdaForTesting([profile]() {
        auto* user = ash::ProfileHelper::Get()->GetUserByProfile(profile);
        if (!user)
          return std::string();

        return base::MD5String(FakeDriveFsHelper::kPredefinedProfileSalt +
                               ("-" + user->GetAccountId().GetAccountIdKey()));
      }));
}
FakeDriveFsHelper::~FakeDriveFsHelper() = default;

base::RepeatingCallback<std::unique_ptr<drivefs::DriveFsBootstrapListener>()>
FakeDriveFsHelper::CreateFakeDriveFsListenerFactory() {
  return base::BindRepeating(&drivefs::FakeDriveFs::CreateMojoListener,
                             base::Unretained(&fake_drivefs_));
}

bool SetUpUserDataDirectoryForDriveFsTest() {
  AccountId account_id = AccountId::FromUserEmailGaiaId(
      FakeDriveFsHelper::kDefaultUserEmail, FakeDriveFsHelper::kDefaultGaiaId);
  return SetUpUserDataDirectoryForDriveFsTest(account_id);
}

bool SetUpUserDataDirectoryForDriveFsTest(const AccountId& account_id) {
  // Account type must be GOOGLE to use Drive.
  CHECK(account_id.GetAccountType() == AccountType::GOOGLE);
  base::Value::List known_users_list;
  base::Value::Dict user_dict;
  user_dict.Set("account_type",
                AccountId::AccountTypeToString(account_id.GetAccountType()));
  user_dict.Set("email", account_id.GetUserEmail());
  user_dict.Set("gaia_id", account_id.GetGaiaId());
  known_users_list.Append(std::move(user_dict));

  base::Value::Dict local_state;
  local_state.Set("KnownUsers", std::move(known_users_list));

  std::string local_state_json;
  if (!base::JSONWriter::Write(local_state, &local_state_json))
    return false;

  base::FilePath local_state_file;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &local_state_file))
    return false;
  local_state_file = local_state_file.Append(chrome::kLocalStateFilename);
  return base::WriteFile(local_state_file, local_state_json);
}

}  // namespace drive
