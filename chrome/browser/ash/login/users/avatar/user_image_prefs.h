// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_PREFS_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace ash::user_image::prefs {

// Key for user pref to allow customization of user avatar image using Google
// profile image or custom local images.
extern const char kUserAvatarCustomizationSelectorsEnabled[];

// Register profile prefs with pref registry.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns boolean value of the kUserAvatarCustomizationSelectorsEnabled pref.
bool IsCustomizationSelectorsPrefEnabled(const PrefService* pref_service);

}  // namespace ash::user_image::prefs

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_PREFS_H_
