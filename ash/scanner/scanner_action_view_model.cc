// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_action_view_model.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/scanner/scanner_action_handler.h"
#include "ash/scanner/scanner_command_delegate.h"
#include "ash/scanner/scanner_unpopulated_action.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "components/manta/proto/scanner.pb.h"

namespace ash {

namespace {

// Executes the populated action, if it exists, calling
// `action_finished_callback` with the result of the execution.
void ExecutePopulatedAction(base::WeakPtr<ScannerCommandDelegate> delegate,
                            ScannerCommandCallback action_finished_callback,
                            std::optional<ScannerAction> populated_action) {
  if (!populated_action.has_value()) {
    std::move(action_finished_callback).Run(false);
    return;
  }

  HandleScannerCommand(std::move(delegate),
                       ScannerActionToCommand(std::move(*populated_action)),
                       std::move(action_finished_callback));
}

}  // namespace

ScannerActionViewModel::ScannerActionViewModel(
    ScannerUnpopulatedAction unpopulated_action,
    base::WeakPtr<ScannerCommandDelegate> delegate)
    : unpopulated_action_(std::move(unpopulated_action)),
      delegate_(std::move(delegate)) {}

ScannerActionViewModel::ScannerActionViewModel(const ScannerActionViewModel&) =
    default;

ScannerActionViewModel& ScannerActionViewModel::operator=(
    const ScannerActionViewModel&) = default;

ScannerActionViewModel::ScannerActionViewModel(ScannerActionViewModel&&) =
    default;

ScannerActionViewModel& ScannerActionViewModel::operator=(
    ScannerActionViewModel&&) = default;

ScannerActionViewModel::~ScannerActionViewModel() = default;

std::u16string ScannerActionViewModel::GetText() const {
  // TODO(b/369470078): Replace this with finalised translated strings.

  switch (unpopulated_action_.action_case()) {
    case manta::proto::ScannerAction::kNewEvent:
      return u"New event";
    case manta::proto::ScannerAction::kNewContact:
      return u"New contact";
    case manta::proto::ScannerAction::kNewGoogleDoc:
      return u"New Google Doc";
    case manta::proto::ScannerAction::kNewGoogleSheet:
      return u"New Google Sheet";
    case manta::proto::ScannerAction::kCopyToClipboard:
      return u"Copy to clipboard";
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      // This should only be possible if `unpopulated_action_` has been
      // previously moved.
      NOTREACHED();
  }

  // This should not be possible as all Protobuf variant case enums should
  // always be known.
  NOTREACHED();
}

const gfx::VectorIcon& ScannerActionViewModel::GetIcon() const {
  // TODO(b/369470078): Replace this placeholder.
  return kCaptureModeIcon;
}

void ScannerActionViewModel::ExecuteAction(
    ScannerCommandCallback action_finished_callback) const {
  unpopulated_action_.PopulateToVariant(base::BindOnce(
      &ExecutePopulatedAction, delegate_, std::move(action_finished_callback)));
}

}  // namespace ash
