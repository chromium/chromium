// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_SCOPED_MEMORY_PRESSURE_LEVEL_OVERRIDE_H_
#define BASE_MEMORY_COORDINATOR_SCOPED_MEMORY_PRESSURE_LEVEL_OVERRIDE_H_

#include <optional>

#include "base/memory/memory_pressure_level.h"
#include "base/memory_coordinator/memory_pressure_source.h"

namespace base {

// A scoped object to override the memory pressure level of the process's
// MemoryPressureSource for tests. Use this when the test doesn't own the
// MemoryPressureSource (e.g. browser tests). Tests that own the source can call
// MemoryPressureSource::SetMemoryPressureLevel() directly.
//
// While a level is set, the levels reported by the producers of the memory
// pressure signal are recorded but not notified. When no suppression tokens
// exist anymore, subscribers are notified of the real level.
//
// Must be used on the main thread.
//
// Note: This class does not support nested overrides.
class ScopedMemoryPressureLevelOverride {
 public:
  ScopedMemoryPressureLevelOverride();
  ~ScopedMemoryPressureLevelOverride();

  ScopedMemoryPressureLevelOverride(const ScopedMemoryPressureLevelOverride&) =
      delete;
  ScopedMemoryPressureLevelOverride& operator=(
      const ScopedMemoryPressureLevelOverride&) = delete;

  // Sets the overridden level, and notifies subscribers if it changed.
  void SetLevel(MemoryPressureLevel memory_pressure_level);

  // Clears the override. Called automatically on destruction.
  void ClearLevel();

 private:
  // Suppresses the notifications of the real level while a level is set.
  std::optional<MemoryPressureSuppressionToken> suppression_token_;
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_SCOPED_MEMORY_PRESSURE_LEVEL_OVERRIDE_H_
