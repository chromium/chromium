// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_metrics.h"

#include <string_view>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace {

using sessions::CommandStorageReadStatus;
using sessions::SessionReplayResult;

SessionRestoreErrorReason ReasonFromReadStatus(
    CommandStorageReadStatus status) {
  switch (status) {
    case CommandStorageReadStatus::kDecryptionUnavailable:
      return SessionRestoreErrorReason::kDecryptionUnavailable;
    case CommandStorageReadStatus::kInvalidHeader:
      return SessionRestoreErrorReason::kInvalidHeaderOrSignature;
    case CommandStorageReadStatus::kUnsupportedVersion:
      return SessionRestoreErrorReason::kUnsupportedFileVersion;
    case CommandStorageReadStatus::kFileInvalid:
    case CommandStorageReadStatus::kFileEmpty:
    case CommandStorageReadStatus::kInvalidCommand:
    case CommandStorageReadStatus::kUnknown:
      return SessionRestoreErrorReason::kCorruptedOrIncompleteFile;
    case CommandStorageReadStatus::kSuccess:
    case CommandStorageReadStatus::kNoFile:
      // Not errors; callers must not reach here.
      NOTREACHED();
  }
}

}  // namespace

SessionRestoreMetrics EvaluateSessionRestore(CommandStorageReadStatus status,
                                             SessionReplayResult replay_result,
                                             bool any_windows_left) {
  // A read-stage error is the root cause and outranks anything the replay
  // stage reports.
  if (sessions::IsCommandStorageReadError(status)) {
    return {SessionRestoreOutcome::kFailed, ReasonFromReadStatus(status)};
  }

  if (status == CommandStorageReadStatus::kNoFile) {
    return {SessionRestoreOutcome::kNoSessionFile, std::nullopt};
  }

  switch (replay_result) {
    case SessionReplayResult::kNoCommands:
    case SessionReplayResult::kAllCommandsClosed:
      // The session was read cleanly and legitimately held nothing to restore.
      return {SessionRestoreOutcome::kNothingToRestore, std::nullopt};
    case SessionReplayResult::kCorruptedCommand:
      // A failure even if windows were produced: the commands after the bad
      // one never ran, so tabs were silently lost.
      return {SessionRestoreOutcome::kFailed,
              SessionRestoreErrorReason::kCorruptedOrIncompleteCommand};
    case SessionReplayResult::kAllTabsPrunedNoNavigations:
      return {SessionRestoreOutcome::kFailed,
              SessionRestoreErrorReason::kAllTabsPrunedNoNavigations};
    case SessionReplayResult::kAllWindowsHadNoValidTabs:
      return {SessionRestoreOutcome::kFailed,
              SessionRestoreErrorReason::kAllWindowsHadNoValidTabs};
    case SessionReplayResult::kSuccess:
      if (any_windows_left) {
        return {SessionRestoreOutcome::kSuccess, std::nullopt};
      }
      // Replay produced windows, but RemoveUnusedRestoreWindows() dropped all
      // of them. Because SetWindowType() and ShouldTrackBrowser() only write
      // windows of each service's own type, this should be close to
      // unreachable; a non-zero count points to an unusual file (e.g., an
      // old-format file, a window type that changed across versions, or a
      // window whose type command was lost).
      return {SessionRestoreOutcome::kFailed,
              SessionRestoreErrorReason::kNoWindowsOfExpectedType};
  }
}

void RecordSessionRestoreMetrics(bool for_apps,
                                 CommandStorageReadStatus status,
                                 SessionReplayResult replay_result,
                                 bool any_windows_left) {
  const std::string_view type = for_apps ? "App" : "Normal";
  const SessionRestoreMetrics metrics =
      EvaluateSessionRestore(status, replay_result, any_windows_left);

  base::UmaHistogramEnumeration(base::StrCat({"SessionRestore.Outcome.", type}),
                                metrics.outcome);
  if (metrics.error_reason.has_value()) {
    CHECK_EQ(metrics.outcome, SessionRestoreOutcome::kFailed);
    base::UmaHistogramEnumeration(
        base::StrCat({"SessionRestore.ErrorReason.", type}),
        *metrics.error_reason);
  }
}
