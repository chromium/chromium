// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drivefs_test_support.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"

namespace drive {

const char FakeDriveFsHelper::kPredefinedProfileSalt[] = "salt";

FakeDriveFsHelper::FakeDriveFsHelper(Profile* profile,
                                     const base::FilePath& mount_path)
    : mount_path_(mount_path), fake_drivefs_(mount_path_) {
  profile->GetPrefs()->SetString(
      drive::prefs::kDriveFsProfileSalt,
      drive::FakeDriveFsHelper::kPredefinedProfileSalt);
  fake_drivefs_.RegisterMountingForAccountId(
      base::BindLambdaForTesting([profile]() {
        auto* user = chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
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
  auto known_users_list = std::make_unique<base::ListValue>();
  auto user_dict = std::make_unique<base::DictionaryValue>();
  user_dict->SetString("account_type", "google");
  user_dict->SetString("email", "testuser@gmail.com");
  user_dict->SetString("gaia_id", "123456");
  known_users_list->Append(std::move(user_dict));

  base::DictionaryValue local_state;
  local_state.SetList("KnownUsers", std::move(known_users_list));

  std::string local_state_json;
  if (!base::JSONWriter::Write(local_state, &local_state_json))
    return false;

  base::FilePath local_state_file;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &local_state_file))
    return false;
  local_state_file = local_state_file.Append(chrome::kLocalStateFilename);
  return base::WriteFile(local_state_file, local_state_json.data(),
                         local_state_json.size()) != -1;
}

}  // namespace drive
