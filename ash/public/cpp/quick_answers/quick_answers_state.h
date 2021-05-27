// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_QUICK_ANSWERS_QUICK_ANSWERS_STATE_H_
#define ASH_PUBLIC_CPP_QUICK_ANSWERS_QUICK_ANSWERS_STATE_H_

#include <memory>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefChangeRegistrar;
class PrefService;

namespace ash {

// A checked observer which receives Quick Answers state change.
class ASH_PUBLIC_EXPORT QuickAnswersStateObserver
    : public base::CheckedObserver {
 public:
  virtual void OnSettingsEnabled(bool enabled) {}
};

// A class that holds Quick Answers related prefs and states.
class ASH_PUBLIC_EXPORT QuickAnswersState : public AssistantStateObserver {
 public:
  static QuickAnswersState* Get();

  QuickAnswersState();

  QuickAnswersState(const QuickAnswersState&) = delete;
  QuickAnswersState& operator=(const QuickAnswersState&) = delete;

  ~QuickAnswersState() override;

  void AddObserver(QuickAnswersStateObserver* observer);
  void RemoveObserver(QuickAnswersStateObserver* observer);

  void RegisterPrefChanges(PrefService* pref_service);

  // AssistantStateObserver:
  void OnAssistantFeatureAllowedChanged(
      chromeos::assistant::AssistantAllowedState state) override;
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantContextEnabled(bool enabled) override;
  void OnLocaleChanged(const std::string& locale) override;

  bool settings_enabled() const { return settings_enabled_; }
  bool is_eligible() const { return is_eligible_; }

  void set_eligibility_for_testing(bool is_eligible) {
    is_eligible_ = is_eligible;
  }

 private:
  void InitializeObserver(QuickAnswersStateObserver* observer);

  // Called when the related preferences are obtained from the pref service.
  void UpdateSettingsEnabled();

  // Called when the feature eligibility might change.
  void UpdateEligibility();

  // Whether the Quick Answers is enabled in system settings.
  bool settings_enabled_ = false;

  // Whether the Quick Answers feature is eligible. The value is derived from a
  // number of other states.
  bool is_eligible_ = false;

  // Whether the pref values has been initialized.
  bool prefs_initialized_ = false;

  // Observes user profile prefs for the Assistant.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ObserverList<QuickAnswersStateObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_QUICK_ANSWERS_QUICK_ANSWERS_STATE_H_
