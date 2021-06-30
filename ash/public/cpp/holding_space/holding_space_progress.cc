// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_progress.h"

#include "base/check_op.h"

namespace ash {

HoldingSpaceProgress::HoldingSpaceProgress()
    : HoldingSpaceProgress(/*current_bytes=*/0,
                           /*total_bytes=*/0) {}

HoldingSpaceProgress::HoldingSpaceProgress(
    const absl::optional<int64_t>& current_bytes,
    const absl::optional<int64_t>& total_bytes)
    : current_bytes_(current_bytes), total_bytes_(total_bytes) {
  DCHECK_GE(current_bytes_.value_or(0), 0);
  DCHECK_GE(total_bytes_.value_or(0), 0);
  if (current_bytes_.has_value() && total_bytes_.has_value())
    DCHECK_LE(current_bytes_.value(), total_bytes_.value());
}

HoldingSpaceProgress::HoldingSpaceProgress(const HoldingSpaceProgress&) =
    default;

HoldingSpaceProgress& HoldingSpaceProgress::operator=(
    const HoldingSpaceProgress&) = default;

HoldingSpaceProgress::~HoldingSpaceProgress() = default;

bool HoldingSpaceProgress::operator==(const HoldingSpaceProgress& rhs) const {
  return std::tie(current_bytes_, total_bytes_) ==
         std::tie(rhs.current_bytes_, rhs.total_bytes_);
}

HoldingSpaceProgress& HoldingSpaceProgress::operator+=(
    const HoldingSpaceProgress& rhs) {
  HoldingSpaceProgress temp(*this);
  temp = temp + rhs;

  current_bytes_ = temp.current_bytes_;
  total_bytes_ = temp.total_bytes_;

  return *this;
}

HoldingSpaceProgress HoldingSpaceProgress::operator+(
    const HoldingSpaceProgress& rhs) const {
  absl::optional<int64_t> current_bytes(current_bytes_);
  absl::optional<int64_t> total_bytes(total_bytes_);

  // The number of `current_bytes` should only be present if present for both
  // the lhs and `rhs` instances. Otherwise `current_bytes` is indeterminate.
  if (current_bytes.has_value()) {
    current_bytes = rhs.current_bytes_.has_value()
                        ? absl::make_optional(current_bytes.value() +
                                              rhs.current_bytes_.value())
                        : absl::nullopt;
  }

  // The number of `total_bytes` should only be present if present for both the
  // lhs and `rhs` instances. Otherwise `total_bytes` is indeterminate.
  if (total_bytes.has_value()) {
    total_bytes = rhs.total_bytes_.has_value()
                      ? absl::make_optional(total_bytes.value() +
                                            rhs.total_bytes_.value())
                      : absl::nullopt;
  }

  return HoldingSpaceProgress(current_bytes, total_bytes);
}

absl::optional<float> HoldingSpaceProgress::GetValue() const {
  if (IsComplete())
    return 1.f;
  if (IsIndeterminate())
    return absl::nullopt;
  return static_cast<double>(current_bytes_.value()) /
         static_cast<double>(total_bytes_.value());
}

bool HoldingSpaceProgress::IsComplete() const {
  return !IsIndeterminate() && current_bytes_ == total_bytes_;
}

bool HoldingSpaceProgress::IsIndeterminate() const {
  return !current_bytes_.has_value() || !total_bytes_.has_value();
}

}  // namespace ash
