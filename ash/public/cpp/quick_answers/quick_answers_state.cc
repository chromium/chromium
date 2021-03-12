// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/quick_answers/quick_answers_state.h"

#include "base/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

QuickAnswersState* g_quick_answers_state = nullptr;

}  // namespace

// static
QuickAnswersState* QuickAnswersState::Get() {
  return g_quick_answers_state;
}

QuickAnswersState::QuickAnswersState() {
  DCHECK(!g_quick_answers_state);
  g_quick_answers_state = this;
}

QuickAnswersState::~QuickAnswersState() {
  DCHECK_EQ(g_quick_answers_state, this);
  g_quick_answers_state = nullptr;
}

void QuickAnswersState::AddObserver(QuickAnswersStateObserver* observer) {
  observers_.AddObserver(observer);
  InitializeObserver(observer);
}

void QuickAnswersState::RemoveObserver(QuickAnswersStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void QuickAnswersState::RegisterPrefChanges(PrefService* pref_service) {
  pref_change_registrar_.reset();

  if (!pref_service)
    return;

  // Register preference changes.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      chromeos::quick_answers::prefs::kQuickAnswersEnabled,
      base::BindRepeating(&QuickAnswersState::UpdateSettingsEnabled,
                          base::Unretained(this)));

  UpdateSettingsEnabled();

  prefs_initialized_ = true;
}

void QuickAnswersState::InitializeObserver(
    QuickAnswersStateObserver* observer) {
  if (prefs_initialized_)
    observer->OnSettingsEnabled(settings_enabled_);
}

void QuickAnswersState::UpdateSettingsEnabled() {
  auto settings_enabled = pref_change_registrar_->prefs()->GetBoolean(
      chromeos::quick_answers::prefs::kQuickAnswersEnabled);
  if (prefs_initialized_ && settings_enabled_ == settings_enabled) {
    return;
  }
  settings_enabled_ = settings_enabled;
  for (auto& observer : observers_)
    observer.OnSettingsEnabled(settings_enabled_);
}

}  // namespace ash
