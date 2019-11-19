// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PROFILE_KEY_UTIL_H_
#define CHROME_BROWSER_ANDROID_PROFILE_KEY_UTIL_H_

class ProfileKey;

namespace android {

// This gets the profile key belonging to the last used profile on android, (or
// the "main" one in reduced mode). This works in both reduced mode and full
// browser mode.
//
// BE WARNED you should only use this if it would have been acceptable to use
// ProfileManager::GetLastUsedProfile() in the same context. If your usecase
// cares about different profiles and their keys, then you should plumb through
// the correct key instead.
ProfileKey* GetLastUsedProfileKey();

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_PROFILE_KEY_UTIL_H_
