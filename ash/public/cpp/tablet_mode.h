// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TABLET_MODE_H_
#define ASH_PUBLIC_CPP_TABLET_MODE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/macros.h"

namespace ash {

// A utility to allow code in //ash/public/cpp to access the tablet mode state,
// regardless of what process it's running in.
class TabletMode {
 public:
  using TabletModeCallback = base::RepeatingCallback<bool(void)>;

  // Sets the callback to be run by IsEnabled().
  static void ASH_PUBLIC_EXPORT SetCallback(TabletModeCallback callback);

  // Returns whether the system is in tablet mode.
  static bool IsEnabled();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TabletMode);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TABLET_MODE_H_
