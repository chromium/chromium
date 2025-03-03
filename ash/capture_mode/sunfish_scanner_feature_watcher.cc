// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/sunfish_scanner_feature_watcher.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/session/session_controller_impl.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "components/lens/lens_overlay_permission_utils.h"

namespace ash {

SunfishScannerFeatureWatcher::SunfishScannerFeatureWatcher(
    SessionControllerImpl& session_controller)
    : can_show_sunfish_ui_(::ash::CanShowSunfishUi()),
      can_show_scanner_ui_(ScannerController::CanShowUiForShell()),
      session_controller_(session_controller) {
  session_controller_->AddObserver(this);
  OnActiveUserPrefServiceChanged(session_controller_->GetActivePrefService());
}

SunfishScannerFeatureWatcher::~SunfishScannerFeatureWatcher() {
  session_controller_->RemoveObserver(this);
}

void SunfishScannerFeatureWatcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SunfishScannerFeatureWatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SunfishScannerFeatureWatcher::UpdateFeatureStates() {
  bool new_sunfish_state = ::ash::CanShowSunfishUi();
  bool new_scanner_state = ScannerController::CanShowUiForShell();

  if (new_sunfish_state == can_show_sunfish_ui_ &&
      new_scanner_state == can_show_scanner_ui_) {
    return;
  }

  can_show_sunfish_ui_ = new_sunfish_state;
  can_show_scanner_ui_ = new_scanner_state;

  for (Observer& observer : observers_) {
    observer.OnSunfishScannerFeatureStatesChanged(*this);
  }
}

void SunfishScannerFeatureWatcher::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_change_registrar_.prefs() == pref_service) {
    return;
  }

  UpdateFeatureStates();

  pref_change_registrar_.Reset();
  if (pref_service == nullptr) {
    // Do not add observers on a null pref service.
    return;
  }
  pref_change_registrar_.Init(pref_service);

  base::RepeatingClosure update_feature_states =
      base::BindRepeating(&SunfishScannerFeatureWatcher::UpdateFeatureStates,
                          weak_ptr_factory_.GetWeakPtr());

  // Sunfish prefs:
  pref_change_registrar_.Add(prefs::kSunfishEnabled, update_feature_states);
  pref_change_registrar_.Add(lens::prefs::kLensOverlaySettings,
                             update_feature_states);
  pref_change_registrar_.Add(lens::prefs::kGenAiLensOverlaySettings,
                             update_feature_states);

  // Scanner prefs:
  pref_change_registrar_.Add(prefs::kScannerEnabled, update_feature_states);
  pref_change_registrar_.Add(prefs::kScannerEnterprisePolicyAllowed,
                             update_feature_states);
  // We do not need to observe Scanner consent, as that does not affect whether
  // UI can be shown.
}

}  // namespace ash
