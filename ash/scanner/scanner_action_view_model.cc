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
#include "ash/scanner/scanner_metrics.h"
#include "ash/scanner/scanner_unpopulated_action.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/manta/proto/scanner.pb.h"

namespace ash {

namespace {

using enum ScannerFeatureUserState;

void RecordExecutePopulatedActionTimer(
    manta::proto::ScannerAction::ActionCase action_case,
    base::TimeTicks execute_start_time) {
  // TODO(b/363101363): Add tests once scanner action view model tests are set
  // up.
  std::string_view variant_name;
  switch (action_case) {
    case manta::proto::ScannerAction::kNewEvent:
      variant_name = kScannerFeatureTimerExecutePopulatedNewCalendarEventAction;
      break;
    case manta::proto::ScannerAction::kNewContact:
      variant_name = kScannerFeatureTimerExecutePopulatedNewContactAction;
      break;
    case manta::proto::ScannerAction::kNewGoogleDoc:
      variant_name = kScannerFeatureTimerExecutePopulatedNewGoogleDocAction;
      break;
    case manta::proto::ScannerAction::kNewGoogleSheet:
      variant_name = kScannerFeatureTimerExecutePopulatedNewGoogleSheetAction;
      break;
    case manta::proto::ScannerAction::kCopyToClipboard:
      variant_name =
          kScannerFeatureTimerExecutePopulatedNewCopyToClipboardAction;
      break;
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      break;
  }
  if (variant_name.empty()) {
    return;
  }
  base::UmaHistogramMediumTimes(variant_name,
                                base::TimeTicks::Now() - execute_start_time);
}

void RecordPopulateActionTimer(
    manta::proto::ScannerAction::ActionCase action_case,
    base::TimeTicks request_start_time) {
  // TODO(b/363101363): Add tests once scanner action view model tests are set
  // up.
  std::string_view variant_name;
  switch (action_case) {
    case manta::proto::ScannerAction::kNewEvent:
      variant_name = kScannerFeatureTimerPopulateNewCalendarEventAction;
      break;
    case manta::proto::ScannerAction::kNewContact:
      variant_name = kScannerFeatureTimerPopulateNewContactAction;
      break;
    case manta::proto::ScannerAction::kNewGoogleDoc:
      variant_name = kScannerFeatureTimerPopulateNewGoogleDocAction;
      break;
    case manta::proto::ScannerAction::kNewGoogleSheet:
      variant_name = kScannerFeatureTimerPopulateNewGoogleSheetAction;
      break;
    case manta::proto::ScannerAction::kCopyToClipboard:
      variant_name = kScannerFeatureTimerPopulateNewCopyToClipboardAction;
      break;
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      break;
  }
  if (variant_name.empty()) {
    return;
  }
  base::UmaHistogramMediumTimes(variant_name,
                                base::TimeTicks::Now() - request_start_time);
}

void RecordPopulateActionFailure(
    manta::proto::ScannerAction::ActionCase action_case) {
  // TODO(b/363101363): Add tests once scanner action view model tests are set
  // up.
  switch (action_case) {
    case manta::proto::ScannerAction::kNewEvent:
      RecordScannerFeatureUserState(kNewCalendarEventActionPopulationFailed);
      return;
    case manta::proto::ScannerAction::kNewContact:
      RecordScannerFeatureUserState(kNewContactActionPopulationFailed);
      return;
    case manta::proto::ScannerAction::kNewGoogleDoc:
      RecordScannerFeatureUserState(kNewGoogleDocActionPopulationFailed);
      return;
    case manta::proto::ScannerAction::kNewGoogleSheet:
      RecordScannerFeatureUserState(kNewGoogleSheetActionPopulationFailed);
      return;
    case manta::proto::ScannerAction::kCopyToClipboard:
      RecordScannerFeatureUserState(kCopyToClipboardActionPopulationFailed);
      return;
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      return;
  }
}

void RecordActionExecutionAndRun(
    manta::proto::ScannerAction::ActionCase action_case,
    base::TimeTicks execute_start_time,
    ScannerCommandCallback action_finished_callback,
    bool success) {
  // TODO(b/363101363): Add tests once scanner action view model tests are set
  // up.
  switch (action_case) {
    case manta::proto::ScannerAction::kNewEvent:
      RecordScannerFeatureUserState(
          success ? kNewCalendarEventActionFinishedSuccessfully
                  : kNewCalendarEventPopulatedActionExecutionFailed);
      break;
    case manta::proto::ScannerAction::kNewContact:
      RecordScannerFeatureUserState(
          success ? kNewContactActionFinishedSuccessfully
                  : kNewContactPopulatedActionExecutionFailed);
      break;
    case manta::proto::ScannerAction::kNewGoogleDoc:
      RecordScannerFeatureUserState(
          success ? kNewGoogleDocActionFinishedSuccessfully
                  : kNewGoogleDocPopulatedActionExecutionFailed);
      break;
    case manta::proto::ScannerAction::kNewGoogleSheet:
      RecordScannerFeatureUserState(
          success ? kNewGoogleSheetActionFinishedSuccessfully
                  : kNewGoogleSheetPopulatedActionExecutionFailed);
      break;
    case manta::proto::ScannerAction::kCopyToClipboard:
      RecordScannerFeatureUserState(
          success ? kCopyToClipboardActionFinishedSuccessfully
                  : kCopyToClipboardPopulatedActionExecutionFailed);
      break;
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      break;
  }
  RecordExecutePopulatedActionTimer(action_case, execute_start_time);
  std::move(action_finished_callback).Run(success);
}

// Executes the populated action, if it exists, calling
// `action_finished_callback` with the result of the execution.
void ExecutePopulatedAction(manta::proto::ScannerAction::ActionCase action_case,
                            base::TimeTicks request_start_time,
                            base::WeakPtr<ScannerCommandDelegate> delegate,
                            ScannerCommandCallback action_finished_callback,
                            std::optional<ScannerAction> populated_action) {
  RecordPopulateActionTimer(action_case, request_start_time);
  if (!populated_action.has_value()) {
    RecordPopulateActionFailure(action_case);
    std::move(action_finished_callback).Run(false);
    return;
  }

  ScannerCommandCallback record_metrics_callback = base::BindOnce(
      &RecordActionExecutionAndRun, action_case, base::TimeTicks::Now(),
      std::move(action_finished_callback));

  HandleScannerCommand(std::move(delegate),
                       ScannerActionToCommand(std::move(*populated_action)),
                       std::move(record_metrics_callback));
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
  // TODO(b/375967525): Replace this with finalised translated strings.

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
  // TODO(b/378002546): Replace these icons with finalized icons when ready.
  switch (unpopulated_action_.action_case()) {
    case manta::proto::ScannerAction::kNewEvent:
      return kGlanceablesCalendarTodayIcon;
    case manta::proto::ScannerAction::kNewContact:
      return kShelfAddPersonButtonIcon;
    case manta::proto::ScannerAction::kNewGoogleDoc:
      return chromeos::kFiletypeGdocIcon;
    case manta::proto::ScannerAction::kNewGoogleSheet:
      return chromeos::kFiletypeGsheetIcon;
    case manta::proto::ScannerAction::kCopyToClipboard:
      return kClipboardIcon;
    case manta::proto::ScannerAction::ACTION_NOT_SET:
      // This should only be possible if `unpopulated_action_` has been
      // previously moved.
      NOTREACHED();
  }
}

void ScannerActionViewModel::ExecuteAction(
    ScannerCommandCallback action_finished_callback) const {
  unpopulated_action_.PopulateToVariant(base::BindOnce(
      &ExecutePopulatedAction, unpopulated_action_.action_case(),
      base::TimeTicks::Now(), delegate_, std::move(action_finished_callback)));
}

}  // namespace ash
