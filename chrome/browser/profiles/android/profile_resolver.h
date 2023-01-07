// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_ANDROID_PROFILE_RESOLVER_H_
#define CHROME_BROWSER_PROFILES_ANDROID_PROFILE_RESOLVER_H_

#include <string>

#include "base/functional/callback_forward.h"

class Profile;
class ProfileKey;

namespace profile_resolver {

using ProfileCallback = base::OnceCallback<void(Profile*)>;
using ProfileKeyCallback = base::OnceCallback<void(ProfileKey*)>;

// These functions provides a profile and C++ specific implementation of the
// Java BrowserContextHandleResolver interface. Generates a string uniquely
// identifying a Profile instance that can be later used to retrieve this
// instance. Works both for regular and off-the-record profiles.
void ResolveProfile(std::string token, ProfileCallback callback);
void ResolveProfileKey(std::string token, ProfileKeyCallback callback);
std::string TokenizeProfile(Profile* profile);
std::string TokenizeProfileKey(ProfileKey* profile_key);

}  // namespace profile_resolver

#endif  // CHROME_BROWSER_PROFILES_ANDROID_PROFILE_RESOLVER_H_
