// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FOCUS_ARROW_KEY_TRAVERSAL_CONTROLLER_H_
#define ASH_FOCUS_ARROW_KEY_TRAVERSAL_CONTROLLER_H_

#include "ash/ash_export.h"
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
  const bool was_enabled_;
};

using ScopedArrowKeyTraversalEnabler = ScopedArrowKeyTraversalController<true>;
using ScopedArrowKeyTraversalDisabler =
    ScopedArrowKeyTraversalController<false>;

// Stores whether or not the arrow key focus traversal is enabled.
class ArrowKeyTraversalController {
 public:
  static ArrowKeyTraversalController* Get();

  ArrowKeyTraversalController();
  ArrowKeyTraversalController(const ArrowKeyTraversalController&) = delete;
  ArrowKeyTraversalController& operator=(const ArrowKeyTraversalController&) =
      delete;
  ~ArrowKeyTraversalController();

  bool enabled() const { return enabled_; }

 private:
  friend class ScopedArrowKeyTraversalController<true>;
  friend class ScopedArrowKeyTraversalController<false>;
  bool enabled_ = false;
};

}  // namespace ash

#endif  // ASH_FOCUS_ARROW_KEY_TRAVERSAL_CONTROLLER_H_
