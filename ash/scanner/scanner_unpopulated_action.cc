// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_unpopulated_action.h"

#include <optional>
#include <utility>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/manta/proto/scanner.pb.h"

namespace ash {

namespace {

// Converts a proto action - a oneof - into the equivalent variant type in
// Scanner code.
// `proto_action.action_case()` is guaranteed to never be `ACTION_NOT_SET`.
ScannerAction ProtoActionToVariant(manta::proto::ScannerAction proto_action) {
  switch (proto_action.action_case()) {
    case manta::proto::ScannerAction::kNewEvent:
      return std::move(*proto_action.mutable_new_event());

    case manta::proto::ScannerAction::kNewContact:
      return std::move(*proto_action.mutable_new_contact());

    case manta::proto::ScannerAction::kNewGoogleDoc:
      return std::move(*proto_action.mutable_new_google_doc());

    case manta::proto::ScannerAction::kNewGoogleSheet:
      return std::move(*proto_action.mutable_new_google_sheet());

    case manta::proto::ScannerAction::kCopyToClipboard:
      return std::move(*proto_action.mutable_copy_to_clipboard());

    case manta::proto::ScannerAction::ACTION_NOT_SET:
      NOTREACHED();
  }

  // This should never be reached, as `action_case()` should always return a
  // valid enum value. If the oneof field is set to something which is not
  // recognised by this client, that is indistinguishable from an unknown field,
  // and the above case should be `ACTION_NOT_SET`.
  NOTREACHED();
}

// Called when `PopulateToVariant`'s call to the provided
// `PopulateToProtoCallback` succeeds.
// `unpopulated_action_case` is guaranteed to never be `ACTION_NOT_SET`.
void OnPopulatedToProto(
    manta::proto::ScannerAction::ActionCase unpopulated_action_case,
    ScannerUnpopulatedAction::PopulateToVariantCallback callback,
    std::optional<manta::proto::ScannerAction> populated_action) {
  if (!populated_action.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  if (populated_action->action_case() != unpopulated_action_case) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(ProtoActionToVariant(std::move(*populated_action)));
}

}  // namespace

ScannerUnpopulatedAction::ScannerUnpopulatedAction(
    manta::proto::ScannerAction unpopulated_action,
    PopulateToProtoCallback populate_to_proto_callback)
    : unpopulated_action_(std::move(unpopulated_action)),
      populate_to_proto_callback_(std::move(populate_to_proto_callback)) {}

std::optional<ScannerUnpopulatedAction> ScannerUnpopulatedAction::FromProto(
    manta::proto::ScannerAction unpopulated_action,
    PopulateToProtoCallback populate_to_proto_callback) {
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

void ScannerUnpopulatedAction::PopulateToVariant(
    PopulateToVariantCallback callback) const& {
  manta::proto::ScannerAction::ActionCase unpopulated_action_case =
      unpopulated_action_.action_case();
  // This should never occur unless the action has been previously moved.
  CHECK_NE(unpopulated_action_case,
           manta::proto::ScannerAction::ACTION_NOT_SET);

  // This causes a copy of the unpopulated action to be made.
  populate_to_proto_callback_.Run(
      unpopulated_action_,
      base::BindOnce(&OnPopulatedToProto, unpopulated_action_case,
                     std::move(callback)));
}

}  // namespace ash
