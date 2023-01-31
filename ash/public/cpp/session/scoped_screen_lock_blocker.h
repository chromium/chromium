// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SESSION_SCOPED_SCREEN_LOCK_BLOCKER_H_
#define ASH_PUBLIC_CPP_SESSION_SCOPED_SCREEN_LOCK_BLOCKER_H_

namespace ash {

// Scoped object to temporarily disable the screen lock.
class ScopedScreenLockBlocker {
 public:
  virtual ~ScopedScreenLockBlocker() = default;

  ScopedScreenLockBlocker(const ScopedScreenLockBlocker&) = delete;
  ScopedScreenLockBlocker& operator=(const ScopedScreenLockBlocker&) = delete;

 protected:
  ScopedScreenLockBlocker() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SESSION_SCOPED_SCREEN_LOCK_BLOCKER_H_
