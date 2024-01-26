// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_STORE_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_STORE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash::input_method {

// Manages consent status read/write from and to the user prefs.
// Each user has a separate consent status bound with their pref
// store.
class EditorConsentStore {
 public:
  explicit EditorConsentStore(PrefService* pref_service,
                              EditorMetricsRecorder* metrics_recorder);
  EditorConsentStore(const EditorConsentStore&) = delete;
  EditorConsentStore& operator=(const EditorConsentStore&) = delete;
  ~EditorConsentStore();

  ConsentStatus GetConsentStatus() const;

  // Updates the consent status based on user consent action.
  void ProcessConsentAction(ConsentAction consent_action);

  void ProcessPromoCardAction(PromoCardAction promo_card_action);

  void SetPrefService(PrefService* pref_service);

 private:
  void SetConsentStatus(ConsentStatus consent_status);

  // Updates the consent status based on the change in the user prefs.
  void OnUserPrefChanged();

  void OverrideUserPref(bool new_pref_value);

  void InitializePrefChangeRegistrar(PrefService* pref_service);

  // Not owned by this class.
  raw_ptr<PrefService> pref_service_;
  raw_ptr<EditorMetricsRecorder> metrics_recorder_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::WeakPtrFactory<EditorConsentStore> weak_ptr_factory_{this};
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_CONSENT_STORE_H_
