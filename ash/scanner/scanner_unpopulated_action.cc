// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_unpopulated_action.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "components/manta/proto/scanner.pb.h"

namespace ash {

ScannerUnpopulatedAction::ScannerUnpopulatedAction(
    manta::proto::ScannerAction unpopulated_action,
    PopulateCallback populate_to_proto_callback)
    : unpopulated_action_(std::move(unpopulated_action)),
      populate_callback_(std::move(populate_to_proto_callback)) {}

std::optional<ScannerUnpopulatedAction> ScannerUnpopulatedAction::FromProto(
    manta::proto::ScannerAction unpopulated_action,
    PopulateCallback populate_to_proto_callback) {
  if (unpopulated_action.action_case() ==
      manta::proto::ScannerAction::ACTION_NOT_SET) {
    return std::nullopt;
  }

  return ScannerUnpopulatedAction(std::move(unpopulated_action),
                                  std::move(populate_to_proto_callback));
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

void ScannerUnpopulatedAction::Populate(
    PopulatedActionCallback callback) const& {
  manta::proto::ScannerAction::ActionCase unpopulated_action_case =
      unpopulated_action_.action_case();
  // This should never occur unless the action has been previously moved.
  CHECK_NE(unpopulated_action_case,
           manta::proto::ScannerAction::ACTION_NOT_SET);

  // This causes a copy of the unpopulated action to be made.
  populate_callback_.Run(unpopulated_action_, std::move(callback));
}

}  // namespace ash
