// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_prefs.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace ash {

const char kShelfAutoHideBehaviorAlways[] = "Always";
const char kShelfAutoHideBehaviorNever[] = "Never";

const char kDeskButtonInShelfShown[] = "Shown";
const char kDeskButtonInShelfHidden[] = "Hidden";

// If any of the following ShelfAlignment values changed, the ShelfAlignment
// policy should be updated.
const char kShelfAlignmentBottom[] = "Bottom";
const char kShelfAlignmentLeft[] = "Left";
const char kShelfAlignmentRight[] = "Right";

namespace {

// Returns the preference value for the display with the given |display_id|.
// The pref value is stored in |local_path| and |path|, but the pref service may
// have per-display preferences and the value can be specified by policy.
// Here is the priority:
//  * A value managed by policy. This is a single value that applies to all
//    displays.
//  * A user-set value for the specified display.
//  * A user-set value in |local_path| or |path|, if no per-display settings are
//    ever specified (see http://crbug.com/173719 for why), |local_path| is
//    preferred. See comment in |kShelfAlignment| as to why we consider two
//    prefs and why |local_path| is preferred.
//  * A value recommended by policy. This is a single value that applies to all
//    root windows.
//  * The default value for |local_path| if the value is not recommended by
//    policy.
std::string GetPerDisplayPref(PrefService* prefs,
                              int64_t display_id,
                              const char* local_path,
                              const char* path) {
  const PrefService::Preference* local_pref = prefs->FindPreference(local_path);
  const std::string value(prefs->GetString(local_path));
  if (local_pref->IsManaged())
    return value;

  std::string pref_key = base::NumberToString(display_id);
  bool has_per_display_prefs = false;
  if (!pref_key.empty()) {
    const base::Value::Dict& shelf_prefs =
        prefs->GetDict(prefs::kShelfPreferences);
    const base::Value::Dict* display_pref = shelf_prefs.FindDict(pref_key);
    if (display_pref) {
      const std::string* per_display_value =
          display_pref->FindStringByDottedPath(path);
      if (per_display_value)
        return *per_display_value;
    }

    // If the pref for the specified display is not found, scan the whole prefs
    // and check if the prefs for other display is already specified.
    std::string unused_value;
    for (const auto iter : shelf_prefs) {
      if (iter.second.is_dict() &&
          iter.second.GetDict().FindStringByDottedPath(path)) {
        has_per_display_prefs = true;
        break;
      }
    }
  }

  if (local_pref->IsRecommended() || !has_per_display_prefs)
    return value;

  const std::string* default_string =
      prefs->GetDefaultPrefValue(local_path)->GetIfString();
  return default_string ? *default_string : std::string();
}

ShelfAlignment AlignmentFromPref(const std::string& value) {
  if (value == kShelfAlignmentLeft)
    return ShelfAlignment::kLeft;
  if (value == kShelfAlignmentRight)
    return ShelfAlignment::kRight;
  // Default to bottom.
  return ShelfAlignment::kBottom;
}

const char* AlignmentToPref(ShelfAlignment alignment) {
  switch (alignment) {
    case ShelfAlignment::kBottom:
      return kShelfAlignmentBottom;
    case ShelfAlignment::kLeft:
      return kShelfAlignmentLeft;
    case ShelfAlignment::kRight:
      return kShelfAlignmentRight;
    case ShelfAlignment::kBottomLocked:
      // This should not be a valid preference option for now. We only want to
      // lock the shelf during login or when adding a user.
      return nullptr;
  }
  NOTREACHED();
}

ShelfAutoHideBehavior AutoHideBehaviorFromPref(const std::string& value) {
  // Note: To maintain sync compatibility with old images of chrome/chromeos
  // the set of values that may be encountered includes the now-extinct
  // "Default" as well as "Never" and "Always", "Default" should now
  // be treated as "Never" (http://crbug.com/146773).
  if (value == kShelfAutoHideBehaviorAlways)
    return ShelfAutoHideBehavior::kAlways;
  return ShelfAutoHideBehavior::kNever;
}

const char* AutoHideBehaviorToPref(ShelfAutoHideBehavior behavior) {
  switch (behavior) {
    case ShelfAutoHideBehavior::kAlways:
      return kShelfAutoHideBehaviorAlways;
    case ShelfAutoHideBehavior::kNever:
      return kShelfAutoHideBehaviorNever;
    case ShelfAutoHideBehavior::kAlwaysHidden:
      // This should not be a valid preference option for now. We only want to
      // completely hide it when we run in app mode - or while we temporarily
      // hide the shelf (e.g. SessionAbortedDialog).
      return nullptr;
  }
  NOTREACHED();
}

}  // namespace

void SetPerDisplayShelfPref(PrefService* prefs,
                            int64_t display_id,
                            const char* pref_key,
                            const std::string& value) {
  if (display_id == display::kInvalidDisplayId)
    return;

  // Avoid ScopedDictPrefUpdate's notifications for read but unmodified prefs.
  const base::Value::Dict& current_shelf_prefs =
      prefs->GetDict(prefs::kShelfPreferences);
  std::string display_key = base::NumberToString(display_id);
  const base::Value::Dict* current_display_prefs =
      current_shelf_prefs.FindDict(display_key);
  if (current_display_prefs) {
    const std::string* current_value =
        current_display_prefs->FindStringByDottedPath(pref_key);
    if (current_value && *current_value == value)
      return;
  }

  ScopedDictPrefUpdate update(prefs, prefs::kShelfPreferences);
  base::Value::Dict& shelf_prefs = update.Get();
  base::Value::Dict* display_prefs_weak = shelf_prefs.EnsureDict(display_key);
  display_prefs_weak->Set(pref_key, value);
}

ShelfAutoHideBehavior GetShelfAutoHideBehaviorPref(PrefService* prefs,
                                                   int64_t display_id) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);

  if (!base::FeatureList::IsEnabled(features::kShelfAutoHideSeparation)) {
    // See comment in |kShelfAlignment| as to why we consider two prefs.
    return AutoHideBehaviorFromPref(
        GetPerDisplayPref(prefs, display_id, prefs::kShelfAutoHideBehaviorLocal,
                          prefs::kShelfAutoHideBehavior));
  }

  const bool is_in_tablet_mode = display::Screen::GetScreen()->InTabletMode();
  // See comment in |kShelfAlignment| as to why we consider two prefs.
  return AutoHideBehaviorFromPref(GetPerDisplayPref(
      prefs, display_id,
      is_in_tablet_mode ? prefs::kShelfAutoHideTabletModeBehaviorLocal
                        : prefs::kShelfAutoHideBehaviorLocal,
      is_in_tablet_mode ? prefs::kShelfAutoHideTabletModeBehavior
                        : prefs::kShelfAutoHideBehavior));
}

void SetShelfAutoHideBehaviorPref(PrefService* prefs,
                                  int64_t display_id,
                                  ShelfAutoHideBehavior behavior) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);

  const char* value = AutoHideBehaviorToPref(behavior);
  if (!value)
    return;

  if (!base::FeatureList::IsEnabled(features::kShelfAutoHideSeparation)) {
    SetPerDisplayShelfPref(prefs, display_id, prefs::kShelfAutoHideBehavior,
                           value);
    if (display_id == display::Screen::GetScreen()->GetPrimaryDisplay().id()) {
      // See comment in |kShelfAlignment| about why we have two prefs here.
      prefs->SetString(prefs::kShelfAutoHideBehaviorLocal, value);
      prefs->SetString(prefs::kShelfAutoHideBehavior, value);
    }
    return;
  }

  const bool is_in_tablet_mode = display::Screen::GetScreen()->InTabletMode();
  SetPerDisplayShelfPref(prefs, display_id,
                         is_in_tablet_mode
                             ? prefs::kShelfAutoHideTabletModeBehavior
                             : prefs::kShelfAutoHideBehavior,
                         value);
  if (display_id == display::Screen::GetScreen()->GetPrimaryDisplay().id()) {
    // See comment in |kShelfAlignment| about why we have two prefs here.
    prefs->SetString(is_in_tablet_mode
                         ? prefs::kShelfAutoHideTabletModeBehaviorLocal
                         : prefs::kShelfAutoHideBehaviorLocal,
                     value);
    prefs->SetString(is_in_tablet_mode ? prefs::kShelfAutoHideTabletModeBehavior
                                       : prefs::kShelfAutoHideBehavior,
                     value);
  }
}

ShelfAlignment GetShelfAlignmentPref(PrefService* prefs, int64_t display_id) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);

  // See comment in |kShelfAlignment| as to why we consider two prefs.
  return AlignmentFromPref(GetPerDisplayPref(
      prefs, display_id, prefs::kShelfAlignmentLocal, prefs::kShelfAlignment));
}

void SetShelfAlignmentPref(PrefService* prefs,
                           int64_t display_id,
                           ShelfAlignment alignment) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);

  const char* value = AlignmentToPref(alignment);
  if (!value)
    return;

  SetPerDisplayShelfPref(prefs, display_id, prefs::kShelfAlignment, value);
  if (display_id == display::Screen::GetScreen()->GetPrimaryDisplay().id()) {
    // See comment in |kShelfAlignment| as to why we consider two prefs.
    prefs->SetString(prefs::kShelfAlignmentLocal, value);
    prefs->SetString(prefs::kShelfAlignment, value);
  }
}

bool GetDeskButtonVisibility(PrefService* prefs) {
  const std::string visibility =
      prefs->GetString(prefs::kShowDeskButtonInShelf);
  if (!visibility.empty()) {
    return visibility == kDeskButtonInShelfShown;
  }
  return prefs->GetBoolean(prefs::kDeviceUsesDesks);
}

void SetShowDeskButtonInShelfPref(PrefService* prefs, bool show) {
  prefs->SetString(prefs::kShowDeskButtonInShelf,
                   show ? kDeskButtonInShelfShown : kDeskButtonInShelfHidden);
}

void SetDeviceUsesDesksPref(PrefService* prefs, bool uses_desks) {
  prefs->SetBoolean(prefs::kDeviceUsesDesks, uses_desks);
}

}  // namespace ash
