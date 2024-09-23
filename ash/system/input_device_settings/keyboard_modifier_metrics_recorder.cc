// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/system/input_device_settings/keyboard_modifier_metrics_recorder.h"

#include <cstdint>
#include <memory>
#include <string_view>

#include "ash/constants/ash_features.h"
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
#include "ui/events/ash/mojom/modifier_key.mojom.h"

namespace ash {
namespace {
constexpr std::string_view kModifierMetricPrefix =
    "ChromeOS.Settings.Keyboard.Modifiers.";
constexpr std::string_view kModifierMetricIndividualChangedSuffix =
    "RemappedTo.Changed";
constexpr std::string_view kModifierMetricIndividualInitSuffix =
    "RemappedTo.Started";
constexpr std::string_view kModifierMetricHash =
    "ChromeOS.Settings.Keyboard.Modifiers.Hash";

// The modifier hash is made up of `kNumModifiers` blocks of
// `kModifierHashWidth` bits. Each modifier is assigned a `kModifierHashWidth`
// width block to track its user configured setting. These user configured
// settings are contained within [0, `kMaxModifierValue`] and are assigned in
// /ash/public/input_device_settings.mojom in the `mojom::ModifierKey` struct.
// Indices are assigned to each modifier based on the order of the table
// `KeyboardModifierMetricsRecorder::kKeyboardModifierPrefs`.

// To decode, break up the hash into `kModifierHashWidth` bit integers.
// For example, if `kModifierHashWidth` is 3, use the following bit ranges to
// extract the value of the remapped modifier:

// | Index in kKeyboardModifierPrefs | Bit Range |
// | 0                               | [0, 2]    |
// | 1                               | [3, 5]    |
// | 2                               | [6, 8]    |
// | 3                               | [9, 11]   |
// | 4                               | [12, 14]  |
// | 5                               | [15, 17]  |
// | 6                               | [18, 20]  |
// | 7                               | [21, 23]  |
// | 8                               | [24, 26]  |
// | 9                               | [27, 29]  |

constexpr int kModifierHashWidth = 3;
constexpr int kMaxModifierValue = (1 << kModifierHashWidth) - 1;
constexpr int kNumModifiers =
    std::size(KeyboardModifierMetricsRecorder::kKeyboardModifierPrefs);

// Verify that the number of modifiers we are trying to hash together into a
// 32-bit int will fit without any overflow or UB.
// Modifier hash is limited to 32 bits as metrics can only handle 32 bit ints.
static_assert((sizeof(int32_t) * 8) >= (kModifierHashWidth * kNumModifiers));
// `kIsoLevel5ShiftMod3`, `kRightAlt`, and `kFunction` are not valid modifiers
// for this metric. Therefore there are 3 less values here than are contained in
// the enum.
static_assert(static_cast<int>(ui::mojom::ModifierKey::kMaxValue) - 3 <=
              kMaxModifierValue);

constexpr ui::mojom::ModifierKey GetDefaultModifier(size_t index) {
  return KeyboardModifierMetricsRecorder::kKeyboardModifierPrefs[index]
      .default_modifier_key;
}

// Precomputes the value of the modifier hash when all prefs are configured to
// their default value.
constexpr int32_t PrecalculateDefaultModifierHash() {
  uint32_t hash = 0;
  for (ssize_t i = kNumModifiers - 1u; i >= 0; i--) {
    hash <<= kModifierHashWidth;
    hash += static_cast<int>(GetDefaultModifier(i));
  }
  return static_cast<uint32_t>(hash);
}
constexpr int32_t kDefaultModifierHash = PrecalculateDefaultModifierHash();
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
                                  static_cast<ui::mojom::ModifierKey>(value));
    }
    RecordModifierRemappingHash();
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
  RecordModifierRemappingChanged(index,
                                 static_cast<ui::mojom::ModifierKey>(value));
}

void KeyboardModifierMetricsRecorder::RecordModifierRemappingChanged(
    size_t index,
    ui::mojom::ModifierKey modifier_key) {
  const std::string changed_metric_name = base::StrCat(
      {kModifierMetricPrefix, kKeyboardModifierPrefs[index].key_name,
       kModifierMetricIndividualChangedSuffix});
  base::UmaHistogramEnumeration(changed_metric_name, modifier_key);
}

void KeyboardModifierMetricsRecorder::RecordModifierRemappingInit(
    size_t index,
    ui::mojom::ModifierKey modifier_key) {
  DCHECK_LT(index, std::size(kKeyboardModifierPrefs));

  // Skip publishing the metric if the pref is set to its default value.
  if (GetDefaultModifier(index) == modifier_key) {
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

void KeyboardModifierMetricsRecorder::RecordModifierRemappingHash() {
  // Compute hash by left-shifting by `kModifierHashWidth` and then inserting
  // the modifier value from prefs at into the lowest `kModifierHashWidth` bits.
  // Repeat for all prefs in `kKeyboardModifierPrefs`.
  uint32_t hash = 0;
  for (ssize_t i = pref_members_.size() - 1; i >= 0; i--) {
    const int value = pref_members_[i]->GetValue();

    // Check that shifting and adding value will not overflow `hash`.
    DCHECK(IsValidModifier(value));
    DCHECK(hash < (1u << ((sizeof(uint32_t) * 8u) - kModifierHashWidth)));

    hash <<= kModifierHashWidth;
    hash += value;
  }

  // If the computed hash matches the hash when settings are in a default state,
  // the metric should not be published.
  if (hash != kDefaultModifierHash) {
    base::UmaHistogramSparse(kModifierMetricHash, static_cast<int>(hash));
  }
}

}  // namespace ash
