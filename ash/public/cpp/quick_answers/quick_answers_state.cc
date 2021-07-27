// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/quick_answers/quick_answers_state.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

using chromeos::assistant::prefs::kAssistantContextEnabled;
using chromeos::assistant::prefs::kAssistantEnabled;
using chromeos::quick_answers::prefs::ConsentStatus;
using chromeos::quick_answers::prefs::kQuickAnswersConsentStatus;
using chromeos::quick_answers::prefs::kQuickAnswersDefinitionEnabled;
using chromeos::quick_answers::prefs::kQuickAnswersEnabled;
using chromeos::quick_answers::prefs::kQuickAnswersNoticeImpressionCount;
using chromeos::quick_answers::prefs::kQuickAnswersNoticeImpressionDuration;
using chromeos::quick_answers::prefs::kQuickAnswersTranslationEnabled;
using chromeos::quick_answers::prefs::kQuickAnswersUnitConverstionEnabled;

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

void MigrateQuickAnswersConsentStatus(PrefService* prefs) {
  // If the consented status has not been set, migrate the current context
  // enabled value.
  if (prefs->FindPreference(kQuickAnswersConsentStatus)->IsDefaultValue()) {
    if (!prefs->FindPreference(kAssistantContextEnabled)->IsDefaultValue()) {
      // Set the consent status based on current feature eligibility.
      bool consented =
          ash::AssistantState::Get()->allowed_state() ==
              chromeos::assistant::AssistantAllowedState::ALLOWED &&
          prefs->GetBoolean(kAssistantEnabled) &&
          prefs->GetBoolean(kAssistantContextEnabled);
      prefs->SetInteger(
          kQuickAnswersConsentStatus,
          consented ? ConsentStatus::kAccepted : ConsentStatus::kRejected);
    } else {
      // Set the consent status to unknown for new users.
      prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kUnknown);
      // Reset the impression count and duration for new users.
      prefs->SetInteger(kQuickAnswersNoticeImpressionCount, 0);
      prefs->SetInteger(kQuickAnswersNoticeImpressionDuration, 0);
    }
  }
}

void IncrementPrefCounter(PrefService* prefs,
                          const std::string& path,
                          int count) {
  prefs->SetInteger(path, prefs->GetInteger(path) + count);
}

}  // namespace

// static
QuickAnswersState* QuickAnswersState::Get() {
  return g_quick_answers_state;
}

QuickAnswersState::QuickAnswersState() {
  DCHECK(!g_quick_answers_state);
  g_quick_answers_state = this;
  if (!chromeos::features::IsQuickAnswersV2Enabled()) {
    DCHECK(AssistantState::Get());
    AssistantState::Get()->AddObserver(this);
  }
}

QuickAnswersState::~QuickAnswersState() {
  if (!chromeos::features::IsQuickAnswersV2Enabled() && AssistantState::Get()) {
    AssistantState::Get()->RemoveObserver(this);
  }
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
      kQuickAnswersEnabled,
      base::BindRepeating(&QuickAnswersState::UpdateSettingsEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersConsentStatus,
      base::BindRepeating(&QuickAnswersState::UpdateConsentStatus,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersDefinitionEnabled,
      base::BindRepeating(&QuickAnswersState::UpdateDefinitionEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersTranslationEnabled,
      base::BindRepeating(&QuickAnswersState::UpdateTranslationEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      kQuickAnswersUnitConverstionEnabled,
      base::BindRepeating(&QuickAnswersState::UpdateUnitConverstionEnabled,
                          base::Unretained(this)));

  UpdateSettingsEnabled();
  UpdateConsentStatus();
  UpdateDefinitionEnabled();
  UpdateTranslationEnabled();
  UpdateUnitConverstionEnabled();

  prefs_initialized_ = true;

  MigrateQuickAnswersConsentStatus(pref_service);
  UpdateEligibility();
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

void QuickAnswersState::StartConsent() {
  // Increments impression count.
  IncrementPrefCounter(pref_change_registrar_->prefs(),
                       kQuickAnswersNoticeImpressionCount, 1);

  consent_start_time_ = base::TimeTicks::Now();
}

void QuickAnswersState::OnConsentResult(ConsentResultType result) {
  auto* prefs = pref_change_registrar_->prefs();

  // Increments impression duration.
  DCHECK(!consent_start_time_.is_null());
  auto duration = base::TimeTicks::Now() - consent_start_time_;
  IncrementPrefCounter(prefs, kQuickAnswersNoticeImpressionDuration,
                       duration.InSeconds());

  switch (result) {
    case ConsentResultType::kAllow:
      prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kAccepted);
      break;
    case ConsentResultType::kNoThanks:
      prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kRejected);
      prefs->SetBoolean(kQuickAnswersEnabled, false);
      break;
    case ConsentResultType::kDismiss:
      // If the count or duration cap is reached, set the consented status to
      // false;
      bool impression_cap_reached =
          prefs->GetInteger(kQuickAnswersNoticeImpressionCount) >=
          kConsentImpressionCap;
      bool duration_cap_reached =
          prefs->GetInteger(kQuickAnswersNoticeImpressionDuration) >=
          kConsentDurationCap;
      if (impression_cap_reached || duration_cap_reached) {
        prefs->SetInteger(kQuickAnswersConsentStatus, ConsentStatus::kRejected);
        prefs->SetBoolean(kQuickAnswersEnabled, false);
      }
  }

  consent_start_time_ = base::TimeTicks();
}

void QuickAnswersState::InitializeObserver(
    QuickAnswersStateObserver* observer) {
  if (prefs_initialized_)
    observer->OnSettingsEnabled(settings_enabled_);
}

void QuickAnswersState::UpdateSettingsEnabled() {
  auto settings_enabled =
      pref_change_registrar_->prefs()->GetBoolean(kQuickAnswersEnabled);
  if (settings_enabled_ == settings_enabled) {
    return;
  }
  settings_enabled_ = settings_enabled;

  // If the user turn on the Quick Answers in settings, set the consented status
  // to true.
  if (settings_enabled_) {
    pref_change_registrar_->prefs()->SetInteger(kQuickAnswersConsentStatus,
                                                ConsentStatus::kAccepted);
  }

  for (auto& observer : observers_)
    observer.OnSettingsEnabled(settings_enabled_);

  UpdateEligibility();
}

void QuickAnswersState::UpdateConsentStatus() {
  auto consent_status = static_cast<ConsentStatus>(
      pref_change_registrar_->prefs()->GetInteger(kQuickAnswersConsentStatus));
  if (consent_status_ == consent_status) {
    return;
  }
  consent_status_ = consent_status;

  // If the user allow Quick Answers consent, turn on the Quick Answers settings
  // pref.
  if (consent_status_) {
    pref_change_registrar_->prefs()->SetBoolean(kQuickAnswersEnabled, true);
  }
}

void QuickAnswersState::UpdateDefinitionEnabled() {
  auto definition_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersDefinitionEnabled);
  if (definition_enabled_ == definition_enabled) {
    return;
  }
  definition_enabled_ = definition_enabled;
}

void QuickAnswersState::UpdateTranslationEnabled() {
  auto translation_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersTranslationEnabled);
  if (translation_enabled_ == translation_enabled) {
    return;
  }
  translation_enabled_ = translation_enabled;
}

void QuickAnswersState::UpdateUnitConverstionEnabled() {
  auto unit_conversion_enabled = pref_change_registrar_->prefs()->GetBoolean(
      kQuickAnswersUnitConverstionEnabled);
  if (unit_conversion_enabled_ == unit_conversion_enabled) {
    return;
  }
  unit_conversion_enabled_ = unit_conversion_enabled;
}

void QuickAnswersState::UpdateEligibility() {
  if (chromeos::features::IsQuickAnswersV2Enabled()) {
    if (!pref_change_registrar_)
      return;

    std::string locale = pref_change_registrar_->prefs()->GetString(
        language::prefs::kApplicationLocale);
    std::string resolved_locale;
    l10n_util::CheckAndResolveLocale(locale, &resolved_locale,
                                     /*perform_io=*/false);
    bool should_show_consent = consent_status_ == ConsentStatus::kUnknown;
    is_eligible_ = (settings_enabled_ || should_show_consent) &&
                   IsQuickAnswersAllowedForLocale(
                       resolved_locale, icu::Locale::getDefault().getName());
    return;
  }

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
