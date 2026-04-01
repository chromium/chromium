// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/sticky_position_constraint.h"

namespace cc {

StickyPositionConstraint::StickyPositionConstraint() = default;
StickyPositionConstraint::StickyPositionConstraint(
    const StickyPositionConstraint& other) = default;

StickyPositionConstraint& StickyPositionConstraint::operator=(
    const StickyPositionConstraint& other) = default;

bool StickyPositionConstraint::operator==(
    const StickyPositionConstraint& other) const = default;

}  // namespace cc
