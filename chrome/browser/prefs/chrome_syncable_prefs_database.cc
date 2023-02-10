// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/chrome_syncable_prefs_database.h"

#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_piece.h"

namespace {
// Non-iOS specific list of syncable preferences.
constexpr auto kChromeSyncablePrefsAllowlist =
    base::MakeFixedFlatSet<base::StringPiece>({"dummy"});
}  // namespace

bool ChromeSyncablePrefsDatabase::IsPreferenceSyncable(
    const std::string& pref_name) const {
  return kChromeSyncablePrefsAllowlist.count(pref_name) ||
         // Also check if `pref_name` is part of the common set of syncable
         // preferences.
         common_syncable_prefs_database_.IsPreferenceSyncable(pref_name);
}
