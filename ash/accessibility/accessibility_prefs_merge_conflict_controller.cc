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

  auto* prefs = AccessibilityController::Get()->GetActiveUserPrefs();

  for (const auto& pref : GetAccessibilityPrefBatchesWithSyncEnabled()) {
    switch (pref.resolution_policy) {
      case ConflictResolutionPolicy::kDialogNeeded: {
        const base::Value* locked_value = locked_prefs.Find(pref.pref_name);
        const base::Value* pending_value = pending_prefs.Find(pref.pref_name);
        if (!locked_value && !pending_value) {
          continue;
        }
        if ((locked_value && pending_value) &&
            (*locked_value == *pending_value)) {
          continue;
        }
        // Normalize the conflict.
        conflicts.emplace_back(
            pref.pref_name,
            locked_value ? locked_value->Clone()
                         : prefs->GetDefaultPrefValue(pref.pref_name)->Clone(),
            pending_value
                ? pending_value->Clone()
                : prefs->GetDefaultPrefValue(pref.pref_name)->Clone());
        continue;
      }
      case ConflictResolutionPolicy::kLocalClobberRemote:
      case ConflictResolutionPolicy::kNone:
        continue;
    }
  }

  return conflicts;
}

}  // namespace

PrefConflict::PrefConflict(std::string pref_name,
                           base::Value local_value,
                           base::Value pending_value)
    : pref_name(std::move(pref_name)),
      local_value(std::move(local_value)),
      pending_value(std::move(pending_value)) {}

PrefConflict::PrefConflict(PrefConflict&&) = default;
PrefConflict& PrefConflict::operator=(PrefConflict&&) = default;

PrefConflict::~PrefConflict() = default;

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
    base::Value value) {
  auto* prefs = Shell::Get()->accessibility_controller()->GetActiveUserPrefs();
  for (auto& conflict : conflicts_) {
    if (conflict.pref_name == pref_name) {
      conflict.local_value = value.Clone();
      prefs->Set(pref_name, value);
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
    pref_service->Set(conflict.pref_name, conflict.local_value);
  }
}

}  // namespace ash
