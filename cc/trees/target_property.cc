// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/target_property.h"

#include <stdint.h>

#include "ui/gfx/animation/keyframe/target_property.h"

namespace cc {

static_assert(TargetProperty::LAST_TARGET_PROPERTY <
                  gfx::kMaxTargetPropertyIndex,
              "The number of cc target properties has exceeded the capacity of"
              " TargetProperties");

// bitset will use a multiple of the architecture int size, which is at least 32
// bits so make it explicit to have as many properties as fit into the memory
// used.
static_assert(gfx::kMaxTargetPropertyIndex % (8 * sizeof(uint32_t)) == 0,
              "The maximum number of target properties should be a multiple of "
              "sizeof(uint32_t)");

}  // namespace cc
