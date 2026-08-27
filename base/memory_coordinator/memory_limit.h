// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MEMORY_LIMIT_H_
#define BASE_MEMORY_COORDINATOR_MEMORY_LIMIT_H_

#include <type_traits>

#include "base/base_export.h"
#include "base/check.h"
#include "base/numerics/safe_conversions.h"

namespace base {

class ByteSize;

// Represents a memory usage limit for a MemoryConsumer.
//
// A memory limit is expressed as a percentage relative to a consumer's baseline
// (100% / normal capacity). It can scale above 100% for high-end devices, or
// down to 0% for critical memory pressure.
class BASE_EXPORT MemoryLimit {
 public:
  // The default memory limit (100%), representing baseline capacity.
  static constexpr MemoryLimit Default() {
    return MemoryLimit(kDefaultPercent);
  }

  constexpr MemoryLimit() = default;

  // Threshold helpers corresponding to legacy MemoryPressureListener levels
  // (MEMORY_PRESSURE_LEVEL_NONE, MEMORY_PRESSURE_LEVEL_MODERATE, and
  // MEMORY_PRESSURE_LEVEL_CRITICAL). These facilitate the migration of clients
  // from MemoryPressureListener to the MemoryConsumer API by providing standard
  // discrete threshold targets.
  static constexpr MemoryLimit NoPressureThreshold() {
    return MemoryLimit(kDefaultPercent);
  }
  static constexpr MemoryLimit ModeratePressureThreshold() {
    return MemoryLimit(kModeratePressurePercent);
  }
  static constexpr MemoryLimit CriticalPressureThreshold() {
    return MemoryLimit(kCriticalPressurePercent);
  }

  // Named constructor for constructing arbitrary percentage limits.
  static constexpr MemoryLimit FromPercent(int percent) {
    return MemoryLimit(percent);
  }

  // Implicit constructor and conversion operator to facilitate phased
  // migration from raw int across Chromium.
  // TODO(crbug.com/441951621): Make constructor private and remove conversion
  // operator after call site migration is complete.
  constexpr MemoryLimit(int percent) : percent_(percent) {
    // Memory limits cannot be negative. Uses CHECK instead of CHECK_GE to avoid
    // including base/check_op.h in this header for this single use case.
    CHECK(percent >= 0);
  }
  constexpr operator int() const { return percent_; }

  // Disallow floating point conversions to prevent silent truncation of ratios
  // (e.g., passing 0.5 becoming 0% / critical pressure).
  // TODO(crbug.com/441951621): Remove once the implicit int constructor is
  // made explicit/private.
  MemoryLimit(float) = delete;
  MemoryLimit(double) = delete;

  constexpr int percent() const { return percent_; }
  constexpr double ratio() const { return percent_ / 100.0; }

  // Scales a baseline value linearly by this memory limit.
  // The result is truncated towards zero and clamped to the range of T.
  template <typename T>
  constexpr T Scale(T baseline) const {
    static_assert(std::is_integral_v<T>, "T must be an integral type.");
    return saturated_cast<T>(baseline * ratio());
  }

  ByteSize Scale(ByteSize baseline) const;

  constexpr bool operator==(const MemoryLimit&) const = default;
  constexpr auto operator<=>(const MemoryLimit&) const = default;

  // Heterogeneous comparisons with int to facilitate phased migration from raw
  // int without operator ambiguity.
  // TODO(crbug.com/441951621): Remove when migration to MemoryLimit is
  // complete.
  constexpr bool operator==(int other) const { return percent_ == other; }
  constexpr auto operator<=>(int other) const { return percent_ <=> other; }

 private:
  static constexpr int kDefaultPercent = 100;
  static constexpr int kModeratePressurePercent = 50;
  static constexpr int kCriticalPressurePercent = 0;

  int percent_ = kDefaultPercent;
};

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_MEMORY_LIMIT_H_
