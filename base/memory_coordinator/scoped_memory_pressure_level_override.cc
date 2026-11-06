// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/scoped_memory_pressure_level_override.h"

namespace base {

ScopedMemoryPressureLevelOverride::ScopedMemoryPressureLevelOverride() =
    default;

ScopedMemoryPressureLevelOverride::~ScopedMemoryPressureLevelOverride() {
  ClearLevel();
}

void ScopedMemoryPressureLevelOverride::SetLevel(
    MemoryPressureLevel memory_pressure_level) {
  if (!suppression_token_) {
    suppression_token_.emplace();
  }
  MemoryPressureSource::SimulatePressureNotification(memory_pressure_level);
}

void ScopedMemoryPressureLevelOverride::ClearLevel() {
  suppression_token_.reset();
}

}  // namespace base
