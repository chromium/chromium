// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_PREFS_CUSTOM_ASSOCIATOR_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_PREFS_CUSTOM_ASSOCIATOR_H_

#include <string>

#include "ash/ash_export.h"
#include "base/values.h"

namespace ash {

struct AccessibilityPrefBatchEntry;

// Coordinates locking and custom merge behavior for some syncable accessibility
// preferences.
//
// This associator controls merge outcomes for a subset of accessibility prefs
// that are syncable and registered with
// sync_preferences::MergeBehavior::kCustom.
//
// The object has a short life cycle and is owned by AccessibilityController.
//
// When a pref is locked, merges prefer the local value. When it is not locked,
// merges behave like sync_preferences::MergeBehavior::kNone (server value
// wins).
// Locking is only used for prefs that can be toggled at the OOBE/login
// screen before full profile sync is established.
class ASH_EXPORT AccessibilityPrefsCustomAssociator {
 public:
  AccessibilityPrefsCustomAssociator();
  ~AccessibilityPrefsCustomAssociator();

  AccessibilityPrefsCustomAssociator(
      const AccessibilityPrefsCustomAssociator&) = delete;
  AccessibilityPrefsCustomAssociator& operator=(
      const AccessibilityPrefsCustomAssociator&) = delete;

  // Can return null.
  static AccessibilityPrefsCustomAssociator* Get();

  // Determines the preferred merged value for a lockable accessibility pref.
  //
  // This method is consulted only for prefs whose merge behavior is controlled
  // by this associator. For such prefs:
  //
  // - If the pref is locked to a specific value, that locked value is returned.
  // - If the pref is lockable but not currently locked, the `server_value` is
  //   preferred and returned.
  //
  // Returns std::nullopt if this associator does not control the merge behavior
  // for `pref_name`.
  std::optional<base::Value> GetPreferredPrefMergeValue(
      std::string_view pref_name,
      const base::Value& server_value) const;

  // Locks `pref_name` to `value` if eligible.
  //
  // While locked, this value takes precedence over the server value during
  // merges.
  void TryLockPref(std::string_view pref_name, const base::Value& value);

  // Unlocks `pref_name`.
  //
  // When unlocked, the server value takes precedence.
  void UnlockPref(std::string_view pref_name);

  // Clears all locking state maintained by this associator.
  void UnlockAllPrefs();

  // Stores the remote (account-store) value for `pref_name`, if eligible.
  //
  // This value represents the remote state of the preference and is used by
  // conflict resolution heuristics during merge. Prefs that are not lockable
  // (as defined by the syncable accessibility pref set) are ignored.
  //
  // Returns true if the value was accepted; false otherwise.
  bool TrySetAccountStorePref(std::string_view pref_name,
                              const base::Value& value);

  // Snapshot of the internal pref state maintained by this associator.
  struct LockedAndAccountStorePrefs {
    base::DictValue locked_prefs;
    base::DictValue account_store_prefs;
  };

  // Returns the currently tracked locked prefs and account-store prefs.
  LockedAndAccountStorePrefs GetLockedAndAccountStorePrefs() const;

  // Prints to stderr the content of the preference/value pairs set by calling
  // TryLockPref() and TrySetAccountStorePref().
  void ToString() const;

 private:
  // Returns whether a given preference `pref_name` can be locked, i.e. it is
  // a syncable accessibility pref that requires conflict resolution.
  bool CanLockPref(std::string_view pref_name) const;

  // Returns whether `pref_name` is currently locked.
  bool IsPrefLocked(std::string_view pref_name) const;

  // Returns the locked value for `pref_name`, or base::Value() if not locked.
  base::Value GetPrefLockedValue(std::string_view pref_name) const;

  // List of locked prefs that must use the stored local value instead of
  // server value.
  base::DictValue locked_prefs_;

  // Stored account-store (remote sync) values for lockable prefs.
  // Used by conflict resolution heuristics during merge.
  base::DictValue account_store_prefs_;

  // Array of accessibility prefs that are syncable as per the feature flags.
  const std::vector<AccessibilityPrefBatchEntry> enabled_sync_prefs_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_PREFS_CUSTOM_ASSOCIATOR_H_
