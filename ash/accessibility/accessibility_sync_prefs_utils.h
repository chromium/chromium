// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_SYNC_PREFS_UTILS_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_SYNC_PREFS_UTILS_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/values.h"

namespace ash {

// Three batches of accessibility preferences are in the process of becoming
// sync-enabled by default, but are currently gated behind feature flags.
// This file contains some utils to help with the process of enabling them.

// When a feature preference set during OOBE differs from the value stored in
// Chrome Sync, one of the following conflict resolution strategies is applied:
//
// - kNone: Default behavior. The value from Chrome Sync overrides the local
// value.
// - kLocalClobberRemote: The value set during OOBE is preserved and overwrites
//   the value from Chrome Sync.
// - kDialogNeeded: The user is prompted with a dialog to choose which value to
// keep.
enum class ConflictResolutionPolicy {
  kNone,
  kLocalClobberRemote,
  kDialogNeeded,
};

// Describes an accessibility preference that can be registered as part of a
// feature-controlled batch.
//
// Each descriptor provides the preference name, its default value, and the
// conflict resolution policy to use if the preference becomes syncable.
// The actual sync registration behavior is determined externally (e.g. via
// feature flags).
//
// If `has_custom_registration` is true, the preference is registered
// elsewhere and should be skipped by generic batch registration helpers.
struct ASH_EXPORT AccessibilityPrefBatchEntry {
  const char* pref_name;
  base::Value default_value;
  ConflictResolutionPolicy resolution_policy;
  const uint32_t registration_flags;
  bool has_custom_registration = false;
};

// Returns the syncable accessibility prefs for batch 1.
ASH_EXPORT std::vector<AccessibilityPrefBatchEntry>
GetSyncableAccessibilityPrefsBatch1();

// Returns the syncable accessibility prefs for batch 2.
ASH_EXPORT std::vector<AccessibilityPrefBatchEntry>
GetSyncableAccessibilityPrefsBatch2();

// Returns the syncable accessibility prefs for batch 3.
ASH_EXPORT std::vector<AccessibilityPrefBatchEntry>
GetSyncableAccessibilityPrefsBatch3();

// Returns the syncable accessibility prefs for the enabled batches.
ASH_EXPORT std::vector<AccessibilityPrefBatchEntry>
GetAccessibilityPrefBatchesWithSyncEnabled();

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_SYNC_PREFS_UTILS_H_
