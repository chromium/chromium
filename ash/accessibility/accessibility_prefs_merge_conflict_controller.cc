// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_prefs_merge_conflict_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_prefs_custom_associator.h"
#include "ash/accessibility/accessibility_sync_prefs_utils.h"
#include "ash/shell.h"
#include "base/memory/ptr_util.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

using PrefConflict = AccessibilityPrefsMergeConflictController::PrefConflict;

std::vector<PrefConflict> BuildConflicts(base::DictValue locked_prefs,
                                         base::DictValue pending_prefs) {
  std::vector<PrefConflict> conflicts;

  for (const auto& pref : GetAccessibilityPrefBatchesWithSyncEnabled()) {
    // TODO(crbug.com/485997707): Handle `kLocalClobberRemote`.
    if (pref.resolution_policy != ConflictResolutionPolicy::kDialogNeeded) {
      continue;
    }

    bool local = locked_prefs.FindBool(pref.pref_name).value_or(false);
    bool pending = pending_prefs.FindBool(pref.pref_name).value_or(false);

    if (local == pending) {
      continue;
    }

    conflicts.push_back({
        pref.pref_name,
        local,
        pending,
    });
  }

  return conflicts;
}

}  // namespace

// static
std::unique_ptr<AccessibilityPrefsMergeConflictController>
AccessibilityPrefsMergeConflictController::MaybeCreate() {
  // The singleton of the class accessibility preferences custom associator only
  // exists in case there are accessibility preferences that conflict between
  // the definitions in OOBE/login screen and Chrome Sync, particularly for
  // newly created users.
  auto* associator = AccessibilityController::Get()->prefs_custom_associator();
  if (!associator) {
    return nullptr;
  }

  auto [locked_prefs, pending_prefs] =
      associator->GetLockedAndAccountStorePrefs();

  // In case there is no preferences "locked" or "pending" to be set by the
  // sync, there is no conflict to be resolved.
  if (locked_prefs.empty() && pending_prefs.empty()) {
    return nullptr;
  }

  auto conflicts =
      BuildConflicts(std::move(locked_prefs), std::move(pending_prefs));
  if (conflicts.empty()) {
    return nullptr;
  }

  return base::WrapUnique(
      new AccessibilityPrefsMergeConflictController(std::move(conflicts)));
}

AccessibilityPrefsMergeConflictController::
    ~AccessibilityPrefsMergeConflictController() = default;

void AccessibilityPrefsMergeConflictController::UpdateConflict(
    std::string_view pref_name,
    bool value) {
  auto* prefs = Shell::Get()->accessibility_controller()->GetActiveUserPrefs();
  for (auto& conflict : conflicts_) {
    if (conflict.pref_name == pref_name) {
      conflict.local_value = value;
      prefs->SetBoolean(pref_name, value);
      return;
    }
  }
  // A preference had to be updated.
  NOTREACHED();
}

// static
std::unique_ptr<AccessibilityPrefsMergeConflictController>
AccessibilityPrefsMergeConflictController::CreateForTest(
    std::vector<PrefConflict> conflicts) {
  return base::WrapUnique(
      new AccessibilityPrefsMergeConflictController(std::move(conflicts)));
}

// static
std::vector<PrefConflict>
AccessibilityPrefsMergeConflictController::BuildConflictsForTest(
    base::DictValue locked_prefs,
    base::DictValue pending_prefs) {
  return BuildConflicts(std::move(locked_prefs), std::move(pending_prefs));
}

AccessibilityPrefsMergeConflictController::
    AccessibilityPrefsMergeConflictController(
        std::vector<PrefConflict> conflicts)
    : conflicts_(std::move(conflicts)) {
  auto* pref_service =
      Shell::Get()->accessibility_controller()->GetActiveUserPrefs();
  for (auto& conflict : conflicts_) {
    pref_service->SetBoolean(conflict.pref_name, conflict.local_value);
  }
}

}  // namespace ash
