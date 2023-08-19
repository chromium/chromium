// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/profile_util.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions::profile_util {

bool ProfileCanUseNonComponentExtensions(const Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!profile || !ash::ProfileHelper::IsUserProfile(profile)) {
    return false;
  }

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user) {
    return false;
  }

  // ChromeOS has special irregular profiles that must also be filtered
  // out in addition to `ProfileHelper::IsUserProfile()`. `IsUserProfile()`
  // includes guest and public users (which cannot use non-component
  // extensions) so instead only look for those user types that can use them.
  switch (user->GetType()) {
    case user_manager::USER_TYPE_REGULAR:
    case user_manager::USER_TYPE_CHILD:
      return true;

    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
    case user_manager::NUM_USER_TYPES:
      return false;
  }
}
#else
  if (!profile) {
    return false;
  }
  return profile->IsRegularProfile();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions::profile_util
