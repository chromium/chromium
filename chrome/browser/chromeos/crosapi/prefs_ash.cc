// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/prefs_ash.h"

#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/check.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"

namespace crosapi {

PrefsAsh::PrefsAsh(PrefService* local_state, PrefService* profile_prefs)
    : local_state_(local_state), profile_prefs_(profile_prefs) {
  DCHECK(local_state_);
  DCHECK(profile_prefs_);
  local_state_registrar_.Init(local_state_);
  profile_prefs_registrar_.Init(profile_prefs_);
}

PrefsAsh::~PrefsAsh() = default;

void PrefsAsh::BindReceiver(mojo::PendingReceiver<mojom::Prefs> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PrefsAsh::GetPref(mojom::PrefPath path, GetPrefCallback callback) {
  auto state = GetState(path);
  const base::Value* value =
      state ? state->pref_service->Get(state->path) : nullptr;
  std::move(callback).Run(value ? base::Optional<base::Value>(value->Clone())
                                : base::nullopt);
}

void PrefsAsh::SetPref(mojom::PrefPath path,
                       base::Value value,
                       SetPrefCallback callback) {
  auto state = GetState(path);
  if (state) {
    state->pref_service->Set(state->path, value);
  }
  std::move(callback).Run();
}

void PrefsAsh::AddObserver(mojom::PrefPath path,
                           mojo::PendingRemote<mojom::PrefObserver> observer) {
  auto state = GetState(path);
  const base::Value* value =
      state ? state->pref_service->Get(state->path) : nullptr;
  if (!value) {
    return;
  }

  // Fire the observer with the initial value.
  mojo::Remote<mojom::PrefObserver> remote(std::move(observer));
  remote->OnPrefChanged(value->Clone());

  if (!state->registrar->IsObserved(state->path)) {
    // Unretained() is safe since PrefChangeRegistrar and RemoteSet within
    // observers_ are owned by this and wont invoke if PrefsAsh is destroyed.
    state->registrar->Add(state->path,
                          base::BindRepeating(&PrefsAsh::OnPrefChanged,
                                              base::Unretained(this), path));
    observers_[path].set_disconnect_handler(base::BindRepeating(
        &PrefsAsh::OnDisconnect, base::Unretained(this), path));
  }
  observers_[path].Add(std::move(remote));
}

base::Optional<PrefsAsh::State> PrefsAsh::GetState(mojom::PrefPath path) {
  switch (path) {
    case mojom::PrefPath::kMetricsReportingEnabled:
      return State{local_state_, &local_state_registrar_,
                   metrics::prefs::kMetricsReportingEnabled};
    case mojom::PrefPath::kAccessibilitySpokenFeedbackEnabled:
      return State{profile_prefs_, &profile_prefs_registrar_,
                   ash::prefs::kAccessibilitySpokenFeedbackEnabled};
    default:
      LOG(WARNING) << "Unknown pref path: " << path;
      return base::nullopt;
  }
}

void PrefsAsh::OnPrefChanged(mojom::PrefPath path) {
  auto state = GetState(path);
  const base::Value* value =
      state ? state->pref_service->Get(state->path) : nullptr;
  if (value) {
    for (auto& observer : observers_[path]) {
      observer->OnPrefChanged(value->Clone());
    }
  }
}

void PrefsAsh::OnDisconnect(mojom::PrefPath path, mojo::RemoteSetElementId id) {
  const auto& it = observers_.find(path);
  if (it != observers_.end() && it->second.empty()) {
    if (auto state = GetState(path)) {
      state->registrar->Remove(state->path);
    }
    observers_.erase(it);
  }
}

}  // namespace crosapi
