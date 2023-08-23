// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_PREFS_H_
#define ASH_ACCELERATORS_ACCELERATOR_PREFS_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;

namespace ash {
// `AcceleratorPrefs` manages shortcut preference settings. It is used to
// register the prefs and observe the change in shortcut policy.
class ASH_EXPORT AcceleratorPrefs {
 public:
  AcceleratorPrefs();
  AcceleratorPrefs(const AcceleratorPrefs&) = delete;
  AcceleratorPrefs& operator=(const AcceleratorPrefs&) = delete;
  ~AcceleratorPrefs();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_PREFS_H_
