// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_prefs_custom_associator.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>

#include "ash/accessibility/accessibility_sync_prefs_utils.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check_is_test.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace ash {

namespace {

static AccessibilityPrefsCustomAssociator*
    g_accessibility_prefs_custom_associator = nullptr;

bool PrefNeedsResolution(const AccessibilityPrefBatchEntry& entry) {
  return entry.resolution_policy != ConflictResolutionPolicy::kNone;
}

}  // namespace

AccessibilityPrefsCustomAssociator::AccessibilityPrefsCustomAssociator(
    sync_preferences::PrefServiceSyncable* prefs)
    : enabled_sync_prefs_(GetAccessibilityPrefBatchesWithSyncEnabled()),
      prefs_(prefs) {
  CHECK(g_accessibility_prefs_custom_associator == nullptr);
  g_accessibility_prefs_custom_associator = this;

  if (!prefs_) {
    CHECK_IS_TEST();
    return;
  }

  for (auto& enabled_sync_pref : enabled_sync_prefs_) {
    if (PrefNeedsResolution(enabled_sync_pref)) {
      prefs_->AddSyncedPrefObserver(enabled_sync_pref.pref_name, this);
    }
  }
}

AccessibilityPrefsCustomAssociator::~AccessibilityPrefsCustomAssociator() {
  CHECK(g_accessibility_prefs_custom_associator);
  g_accessibility_prefs_custom_associator = nullptr;

  if (!prefs_) {
    return;
  }

  for (auto& enabled_sync_pref : enabled_sync_prefs_) {
    if (PrefNeedsResolution(enabled_sync_pref)) {
      prefs_->RemoveSyncedPrefObserver(enabled_sync_pref.pref_name, this);
    }
  }
}

// static
AccessibilityPrefsCustomAssociator* AccessibilityPrefsCustomAssociator::Get() {
  return g_accessibility_prefs_custom_associator;
}

std::optional<base::Value>
AccessibilityPrefsCustomAssociator::GetPreferredPrefMergeValue(
    std::string_view pref_name,
    const base::Value& server_value) const {
  if (!CanLockPref(pref_name)) {
    return std::nullopt;
  }

  base::Value val = GetPrefLockedValue(pref_name);
  if (val.is_none()) {
    return server_value.Clone();
  } else {
    return val;
  }
}

// A preference is lockable if it is one of the syncable accessibility
// preferences returned by ash::GetAccessibilityPrefBatchesWithSyncEnabled()
// and its configured conflict resolution policy requires conflict resolution.
// Preferences that do not meet these conditions are not handled by this
// associator.
bool AccessibilityPrefsCustomAssociator::CanLockPref(
    std::string_view pref_name) const {
  // TODO(crbug.com/485997708): Remove the lambda below when there is no
  // AccessibilityPrefBatchEntry set with ConflictResolutionPolicy::kNone left.
  auto it =
      std::find_if(enabled_sync_prefs_.begin(), enabled_sync_prefs_.end(),
                   [&](const auto& p) {
                     return p.pref_name == pref_name && PrefNeedsResolution(p);
                   });
  return it != enabled_sync_prefs_.end();
}

void AccessibilityPrefsCustomAssociator::TryLockPref(std::string_view pref_name,
                                                     const base::Value& value) {
  // This preference is not part of any batch of syncable accessibility
  // preferences. Hence, cannot lock it to the `value`.
  if (!CanLockPref(pref_name)) {
    return;
  }

  locked_prefs_.Set(pref_name, value.Clone());
}

void AccessibilityPrefsCustomAssociator::UnlockPref(
    std::string_view pref_name) {
  locked_prefs_.Remove(pref_name);
  account_store_prefs_.Remove(pref_name);
}

void AccessibilityPrefsCustomAssociator::UnlockAllPrefs() {
  locked_prefs_.clear();
  account_store_prefs_.clear();
}

bool AccessibilityPrefsCustomAssociator::IsPrefLocked(
    std::string_view pref_name) const {
  return !!locked_prefs_.Find(pref_name);
}

base::Value AccessibilityPrefsCustomAssociator::GetPrefLockedValue(
    std::string_view pref_name) const {
  const base::Value* value = locked_prefs_.Find(pref_name);
  return value ? value->Clone() : base::Value();
}

bool AccessibilityPrefsCustomAssociator::TrySetAccountStorePref(
    std::string_view pref_name,
    const base::Value& value) {
  // This preference is not part of the batch of syncable accessibility
  // preferences. Hence, ignore its account store value.
  if (!CanLockPref(pref_name)) {
    return false;
  }

  account_store_prefs_.Set(pref_name, value.Clone());
  return true;
}

AccessibilityPrefsCustomAssociator::LockedAndAccountStorePrefs
AccessibilityPrefsCustomAssociator::GetLockedAndAccountStorePrefs() const {
  return {
      locked_prefs_.Clone(),
      account_store_prefs_.Clone(),
  };
}

void AccessibilityPrefsCustomAssociator::ToString() const {
  VLOG(1) << " locked_prefs_: " << locked_prefs_;
  VLOG(1) << " account_store_prefs_: " << account_store_prefs_;
}

void AccessibilityPrefsCustomAssociator::OnStartedSyncing(
    std::string_view pref_name,
    const base::Value& sync_value) {
  if (!sync_value.is_none()) {
    TrySetAccountStorePref(pref_name, sync_value);
  }
}

}  // namespace ash
