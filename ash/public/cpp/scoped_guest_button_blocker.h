// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCOPED_GUEST_BUTTON_BLOCKER_H_
#define ASH_PUBLIC_CPP_SCOPED_GUEST_BUTTON_BLOCKER_H_

namespace ash {

// Class that temporarily disables the Browse as Guest login button on shelf.
class ScopedGuestButtonBlocker {
 public:
  ScopedGuestButtonBlocker(const ScopedGuestButtonBlocker&) = delete;
  ScopedGuestButtonBlocker& operator=(const ScopedGuestButtonBlocker&) = delete;

  virtual ~ScopedGuestButtonBlocker() = default;

 protected:
  ScopedGuestButtonBlocker() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCOPED_GUEST_BUTTON_BLOCKER_H_
