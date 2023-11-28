// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_progress.h"

#include <limits>

#include "base/check_op.h"

namespace ash {
namespace {

// Helpers ---------------------------------------------------------------------

// Returns whether or not the specified byte counts indicate completion.
bool CalculateComplete(const std::optional<int64_t>& current_bytes,
                       const std::optional<int64_t>& total_bytes) {
  return current_bytes.has_value() && current_bytes == total_bytes;
}

}  // namespace

// HoldingSpaceProgress --------------------------------------------------------

HoldingSpaceProgress::HoldingSpaceProgress()
    : HoldingSpaceProgress(/*current_bytes=*/0,
                           /*total_bytes=*/0) {}

HoldingSpaceProgress::HoldingSpaceProgress(
    const std::optional<int64_t>& current_bytes,
    const std::optional<int64_t>& total_bytes)
    : HoldingSpaceProgress(current_bytes,
                           total_bytes,
                           /*complete=*/std::nullopt) {}

HoldingSpaceProgress::HoldingSpaceProgress(
    const std::optional<int64_t>& current_bytes,
    const std::optional<int64_t>& total_bytes,
    const std::optional<bool>& complete)
    : HoldingSpaceProgress(current_bytes,
                           total_bytes,
                           complete,
                           /*hidden=*/false) {}

HoldingSpaceProgress::HoldingSpaceProgress(
    const std::optional<int64_t>& current_bytes,
    const std::optional<int64_t>& total_bytes,
    const std::optional<bool>& complete,
    bool hidden)
    : current_bytes_(current_bytes),
      total_bytes_(total_bytes),
      complete_(
          complete.value_or(CalculateComplete(current_bytes_, total_bytes_))),
      hidden_(hidden) {
  DCHECK_GE(current_bytes_.value_or(0), 0);
  DCHECK_GE(total_bytes_.value_or(0), 0);

  if (!current_bytes_.has_value() || !total_bytes_.has_value()) {
    DCHECK(!complete_);
    return;
  }

  if (complete_) {
    DCHECK_EQ(current_bytes_.value(), total_bytes_.value());
    return;
  }

  DCHECK_LE(current_bytes_.value(), total_bytes_.value());
}

HoldingSpaceProgress::HoldingSpaceProgress(const HoldingSpaceProgress&) =
    default;

HoldingSpaceProgress& HoldingSpaceProgress::operator=(
    const HoldingSpaceProgress&) = default;

HoldingSpaceProgress::~HoldingSpaceProgress() = default;

bool HoldingSpaceProgress::operator==(const HoldingSpaceProgress& rhs) const {
  return std::tie(current_bytes_, total_bytes_, complete_, hidden_) ==
         std::tie(rhs.current_bytes_, rhs.total_bytes_, rhs.complete_,
                  rhs.hidden_);
}

HoldingSpaceProgress& HoldingSpaceProgress::operator+=(
    const HoldingSpaceProgress& rhs) {
  // Hidden instances shouldn't be included in cumulative progress calculations.
  DCHECK(!hidden_);
  DCHECK(!rhs.hidden_);

  HoldingSpaceProgress temp(*this);
  temp = temp + rhs;

  current_bytes_ = temp.current_bytes_;
  total_bytes_ = temp.total_bytes_;
  complete_ = temp.complete_;

  return *this;
}

HoldingSpaceProgress HoldingSpaceProgress::operator+(
    const HoldingSpaceProgress& rhs) const {
  // Hidden instances shouldn't be included in cumulative progress calculations.
  DCHECK(!hidden_);
  DCHECK(!rhs.hidden_);

  // The number of `current_bytes` should only be present if present for both
  // the lhs and `rhs` instances. Otherwise `current_bytes` is indeterminate.
  std::optional<int64_t> current_bytes(current_bytes_);
  if (current_bytes.has_value()) {
    current_bytes = rhs.current_bytes_.has_value()
                        ? std::make_optional(current_bytes.value() +
                                             rhs.current_bytes_.value())
                        : std::nullopt;
  }

  // The number of `total_bytes` should only be present if present for both the
  // lhs and `rhs` instances. Otherwise `total_bytes` is indeterminate.
  std::optional<int64_t> total_bytes(total_bytes_);
  if (total_bytes.has_value()) {
    total_bytes =
        rhs.total_bytes_.has_value()
            ? std::make_optional(total_bytes.value() + rhs.total_bytes_.value())
            : std::nullopt;
  }

  // The result of summing lhs and `rhs` instances is `complete` if and only if
  // both the lhs and `rhs` are themselves complete.
  const bool complete = complete_ && rhs.complete_;

  return HoldingSpaceProgress(current_bytes, total_bytes, complete);
}

std::optional<float> HoldingSpaceProgress::GetValue() const {
  if (IsComplete())
    return 1.f;

  if (IsIndeterminate())
    return std::nullopt;

  // If `current_bytes_` == `total_bytes_` but progress is not complete,
  // return a value that is extremely close but not equal to `1.f`.
  if (current_bytes_.value() == total_bytes_.value())
    return 1.f - std::numeric_limits<float>::epsilon();

  return static_cast<double>(current_bytes_.value()) /
         static_cast<double>(total_bytes_.value());
}

bool HoldingSpaceProgress::IsComplete() const {
  return complete_;
}

bool HoldingSpaceProgress::IsIndeterminate() const {
  return !current_bytes_.has_value() || !total_bytes_.has_value();
}

bool HoldingSpaceProgress::IsHidden() const {
  return hidden_;
}

}  // namespace ash
