// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_KEEP_ALIVE_SCOPED_PROFILE_KEEP_ALIVE_H_
#define CHROME_BROWSER_PROFILES_KEEP_ALIVE_SCOPED_PROFILE_KEEP_ALIVE_H_

#include "base/memory/raw_ptr.h"

class Profile;
enum class ProfileKeepAliveOrigin;

// Prevents a Profile from getting destroyed before the browser shuts down, e.g.
// because there's a UI window or job in progress that needs the Profile. Use
// these objects with a unique_ptr for easy management.
//
// This does not affect BrowserProcess lifetime, so the browser can still shut
// down if the user asks for it (which will cause Profile deletion). If you
// don't want the browser to shut down, you should use a ScopedKeepAlive as
// well.
//
// This is a no-op when the DestroyProfileOnBrowserClose flag is disabled.
//
// This is only meant for non-OTR Profile. Trying to acquire a keepalive on an
// off-the-record Profile triggers a DCHECK.
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

  const raw_ptr<const Profile, DanglingUntriaged> profile_;
  const ProfileKeepAliveOrigin origin_;
};

#endif  // CHROME_BROWSER_PROFILES_KEEP_ALIVE_SCOPED_PROFILE_KEEP_ALIVE_H_
