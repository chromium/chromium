// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCOPED_GUEST_BUTTON_BLOCKER_H_
#define ASH_PUBLIC_CPP_SCOPED_GUEST_BUTTON_BLOCKER_H_

#include "base/macros.h"

namespace ash {

// Class that temporarily disables the Browse as Guest login button on shelf.
class ScopedGuestButtonBlocker {
 public:
  virtual ~ScopedGuestButtonBlocker() = default;

 protected:
  ScopedGuestButtonBlocker() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedGuestButtonBlocker);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCOPED_GUEST_BUTTON_BLOCKER_H_
