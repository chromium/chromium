// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AUTOTEST_DESKS_API_H_
#define ASH_PUBLIC_CPP_AUTOTEST_DESKS_API_H_

#include "ash/ash_export.h"
#include "base/callback_forward.h"

namespace ash {

// Exposes limited APIs for the autotest private APIs to interact with Virtual
// Desks.
class ASH_EXPORT AutotestDesksApi {
 public:
  AutotestDesksApi();
  ~AutotestDesksApi();

  AutotestDesksApi(const AutotestDesksApi& other) = delete;
  AutotestDesksApi& operator=(const AutotestDesksApi& rhs) = delete;

  // Creates a new desk if the maximum number of desks has not been reached, and
  // returns true if succeeded, false otherwise.
  bool CreateNewDesk();

  // Activates the desk at the given |index| and invokes |on_complete| when the
  // switch-desks animation completes. Returns false if |index| is invalid, or
  // the desk at |index| is already the active desk; true otherwise.
  bool ActivateDeskAtIndex(int index, base::OnceClosure on_complete);

  // Removes the currently active desk and triggers the remove-desk animation.
  // |on_complete| will be invoked when the remove-desks animation completes.
  // Returns false if the active desk is the last available desk which cannot be
  // removed; true otherwise.
  bool RemoveActiveDesk(base::OnceClosure on_complete);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AUTOTEST_DESKS_API_H_
