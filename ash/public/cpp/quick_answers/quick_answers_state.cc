// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/quick_answers/quick_answers_state.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace ash {

namespace {

QuickAnswersState* g_quick_answers_state = nullptr;

bool IsQuickAnswersAllowedForLocale(const std::string& locale,
                                    const std::string& runtime_locale) {
  // String literals used in some cases in the array because their
  // constant equivalents don't exist in:
  // third_party/icu/source/common/unicode/uloc.h
  const std::string kAllowedLocales[] = {ULOC_CANADA, ULOC_UK, ULOC_US,
                                         "en_AU",     "en_IN", "en_NZ"};
  return base::Contains(kAllowedLocales, locale) ||
         base::Contains(kAllowedLocales, runtime_locale);
}

}  // namespace

// static
QuickAnswersState* QuickAnswersState::Get() {
  return g_quick_answers_state;
}

QuickAnswersState::QuickAnswersState() {
  DCHECK(!g_quick_answers_state);
  g_quick_answers_state = this;
  DCHECK(AssistantState::Get());
  AssistantState::Get()->AddObserver(this);
}

QuickAnswersState::~QuickAnswersState() {
  if (AssistantState::Get())
    AssistantState::Get()->RemoveObserver(this);
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

void QuickAnswersState::OnAssistantFeatureAllowedChanged(
    chromeos::assistant::AssistantAllowedState state) {
  UpdateEligibility();
}

void QuickAnswersState::OnAssistantSettingsEnabled(bool enabled) {
  UpdateEligibility();
}

void QuickAnswersState::OnAssistantContextEnabled(bool enabled) {
  UpdateEligibility();
}

void QuickAnswersState::OnLocaleChanged(const std::string& locale) {
  UpdateEligibility();
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

void QuickAnswersState::UpdateEligibility() {
  auto* assistant_state = AssistantState::Get();
  if (!assistant_state->settings_enabled().has_value() ||
      !assistant_state->locale().has_value() ||
      !assistant_state->context_enabled().has_value() ||
      !assistant_state->allowed_state().has_value()) {
    // Assistant state has not finished initialization, let's wait.
    return;
  }
  is_eligible_ =
      (chromeos::features::IsQuickAnswersEnabled() &&
       assistant_state->settings_enabled().value() &&
       IsQuickAnswersAllowedForLocale(assistant_state->locale().value(),
                                      icu::Locale::getDefault().getName()) &&
       assistant_state->context_enabled().value() &&
       assistant_state->allowed_state().value() ==
           chromeos::assistant::AssistantAllowedState::ALLOWED);
}

}  // namespace ash
