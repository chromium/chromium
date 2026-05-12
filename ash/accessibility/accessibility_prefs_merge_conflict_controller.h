// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_PREFS_MERGE_CONFLICT_CONTROLLER_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_PREFS_MERGE_CONFLICT_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/values.h"

namespace ash {

// Owns the business logic for resolving accessibility pref conflicts
// between local (OOBE/login screen) and account-stored values.
//
// If created, its ownership is transferred to the corresponding conflict
// resolution dialog.
//
// TODO(crbug.com/487229145): This class needs to listen and react to preference
// updates for the time the dialog is shown to the user.
class ASH_EXPORT AccessibilityPrefsMergeConflictController {
 public:
  struct PrefConflict {
    PrefConflict(std::string pref_name,
                 base::Value local_value,
                 base::Value pending_value);
    PrefConflict(const PrefConflict&) = delete;
    PrefConflict& operator=(const PrefConflict&) = delete;
    PrefConflict(PrefConflict&&);
    PrefConflict& operator=(PrefConflict&&);
    ~PrefConflict();

    std::string pref_name;
    base::Value local_value;
    base::Value pending_value;
  };

  // Returns a controller if accessibility prefs have sync/OOBE conflicts;
  // otherwise returns nullptr.
  static std::unique_ptr<AccessibilityPrefsMergeConflictController>
  MaybeCreate();

  virtual ~AccessibilityPrefsMergeConflictController();

  AccessibilityPrefsMergeConflictController(
      const AccessibilityPrefsMergeConflictController&) = delete;
  AccessibilityPrefsMergeConflictController& operator=(
      const AccessibilityPrefsMergeConflictController&) = delete;

  // Returns a vector of the preferences whose values conflict, as well as the
  // respective values.
  const std::vector<PrefConflict>& conflicts() const { return conflicts_; }

  // Updates and persists the preference value of the given |pref_name|.
  virtual void UpdateConflict(std::string_view pref_name, base::Value value);

  static std::unique_ptr<AccessibilityPrefsMergeConflictController>
  CreateForTest(std::vector<PrefConflict> conflicts);
  static std::vector<PrefConflict> BuildConflictsForTest(
      base::DictValue locked_prefs,
      base::DictValue pending_prefs);

 protected:
  explicit AccessibilityPrefsMergeConflictController(
      std::vector<PrefConflict> conflicts);

 private:
  std::vector<PrefConflict> conflicts_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_PREFS_MERGE_CONFLICT_CONTROLLER_H_
