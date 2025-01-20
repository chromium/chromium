// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_unpopulated_action.h"

#include <optional>
#include <utility>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "components/manta/proto/scanner.pb.h"

namespace ash {

ScannerUnpopulatedAction::ScannerUnpopulatedAction(
    manta::proto::ScannerAction unpopulated_action,
    scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes)
    : unpopulated_action_(std::move(unpopulated_action)),
      downscaled_jpeg_bytes_(std::move(downscaled_jpeg_bytes)) {}

std::optional<ScannerUnpopulatedAction> ScannerUnpopulatedAction::FromProto(
    manta::proto::ScannerAction unpopulated_action,
    scoped_refptr<base::RefCountedMemory> downscaled_jpeg_bytes) {
  if (unpopulated_action.action_case() ==
      manta::proto::ScannerAction::ACTION_NOT_SET) {
    return std::nullopt;
  }

  return ScannerUnpopulatedAction(std::move(unpopulated_action),
                                  std::move(downscaled_jpeg_bytes));
}

ScannerUnpopulatedAction::ScannerUnpopulatedAction(
    const ScannerUnpopulatedAction&) = default;
ScannerUnpopulatedAction& ScannerUnpopulatedAction::operator=(
    const ScannerUnpopulatedAction&) = default;
ScannerUnpopulatedAction::ScannerUnpopulatedAction(ScannerUnpopulatedAction&&) =
    default;
ScannerUnpopulatedAction& ScannerUnpopulatedAction::operator=(
    ScannerUnpopulatedAction&&) = default;

ScannerUnpopulatedAction::~ScannerUnpopulatedAction() = default;

}  // namespace ash
