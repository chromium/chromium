// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_devices_controller.h"

#include <utility>

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

namespace {

PrefService* GetActivePrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

}  // namespace

// static
void TouchDevicesController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kTapDraggingEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PRIORITY_PREF);
  registry->RegisterBooleanPref(prefs::kTouchpadEnabled, true);
  registry->RegisterBooleanPref(prefs::kTouchscreenEnabled, true);
}

TouchDevicesController::TouchDevicesController() {
  Shell::Get()->session_controller()->AddObserver(this);
}

TouchDevicesController::~TouchDevicesController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void TouchDevicesController::ToggleTouchpad() {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  const bool touchpad_enabled = prefs->GetBoolean(prefs::kTouchpadEnabled);
  prefs->SetBoolean(prefs::kTouchpadEnabled, !touchpad_enabled);
}

bool TouchDevicesController::GetTouchpadEnabled(
    TouchDeviceEnabledSource source) const {
  if (source == TouchDeviceEnabledSource::GLOBAL)
    return global_touchpad_enabled_;

  PrefService* prefs = GetActivePrefService();
  return prefs && prefs->GetBoolean(prefs::kTouchpadEnabled);
}

void TouchDevicesController::SetTouchpadEnabled(
    bool enabled,
    TouchDeviceEnabledSource source) {
  if (source == TouchDeviceEnabledSource::GLOBAL) {
    global_touchpad_enabled_ = enabled;
    UpdateTouchpadEnabled();
    return;
  }

  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kTouchpadEnabled, enabled);
}

bool TouchDevicesController::GetTouchscreenEnabled(
    TouchDeviceEnabledSource source) const {
  if (source == TouchDeviceEnabledSource::GLOBAL)
    return global_touchscreen_enabled_;

  PrefService* prefs = GetActivePrefService();
  return prefs && prefs->GetBoolean(prefs::kTouchscreenEnabled);
}

void TouchDevicesController::SetTouchscreenEnabled(
    bool enabled,
    TouchDeviceEnabledSource source) {
  if (source == TouchDeviceEnabledSource::GLOBAL) {
    global_touchscreen_enabled_ = enabled;
    // Explicitly call |UpdateTouchscreenEnabled()| to update the actual
    // touchscreen state from multiple sources.
    UpdateTouchscreenEnabled();
    return;
  }

  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kTouchscreenEnabled, enabled);
}

void TouchDevicesController::OnUserSessionAdded(const AccountId& account_id) {
  uma_record_callback_ = base::BindOnce([](PrefService* prefs) {
    UMA_HISTOGRAM_BOOLEAN("Touchpad.TapDragging.Started",
                          prefs->GetBoolean(prefs::kTapDraggingEnabled));
  });
}

void TouchDevicesController::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  ObservePrefs(prefs);
}

void TouchDevicesController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  if (uma_record_callback_)
    std::move(uma_record_callback_).Run(prefs);
  ObservePrefs(prefs);
}

void TouchDevicesController::ObservePrefs(PrefService* prefs) {
  // Watch for pref updates.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kTapDraggingEnabled,
      base::BindRepeating(&TouchDevicesController::UpdateTapDraggingEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kTouchpadEnabled,
      base::BindRepeating(&TouchDevicesController::UpdateTouchpadEnabled,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kTouchscreenEnabled,
      base::BindRepeating(&TouchDevicesController::UpdateTouchscreenEnabled,
                          base::Unretained(this)));
  // Load current state.
  UpdateTapDraggingEnabled();
  UpdateTouchpadEnabled();
  UpdateTouchscreenEnabled();
}

void TouchDevicesController::UpdateTapDraggingEnabled() {
  PrefService* prefs = GetActivePrefService();
  const bool enabled = prefs->GetBoolean(prefs::kTapDraggingEnabled);

  if (tap_dragging_enabled_ == enabled)
    return;

  tap_dragging_enabled_ = enabled;

  UMA_HISTOGRAM_BOOLEAN("Touchpad.TapDragging.Changed", enabled);

  ui::OzonePlatform::GetInstance()->GetInputController()->SetTapDragging(
      enabled);
}

void TouchDevicesController::UpdateTouchpadEnabled() {
  bool enabled = GetTouchpadEnabled(TouchDeviceEnabledSource::GLOBAL) &&
                 GetTouchpadEnabled(TouchDeviceEnabledSource::USER_PREF);
  ui::InputController* input_controller =
      ui::OzonePlatform::GetInstance()->GetInputController();
  const bool old_value = input_controller->IsInternalTouchpadEnabled();
  input_controller->SetInternalTouchpadEnabled(enabled);
  if (old_value == input_controller->IsInternalTouchpadEnabled())
    return;  // Value didn't actually change.

  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  if (!cursor_manager)
    return;

  if (enabled)
    cursor_manager->ShowCursor();
  else
    cursor_manager->HideCursor();
}

void TouchDevicesController::UpdateTouchscreenEnabled() {
  ui::OzonePlatform::GetInstance()
      ->GetInputController()
      ->SetTouchscreensEnabled(
          GetTouchscreenEnabled(TouchDeviceEnabledSource::GLOBAL) &&
          GetTouchscreenEnabled(TouchDeviceEnabledSource::USER_PREF));
}

}  // namespace ash
