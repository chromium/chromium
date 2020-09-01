// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/browser_util.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/channel.h"

using user_manager::User;
using version_info::Channel;

namespace crosapi {
namespace browser_util {
namespace {

// Some account types require features that aren't yet supported by lacros.
// See https://crbug.com/1080693
bool IsUserTypeAllowed(const User* user) {
  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
      return true;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_SUPERVISED:
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_CHILD:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
    case user_manager::NUM_USER_TYPES:
      return false;
  }
}

}  // namespace

const char kLaunchOnLoginPref[] = "lacros.launch_on_login";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kLaunchOnLoginPref, /*default_value=*/false);
}

base::FilePath GetUserDataDir() {
  base::FilePath base_path;
  if (base::SysInfo::IsRunningOnChromeOS()) {
    // NOTE: On device this function is privacy/security sensitive. The
    // directory must be inside the encrypted user partition.
    base_path = base::FilePath("/home/chronos/user");
  } else {
    // For developers on Linux desktop, put the directory under the developer's
    // specified --user-data-dir.
    base::PathService::Get(chrome::DIR_USER_DATA, &base_path);
  }
  return base_path.Append("lacros");
}

bool IsLacrosAllowed() {
  return IsLacrosAllowed(chrome::GetChannel());
}

bool IsLacrosAllowed(Channel channel) {
  const User* user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!user)
    return false;

  if (!IsUserTypeAllowed(user))
    return false;

  switch (channel) {
    case Channel::UNKNOWN:
      // Developer builds can use lacros.
      return true;
    case Channel::CANARY:
    case Channel::DEV:
    case Channel::BETA: {
      std::string canonical_email = user->GetAccountId().GetUserEmail();
      return base::EndsWith(canonical_email, "google.com",
                            base::CompareCase::INSENSITIVE_ASCII);
    }
    case Channel::STABLE:
      return false;
  }
}

}  // namespace browser_util
}  // namespace crosapi
