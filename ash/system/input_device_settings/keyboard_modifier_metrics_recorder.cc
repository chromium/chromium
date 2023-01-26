// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/keyboard_modifier_metrics_recorder.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {
constexpr base::StringPiece kModifierMetricPrefix =
    "ChromeOS.Settings.Keyboard.Modifiers.";
constexpr base::StringPiece kModifierMetricIndividualChangedSuffix =
    "RemappedTo.Changed";
constexpr base::StringPiece kModifierMetricIndividualInitSuffix =
    "RemappedTo.Started";
}  // namespace

KeyboardModifierMetricsRecorder::KeyboardModifierMetricsRecorder() {
  Shell::Get()->session_controller()->AddObserver(this);
}

KeyboardModifierMetricsRecorder::~KeyboardModifierMetricsRecorder() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void KeyboardModifierMetricsRecorder::RegisterProfilePrefs(
    PrefRegistrySimple* registry,
    bool for_test) {
  if (for_test) {
    for (const auto& modifier_pref : kKeyboardModifierPrefs) {
      registry->RegisterIntegerPref(
          modifier_pref.pref_name,
          static_cast<int>(modifier_pref.default_modifier_key));
    }
  }
}

void KeyboardModifierMetricsRecorder::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  // Once per device settings launches, this method of publishing metrics will
  // no longer work.
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    // Initialize all pref members with the updated `pref_service`. Callback for
    // each pref member is
    // `KeyboardModifierMetricsRecorder::OnModifierRemappingChanged` where the
    // index is bound into the callback so it is known which modifier the
    // callback applies to.
    ResetPrefMembers();
    for (size_t index = 0; index < std::size(kKeyboardModifierPrefs); index++) {
      pref_members_[index]->Init(
          kKeyboardModifierPrefs[index].pref_name, pref_service,
          base::BindRepeating(
              &KeyboardModifierMetricsRecorder::OnModifierRemappingChanged,
              base::Unretained(this), index));
    }

    // Track the account ids that have already had a metric recorded so it is
    // only emitted once.
    const auto account_id =
        Shell::Get()->session_controller()->GetActiveAccountId();
    if (base::Contains(recorded_accounts_, account_id)) {
      return;
    }
    recorded_accounts_.insert(account_id);

    // Record the metric for each pref member.
    for (size_t index = 0; index < pref_members_.size(); index++) {
      const auto& pref_member = pref_members_[index];
      DCHECK(pref_member);

      const int value = pref_member->GetValue();
      DCHECK(IsValidModifier(value));
      RecordModifierRemappingInit(index,
                                  static_cast<mojom::ModifierKey>(value));
    }
  }
}

void KeyboardModifierMetricsRecorder::OnModifierRemappingChanged(
    size_t index,
    const std::string& pref_name) {
  DCHECK_LT(index, pref_members_.size());
  IntegerPrefMember* pref_member = pref_members_[index].get();
  DCHECK(pref_member);
  DCHECK_EQ(pref_member->GetPrefName(), pref_name);

  int value = pref_member->GetValue();
  DCHECK(IsValidModifier(value));
  RecordModifierRemappingChanged(index, static_cast<mojom::ModifierKey>(value));
}

void KeyboardModifierMetricsRecorder::RecordModifierRemappingChanged(
    size_t index,
    mojom::ModifierKey modifier_key) {
  const std::string changed_metric_name = base::StrCat(
      {kModifierMetricPrefix, kKeyboardModifierPrefs[index].key_name,
       kModifierMetricIndividualChangedSuffix});
  base::UmaHistogramEnumeration(changed_metric_name, modifier_key);
}

void KeyboardModifierMetricsRecorder::RecordModifierRemappingInit(
    size_t index,
    mojom::ModifierKey modifier_key) {
  DCHECK_LT(index, std::size(kKeyboardModifierPrefs));
  // Skip publishing the metric if the pref is set to its default value.
  if (kKeyboardModifierPrefs[index].default_modifier_key == modifier_key) {
    return;
  }

  const std::string changed_metric_name = base::StrCat(
      {kModifierMetricPrefix, kKeyboardModifierPrefs[index].key_name,
       kModifierMetricIndividualInitSuffix});
  base::UmaHistogramEnumeration(changed_metric_name, modifier_key);
}

void KeyboardModifierMetricsRecorder::ResetPrefMembers() {
  for (auto& pref_member : pref_members_) {
    pref_member = std::make_unique<IntegerPrefMember>();
  }
}

}  // namespace ash
