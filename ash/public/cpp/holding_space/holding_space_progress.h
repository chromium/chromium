// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_H_

#include <optional>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// A class to represent progress in holding space. Progress can either be
// complete, determinate, or indeterminate.
class ASH_PUBLIC_EXPORT HoldingSpaceProgress {
 public:
  // Creates an instance which is complete.
  HoldingSpaceProgress();

  // Creates an instance for the specified `current_bytes` and `total_bytes`.
  HoldingSpaceProgress(const std::optional<int64_t>& current_bytes,
                       const std::optional<int64_t>& total_bytes);

  // Creates an instance for the specified `current_bytes` and `total_bytes`
  // which is explicitly `complete` or incomplete. If absent, completion will be
  // calculated based on `current_bytes` and `total_bytes`. If `true`, then it
  // must also be true that `current_bytes.value()` == `total_bytes.value()`.
  HoldingSpaceProgress(const std::optional<int64_t>& current_bytes,
                       const std::optional<int64_t>& total_bytes,
                       const std::optional<bool>& complete);

  // Creates an instance for the specified `current_bytes` and `total_bytes`
  // which is explicitly `complete` or incomplete. If absent, completion will be
  // calculated based on `current_bytes` and `total_bytes`. If `true`, then it
  // must also be true that `current_bytes.value()` == `total_bytes.value()`. If
  // `hidden` is `true`, this instance should not be painted nor included in
  // cumulative progress calculations.
  HoldingSpaceProgress(const std::optional<int64_t>& current_bytes,
                       const std::optional<int64_t>& total_bytes,
                       const std::optional<bool>& complete,
                       bool hidden);

  HoldingSpaceProgress(const HoldingSpaceProgress&);
  HoldingSpaceProgress& operator=(const HoldingSpaceProgress&);
  ~HoldingSpaceProgress();

  // Supported operations.
  bool operator==(const HoldingSpaceProgress& rhs) const;
  HoldingSpaceProgress& operator+=(const HoldingSpaceProgress& rhs);
  HoldingSpaceProgress operator+(const HoldingSpaceProgress& rhs) const;

  // Returns progress as an optional float value. If present, the returned
  // value is >= `0.f` and <= `1.f`. The value `1.f` indicates progress
  // completion while an absent value indicates indeterminate progress.
  std::optional<float> GetValue() const;

  // Returns `true` if progress is complete.
  bool IsComplete() const;

  // Returns `true` if progress is indeterminate.
  bool IsIndeterminate() const;

  // Returns `true` if progress is hidden and therefore should not be painted
  // nor included in cumulative progress calculations.
  bool IsHidden() const;

 private:
  std::optional<int64_t> current_bytes_;
  std::optional<int64_t> total_bytes_;
  bool complete_;
  bool hidden_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_PROGRESS_H_
