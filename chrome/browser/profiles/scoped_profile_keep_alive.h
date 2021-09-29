// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_SCOPED_PROFILE_KEEP_ALIVE_H_
#define CHROME_BROWSER_PROFILES_SCOPED_PROFILE_KEEP_ALIVE_H_

class Profile;
enum class ProfileKeepAliveOrigin;

class ScopedProfileKeepAlive {
 public:
  ScopedProfileKeepAlive(const Profile* profile, ProfileKeepAliveOrigin origin);
  ~ScopedProfileKeepAlive();

  ScopedProfileKeepAlive(const ScopedProfileKeepAlive&) = delete;
  ScopedProfileKeepAlive& operator=(const ScopedProfileKeepAlive&) = delete;

  const Profile* profile() { return profile_; }
  ProfileKeepAliveOrigin origin() { return origin_; }

 private:
  // Called after the ScopedProfileKeepAlive has been deleted, so this is a
  // static method where we pass parameters manually.
  static void RemoveKeepAliveOnUIThread(const Profile* profile,
                                        ProfileKeepAliveOrigin origin);

  const Profile* const profile_;
  const ProfileKeepAliveOrigin origin_;
};

#endif  // CHROME_BROWSER_PROFILES_SCOPED_PROFILE_KEEP_ALIVE_H_
