// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_KEEP_ALIVE_PROFILE_KEEP_ALIVE_WAITER_H_
#define CHROME_BROWSER_PROFILES_KEEP_ALIVE_PROFILE_KEEP_ALIVE_WAITER_H_

#include "base/run_loop.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile_manager_observer.h"

// Waiter for when a keep alive from a specific origin is added to a specific
// profile. Used for testing only.
class ProfileKeepAliveAddedWaiter : public ProfileManagerObserver {
 public:
  explicit ProfileKeepAliveAddedWaiter(Profile* observed_profile,
                                       ProfileKeepAliveOrigin observed_origin);
  ProfileKeepAliveAddedWaiter() = delete;
  ProfileKeepAliveAddedWaiter(const ProfileKeepAliveAddedWaiter&) = delete;
  ProfileKeepAliveAddedWaiter& operator=(const ProfileKeepAliveAddedWaiter&) =
      delete;

  ~ProfileKeepAliveAddedWaiter() override;

  void Wait();

  // ProfileManagerObserver:
  void OnKeepAliveAdded(const Profile* profile,
                        ProfileKeepAliveOrigin origin) override;

 private:
  raw_ptr<Profile> observed_profile_;
  ProfileKeepAliveOrigin observed_origin_;
  base::RunLoop run_loop_;
};

#endif  // CHROME_BROWSER_PROFILES_KEEP_ALIVE_PROFILE_KEEP_ALIVE_WAITER_H_
