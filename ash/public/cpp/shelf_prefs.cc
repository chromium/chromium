// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_prefs.h"

#include <memory>

#include "ash/public/cpp/ash_pref_names.h"
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
    const base::DictionaryValue* shelf_prefs =
        prefs->GetDictionary(prefs::kShelfPreferences);
    const base::DictionaryValue* display_pref = nullptr;
    std::string per_display_value;
    if (shelf_prefs->GetDictionary(pref_key, &display_pref) &&
        display_pref->GetString(path, &per_display_value))
      return per_display_value;

    // If the pref for the specified display is not found, scan the whole prefs
    // and check if the prefs for other display is already specified.
    std::string unused_value;
    for (base::DictionaryValue::Iterator iter(*shelf_prefs); !iter.IsAtEnd();
         iter.Advance()) {
      const base::DictionaryValue* display_pref = nullptr;
      if (iter.value().GetAsDictionary(&display_pref) &&
          display_pref->GetString(path, &unused_value)) {
        has_per_display_prefs = true;
        break;
      }
    }
  }

  if (local_pref->IsRecommended() || !has_per_display_prefs)
    return value;

  std::string default_string;
  prefs->GetDefaultPrefValue(local_path)->GetAsString(&default_string);
  return default_string;
}

// Sets the preference value for the display with the given |display_id|.
void SetPerDisplayPref(PrefService* prefs,
                       int64_t display_id,
                       const char* pref_key,
                       const std::string& value) {
  if (display_id == display::kInvalidDisplayId)
    return;

  // Avoid DictionaryPrefUpdate's notifications for read but unmodified prefs.
  const base::DictionaryValue* current_shelf_prefs =
      prefs->GetDictionary(prefs::kShelfPreferences);
  DCHECK(current_shelf_prefs);
  std::string display_key = base::NumberToString(display_id);
  const base::DictionaryValue* current_display_prefs = nullptr;
  std::string current_value;
  if (current_shelf_prefs->GetDictionary(display_key, &current_display_prefs) &&
      current_display_prefs->GetString(pref_key, &current_value) &&
      current_value == value) {
    return;
  }

  DictionaryPrefUpdate update(prefs, prefs::kShelfPreferences);
  base::DictionaryValue* shelf_prefs = update.Get();
  base::DictionaryValue* display_prefs_weak = nullptr;
  if (!shelf_prefs->GetDictionary(display_key, &display_prefs_weak)) {
    auto display_prefs = std::make_unique<base::DictionaryValue>();
    display_prefs_weak = display_prefs.get();
    shelf_prefs->Set(display_key, std::move(display_prefs));
  }
  display_prefs_weak->SetKey(pref_key, base::Value(value));
}

ShelfAlignment AlignmentFromPref(const std::string& value) {
  if (value == kShelfAlignmentLeft)
    return SHELF_ALIGNMENT_LEFT;
  if (value == kShelfAlignmentRight)
    return SHELF_ALIGNMENT_RIGHT;
  // Default to bottom.
  return SHELF_ALIGNMENT_BOTTOM;
}

const char* AlignmentToPref(ShelfAlignment alignment) {
  switch (alignment) {
    case SHELF_ALIGNMENT_BOTTOM:
      return kShelfAlignmentBottom;
    case SHELF_ALIGNMENT_LEFT:
      return kShelfAlignmentLeft;
    case SHELF_ALIGNMENT_RIGHT:
      return kShelfAlignmentRight;
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
      // This should not be a valid preference option for now. We only want to
      // lock the shelf during login or when adding a user.
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

ShelfAutoHideBehavior AutoHideBehaviorFromPref(const std::string& value) {
  // Note: To maintain sync compatibility with old images of chrome/chromeos
  // the set of values that may be encountered includes the now-extinct
  // "Default" as well as "Never" and "Always", "Default" should now
  // be treated as "Never" (http://crbug.com/146773).
  if (value == kShelfAutoHideBehaviorAlways)
    return SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS;
  return SHELF_AUTO_HIDE_BEHAVIOR_NEVER;
}

const char* AutoHideBehaviorToPref(ShelfAutoHideBehavior behavior) {
  switch (behavior) {
    case SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      return kShelfAutoHideBehaviorAlways;
    case SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      return kShelfAutoHideBehaviorNever;
    case SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
      // This should not be a valid preference option for now. We only want to
      // completely hide it when we run in app mode - or while we temporarily
      // hide the shelf (e.g. SessionAbortedDialog).
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

ShelfAutoHideBehavior GetShelfAutoHideBehaviorPref(PrefService* prefs,
                                                   int64_t display_id) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);

  // See comment in |kShelfAlignment| as to why we consider two prefs.
  return AutoHideBehaviorFromPref(
      GetPerDisplayPref(prefs, display_id, prefs::kShelfAutoHideBehaviorLocal,
                        prefs::kShelfAutoHideBehavior));
}

void SetShelfAutoHideBehaviorPref(PrefService* prefs,
                                  int64_t display_id,
                                  ShelfAutoHideBehavior behavior) {
  DCHECK_NE(display_id, display::kInvalidDisplayId);

  const char* value = AutoHideBehaviorToPref(behavior);
  if (!value)
    return;

  SetPerDisplayPref(prefs, display_id, prefs::kShelfAutoHideBehavior, value);
  if (display_id == display::Screen::GetScreen()->GetPrimaryDisplay().id()) {
    // See comment in |kShelfAlignment| about why we have two prefs here.
    prefs->SetString(prefs::kShelfAutoHideBehaviorLocal, value);
    prefs->SetString(prefs::kShelfAutoHideBehavior, value);
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

  SetPerDisplayPref(prefs, display_id, prefs::kShelfAlignment, value);
  if (display_id == display::Screen::GetScreen()->GetPrimaryDisplay().id()) {
    // See comment in |kShelfAlignment| as to why we consider two prefs.
    prefs->SetString(prefs::kShelfAlignmentLocal, value);
    prefs->SetString(prefs::kShelfAlignment, value);
  }
}

}  // namespace ash
