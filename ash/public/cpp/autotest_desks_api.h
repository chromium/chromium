// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AUTOTEST_DESKS_API_H_
#define ASH_PUBLIC_CPP_AUTOTEST_DESKS_API_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "ui/aura/window.h"

namespace ash {

// Exposes limited APIs for the autotest private APIs to interact with Virtual
// Desks.
class ASH_EXPORT AutotestDesksApi {
 public:
  AutotestDesksApi();
  AutotestDesksApi(const AutotestDesksApi& other) = delete;
  AutotestDesksApi& operator=(const AutotestDesksApi& rhs) = delete;
  ~AutotestDesksApi();

  struct DesksInfo {
    DesksInfo();
    DesksInfo(const DesksInfo&);
    ~DesksInfo();

    // The zero-based index of the currently active desk.
    int active_desk_index;
    // Total number of desks.
    int num_desks;
    // True if desks are currently animating.
    bool is_animating;
    // Names of all current desk containers.
    std::vector<std::string> desk_containers;
  };

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

  // Activates the desk at the given |index| by activating all the desks between
  // the current desk and the desk at |index| in succession. This mimics
  // pressing the activate adjacent desk accelerator rapidly. |on_complete| will
  // be invoked when the the final animation to |index| completes. Returns false
  // if |index| is invalid, or the desk at |index| is already the active desk;
  // true otherwise.
  bool ActivateAdjacentDesksToTargetIndex(int index,
                                          base::OnceClosure on_complete);

  // Check whether a window belongs to a desk at |desk_index| or not.
  bool IsWindowInDesk(aura::Window* window, int desk_index);

  // Gets overall desks info, which includes the total number of desks, the
  // active desk index and whether a desk animation is happening.
  DesksInfo GetDesksInfo() const;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AUTOTEST_DESKS_API_H_
