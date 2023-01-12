// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_TYPES_ASH_H_
#define CHROME_BROWSER_PROFILES_PROFILE_TYPES_ASH_H_

// DEPRECATED: please use
// chromeos/ash/components/browser_context_helper/browser_context_types.h
// in the new code.
// TODO(crbug.com/1325210): Remove this file.

class Profile;

namespace base {
class FilePath;
}

bool IsUserProfile(const Profile* profile);
bool IsUserProfilePath(const base::FilePath& profile_path);

bool IsLockScreenProfile(const Profile* profile);
bool IsLockScreenAppProfile(const Profile* profile);
bool IsSigninProfile(const Profile* profile);

#endif  // CHROME_BROWSER_PROFILES_PROFILE_TYPES_ASH_H_
