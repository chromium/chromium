// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_prefs_controller_ash.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace mahi {

MahiPrefsControllerAsh::MahiPrefsControllerAsh() {
  // `Shell` might not be available in tests.
  if (!ash::Shell::HasInstance()) {
    return;
  }

  auto* session_controller = ash::Shell::Get()->session_controller();
  CHECK(session_controller);

  session_observation_.Observe(session_controller);

  // Register pref changes if use session already started.
  if (session_controller->IsActiveUserSessionStarted()) {
    PrefService* prefs = session_controller->GetActivePrefService();
    CHECK(prefs);
    RegisterPrefChanges(prefs);
  }
}

MahiPrefsControllerAsh::~MahiPrefsControllerAsh() = default;

void MahiPrefsControllerAsh::OnFirstSessionStarted() {
  CHECK(ash::Shell::Get()->session_controller());
  PrefService* prefs =
      ash::Shell::Get()->session_controller()->GetActivePrefService();
  RegisterPrefChanges(prefs);
}

void MahiPrefsControllerAsh::OnChromeTerminating() {
  session_observation_.Reset();
}

void MahiPrefsControllerAsh::OnShellDestroying() {
  session_observation_.Reset();
  shell_observation_.Reset();
}

void MahiPrefsControllerAsh::RegisterPrefChanges(PrefService* pref_service) {
  pref_change_registrar_.reset();

  if (!pref_service) {
    return;
  }

  // Register preference changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      ash::prefs::kMahiEnabled,
      base::BindRepeating(&MahiPrefsControllerAsh::OnMahiEnableStateChanged,
                          base::Unretained(this)));

  OnMahiEnableStateChanged();
}

void MahiPrefsControllerAsh::SetMahiEnabled(bool enabled) {
  pref_change_registrar_->prefs()->SetBoolean(ash::prefs::kMahiEnabled,
                                              enabled);
}

void MahiPrefsControllerAsh::OnMahiEnableStateChanged() {
  if (GetMahiEnabled()) {
    // TODO(b/341485303): If the user turn on Mahi in settings, set the Magic
    // Boost consented status to true.
  }
}

}  // namespace mahi
