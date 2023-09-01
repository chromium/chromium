// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_PREFS_DELEGATE_H_
#define ASH_ACCELERATORS_ACCELERATOR_PREFS_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {

// AcceleratorPrefsDelegate serves as an abstract interface that contains
// function to determine whether user is managed. The AcceleratorPrefs can not
// do this directly because ProfileManager and UserManager are in //chrome.
class ASH_EXPORT AcceleratorPrefsDelegate {
 public:
  virtual ~AcceleratorPrefsDelegate() = default;

  // Return true if the user is managed user.
  virtual bool IsUserEnterpriseManaged() const = 0;
};
}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_PREFS_DELEGATE_H_
