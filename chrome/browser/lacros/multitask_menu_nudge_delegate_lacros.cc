// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/multitask_menu_nudge_delegate_lacros.h"

#include "base/barrier_callback.h"
#include "base/json/values_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/lacros/lacros_service.h"
#include "multitask_menu_nudge_delegate_lacros.h"

namespace {

// If we can't get the pref, run the callback with the max nudge show count
// value so that no nudge gets shown.
// TODO(b/267787811): The callback should instead be updated to provide a
// success parameter.
constexpr int kMaxNudgeShowCount = 3;

chromeos::LacrosService* GetLacrosService() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service && lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    return lacros_service;
  }
  return nullptr;
}

}  // namespace

MultitaskMenuNudgeDelegateLacros::MultitaskMenuNudgeDelegateLacros() = default;

MultitaskMenuNudgeDelegateLacros::~MultitaskMenuNudgeDelegateLacros() = default;

int MultitaskMenuNudgeDelegateLacros::GetTabletNudgeYOffset() const {
  // Tablet nudge is handled by ash.
  NOTREACHED();
  return 0;
}

void MultitaskMenuNudgeDelegateLacros::GetNudgePreferences(
    bool tablet_mode,
    GetPreferencesCallback callback) {
  // These prefs should be read from ash, as they are also used by frames
  // created and maintained in ash.
  auto* lacros_service = GetLacrosService();
  if (!lacros_service) {
    std::move(callback).Run(/*tablet_mode=*/false, kMaxNudgeShowCount,
                            base::Time());
    return;
  }

  auto barrier = base::BarrierCallback<PrefPair>(
      /*num_callbacks=*/2u, /*done_callback=*/base::BindOnce(
          &MultitaskMenuNudgeDelegateLacros::OnGotAllPreferences,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  lacros_service->GetRemote<crosapi::mojom::Prefs>()->GetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount,
      base::BindOnce(
          &MultitaskMenuNudgeDelegateLacros::OnGetPreference,
          weak_ptr_factory_.GetWeakPtr(), barrier,
          crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount));
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->GetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellLastShown,
      base::BindOnce(
          &MultitaskMenuNudgeDelegateLacros::OnGetPreference,
          weak_ptr_factory_.GetWeakPtr(), barrier,
          crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellLastShown));
}

void MultitaskMenuNudgeDelegateLacros::SetNudgePreferences(bool tablet_mode,
                                                           int count,
                                                           base::Time time) {
  auto* lacros_service = GetLacrosService();
  if (!lacros_service) {
    return;
  }

  lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount,
      base::Value(count), base::DoNothing());
  lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
      crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellLastShown,
      base::TimeToValue(time), base::DoNothing());
}

void MultitaskMenuNudgeDelegateLacros::OnGetPreference(
    base::OnceCallback<void(PrefPair)> callback,
    crosapi::mojom::PrefPath pref_path,
    absl::optional<base::Value> value) {
  // If `value` is empty just pass a default `base::Value`; the other callback
  // (`OnGotAllPreferences()`) function will handle it properly.
  PrefPair pref_pair{pref_path, value ? std::move(*value) : base::Value()};
  std::move(callback).Run(std::move(pref_pair));
}

void MultitaskMenuNudgeDelegateLacros::OnGotAllPreferences(
    GetPreferencesCallback callback,
    std::vector<PrefPair> pref_values) {
  DCHECK_EQ(2u, pref_values.size());

  // The values in the array could be in any order. Parse them into the
  // `shown_count` and `last_shown_time`.
  absl::optional<int> shown_count;
  absl::optional<base::Time> last_shown_time;
  for (const auto& pair : pref_values) {
    if (pair.first ==
        crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellShownCount) {
      shown_count = pair.second.GetIfInt().value_or(kMaxNudgeShowCount);
    } else {
      DCHECK_EQ(
          pair.first,
          crosapi::mojom::PrefPath::kMultitaskMenuNudgeClamshellLastShown);
      last_shown_time = base::ValueToTime(pair.second).value_or(base::Time());
    }
  }

  DCHECK(shown_count.has_value());
  DCHECK(last_shown_time.has_value());
  std::move(callback).Run(/*tablet_mode=*/false, *shown_count,
                          *last_shown_time);
}

bool MultitaskMenuNudgeDelegateLacros::IsUserNew() const {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  return profile && profile->IsNewProfile();
}
