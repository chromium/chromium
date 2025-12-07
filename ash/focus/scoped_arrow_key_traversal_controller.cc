// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus/scoped_arrow_key_traversal_controller.h"

#include "base/check.h"

namespace ash {

namespace {
bool arrow_key_traversal_enabled = false;
}  // namespace

template <>
ScopedArrowKeyTraversalController<true>::ScopedArrowKeyTraversalController()
    : auto_reset_(&arrow_key_traversal_enabled, true) {}

template <>
ScopedArrowKeyTraversalController<true>::~ScopedArrowKeyTraversalController() {
  CHECK(arrow_key_traversal_enabled);
}

template <>
ScopedArrowKeyTraversalController<false>::ScopedArrowKeyTraversalController()
    : auto_reset_(&arrow_key_traversal_enabled, false) {}

template <>
ScopedArrowKeyTraversalController<false>::~ScopedArrowKeyTraversalController() {
  CHECK(!arrow_key_traversal_enabled);
}

bool IsArrowKeyTraversalEnabled() {
  return arrow_key_traversal_enabled;
}

}  // namespace ash
