// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus/arrow_key_traversal_controller.h"

#include "base/check.h"

namespace ash {

namespace {
ArrowKeyTraversalController* instance = nullptr;
}  // namespace

template <>
ScopedArrowKeyTraversalController<true>::ScopedArrowKeyTraversalController()
    : was_enabled_(ArrowKeyTraversalController::Get()->enabled()) {
  ArrowKeyTraversalController::Get()->enabled_ = true;
}

template <>
ScopedArrowKeyTraversalController<true>::~ScopedArrowKeyTraversalController() {
  ArrowKeyTraversalController::Get()->enabled_ = was_enabled_;
}

template <>
ScopedArrowKeyTraversalController<false>::ScopedArrowKeyTraversalController()
    : was_enabled_(ArrowKeyTraversalController::Get()->enabled()) {
  ArrowKeyTraversalController::Get()->enabled_ = false;
}

template <>
ScopedArrowKeyTraversalController<false>::~ScopedArrowKeyTraversalController() {
  ArrowKeyTraversalController::Get()->enabled_ = was_enabled_;
}

ArrowKeyTraversalController::ArrowKeyTraversalController() {
  CHECK(!instance);
  instance = this;
}

ArrowKeyTraversalController::~ArrowKeyTraversalController() {
  instance = nullptr;
}

ArrowKeyTraversalController* ArrowKeyTraversalController::Get() {
  return instance;
}

}  // namespace ash
