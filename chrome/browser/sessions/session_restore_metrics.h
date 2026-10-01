// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_METRICS_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_METRICS_H_

#include <optional>

#include "components/sessions/core/command_storage_read_status.h"
#include "components/sessions/core/session_service_commands.h"

// Records SessionRestore.Outcome.{Type} and SessionRestore.ErrorReason.{Type},
// which answer two questions about each restore subsystem: did the restore
// complete successfully, and if not, why?
//
// The storage and replay layers in //components/sessions report facts
// (sessions::CommandStorageReadStatus and sessions::SessionReplayResult);
// deciding which combinations of those facts amount to a failed restore is
// policy, and lives here.

// Whether a session restore succeeded, and if not, the broad category of
// failure. A restore succeeds when the session file was read, all of its
// commands were replayed without error, and at least one restorable window
// remained after pruning empty tabs/windows and filtering by window type.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SessionRestoreOutcome)
enum class SessionRestoreOutcome {
  // Session file read and all commands replayed without error, leaving at least
  // one restorable window.
  kSuccess = 0,
  // No session file existed for this subsystem.
  kNoSessionFile = 1,
  // Clean read and replay, but the session held no restorable windows (every
  // window was closed before exit). No user state was lost.
  kNothingToRestore = 2,
  // The session file could not be read, its commands could not be fully
  // replayed, or every window it described was dropped. See
  // SessionRestore.ErrorReason.{Type} for the cause.
  kFailed = 3,
  kMaxValue = kFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:SessionRestoreOutcome)

// Why a session restore failed. Only recorded alongside
// SessionRestoreOutcome::kFailed.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SessionRestoreErrorReason)
enum class SessionRestoreErrorReason {
  // OSCrypt key unavailable or keychain locked.
  kDecryptionUnavailable = 0,
  kInvalidHeaderOrSignature = 1,
  kUnsupportedFileVersion = 2,
  // Empty, truncated or otherwise unreadable file.
  kCorruptedOrIncompleteFile = 3,
  // Replay aborted part way through. Also covers command ids this version does
  // not recognize, which can happen after a downgrade rather than because of
  // genuine corruption.
  kCorruptedOrIncompleteCommand = 4,
  // Every unclosed tab had no navigations, so AddTabsToWindows() dropped them
  // all and left no windows with tabs.
  kAllTabsPrunedNoNavigations = 5,
  // Unclosed windows were present, but none ended up with a valid tab (e.g.,
  // all tabs were closed, or tabs with navigations referenced a closed window
  // ID), so every window was dropped.
  kAllWindowsHadNoValidTabs = 6,
  // The session described windows, but none of the type this subsystem
  // restores, so RemoveUnusedRestoreWindows() dropped them all. Because each
  // service only writes windows of its own type, this is unexpected in normal
  // operation and points to an old-format file, a window type that changed
  // across versions, or a missing window-type command.
  kNoWindowsOfExpectedType = 7,
  kMaxValue = kNoWindowsOfExpectedType,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:SessionRestoreErrorReason)

struct SessionRestoreMetrics {
  SessionRestoreOutcome outcome;
  // Set if and only if `outcome` is kFailed.
  std::optional<SessionRestoreErrorReason> error_reason;
};

// Classifies one subsystem's restore. `any_windows_left` is whether that
// subsystem still had windows once RemoveUnusedRestoreWindows() had run.
//
// When a read-stage error and a replay-stage failure occur together the
// read-stage error wins, because it is the root cause. Both shapes of read
// error leave the replay result describing a consequence rather than an
// independent problem:
//   - A file that could not be decrypted or whose header was bad yields no
//     commands at all, so the replay trivially reports kNoCommands.
//   - A mid-file error such as truncation yields the commands read before the
//     error plus a failing status, and that partial stream is still replayed.
//     Replaying it tends to fail on its own terms (windows whose navigation
//     commands never arrived, for instance), but the truncated read is what
//     caused it.
SessionRestoreMetrics EvaluateSessionRestore(
    sessions::CommandStorageReadStatus status,
    sessions::SessionReplayResult replay_result,
    bool any_windows_left);

// Records the metrics for one restore subsystem. `for_apps` selects between the
// App (AppSessionService) and Normal (SessionService) variants.
void RecordSessionRestoreMetrics(bool for_apps,
                                 sessions::CommandStorageReadStatus status,
                                 sessions::SessionReplayResult replay_result,
                                 bool any_windows_left);

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_METRICS_H_
