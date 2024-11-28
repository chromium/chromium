// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FOCUS_SCOPED_ARROW_KEY_TRAVERSAL_CONTROLLER_H_
#define ASH_FOCUS_SCOPED_ARROW_KEY_TRAVERSAL_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/auto_reset.h"
#include "ui/views/focus/focus_manager_factory.h"

namespace ash {

template <bool>
class ASH_EXPORT ScopedArrowKeyTraversalController {
 public:
  ScopedArrowKeyTraversalController();
  ScopedArrowKeyTraversalController(const ScopedArrowKeyTraversalController&) =
      delete;
  ScopedArrowKeyTraversalController& operator=(
      const ScopedArrowKeyTraversalController&) = delete;
  ~ScopedArrowKeyTraversalController();

 private:
  base::AutoReset<bool> auto_reset_;
};

using ScopedArrowKeyTraversalEnabler = ScopedArrowKeyTraversalController<true>;
using ScopedArrowKeyTraversalDisabler =
    ScopedArrowKeyTraversalController<false>;

// Returns true if the arrow key focus traversal is enabled.
bool IsArrowKeyTraversalEnabled();

}  // namespace ash

#endif  // ASH_FOCUS_SCOPED_ARROW_KEY_TRAVERSAL_CONTROLLER_H_
