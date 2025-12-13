// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/time_remaining_calculator.h"

#include "base/check_op.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "ui/base/l10n/time_format.h"

namespace save_to_drive {
namespace {

using extensions::api::pdf_viewer_private::SaveToDriveStatus;

}  // namespace

TimeRemainingCalculator::~TimeRemainingCalculator() = default;

std::optional<base::TimeDelta> TimeRemainingCalculator::GetRemainingTime(
    const base::ByteCount& uploaded_bytes,
    const base::ByteCount& file_size_bytes) const {
  const base::ByteCount remaining_bytes = file_size_bytes - uploaded_bytes;
  if (remaining_bytes.is_zero() || remaining_bytes.is_negative()) {
    return std::nullopt;
  }
  const int upload_speed = upload_speed_estimator_.GetCountPerSecond();
  if (upload_speed <= 0) {
    return std::nullopt;
  }
  // No rounding is done here since the output is just an estimation.
  return base::Seconds(remaining_bytes.InBytes() / upload_speed);
}

std::optional<std::u16string>
TimeRemainingCalculator::CalculateTimeRemainingText(
    const extensions::api::pdf_viewer_private::SaveToDriveProgress& progress) {
  CHECK(progress.status == SaveToDriveStatus::kUploadStarted ||
        progress.status == SaveToDriveStatus::kUploadInProgress);
  std::optional<base::TimeDelta> remaining_time;
  base::ByteCount uploaded_bytes(progress.uploaded_bytes.value());
  if (progress.status == SaveToDriveStatus::kUploadInProgress) {
    const base::ByteCount bytes_delta = uploaded_bytes - last_uploaded_bytes_;
    upload_speed_estimator_.Increment(bytes_delta.InBytes());
    remaining_time = GetRemainingTime(
        uploaded_bytes, base::ByteCount(progress.file_size_bytes.value()));
  }
  last_uploaded_bytes_ = std::move(uploaded_bytes);
  if (!remaining_time.has_value()) {
    return std::nullopt;
  }
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                                ui::TimeFormat::LENGTH_LONG,
                                remaining_time.value());
}

}  // namespace save_to_drive
