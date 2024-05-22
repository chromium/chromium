// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_prefs_controller_lacros.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/prefs/pref_service.h"

namespace {

void SetPref(crosapi::mojom::PrefPath path, base::Value value) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service ||
      !lacros_service->IsAvailable<crosapi::mojom::Prefs>()) {
    LOG(WARNING) << "crosapi: Prefs API not available";
    return;
  }
  return lacros_service->GetRemote<crosapi::mojom::Prefs>()->SetPref(
      path, std::move(value), /*callback=*/base::DoNothing());
}

}  // namespace

namespace mahi {

MahiPrefsControllerLacros::MahiPrefsControllerLacros() {
  // The observers are fired immediate with the current pref value on
  // initialization.
  // TODO(b/341844502): Consolidate the observer in `PrefsAshObserver` into this
  // one.
  mahi_enabled_observer_ = std::make_unique<CrosapiPrefObserver>(
      crosapi::mojom::PrefPath::kMahiEnabled,
      base::BindRepeating(&MahiPrefsControllerLacros::OnMahiEnableStateChanged,
                          base::Unretained(this)));
}

MahiPrefsControllerLacros::~MahiPrefsControllerLacros() = default;

void MahiPrefsControllerLacros::SetMahiEnabled(bool enabled) {
  SetPref(crosapi::mojom::PrefPath::kMahiEnabled, base::Value(enabled));
}

void MahiPrefsControllerLacros::OnMahiEnableStateChanged(base::Value value) {
  DCHECK(value.is_bool());
  bool mahi_enabled = value.GetBool();

  if (mahi_enabled) {
    // TODO(b/341485303): If the user turn on Mahi in settings, set the Magic
    // Boost consented status to true.
  }
}

}  // namespace mahi
