// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/accelerators.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "media/base/media_switches.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

AcceleratorController* g_instance = nullptr;

base::RepeatingClosure* GetVolumeAdjustmentCallback() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return callback.get();
}

}  // namespace

// static
AcceleratorController* AcceleratorController::Get() {
  return g_instance;
}

// static
void AcceleratorController::SetVolumeAdjustmentSoundCallback(
    const base::RepeatingClosure& closure) {
  DCHECK(GetVolumeAdjustmentCallback()->is_null() || closure.is_null());
  *GetVolumeAdjustmentCallback() = std::move(closure);
}

// static
void AcceleratorController::PlayVolumeAdjustmentSound() {
  if (*GetVolumeAdjustmentCallback())
    GetVolumeAdjustmentCallback()->Run();
}

// static
bool AcceleratorController::IsSystemKey(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_ASSISTANT:
    case ui::VKEY_ZOOM:               // Fullscreen button.
    case ui::VKEY_MEDIA_LAUNCH_APP1:  // Overview button.
    case ui::VKEY_BRIGHTNESS_DOWN:
    case ui::VKEY_BRIGHTNESS_UP:
    case ui::VKEY_KBD_BRIGHTNESS_DOWN:
    case ui::VKEY_KBD_BRIGHTNESS_UP:
    case ui::VKEY_VOLUME_MUTE:
    case ui::VKEY_VOLUME_DOWN:
    case ui::VKEY_VOLUME_UP:
    case ui::VKEY_POWER:
    case ui::VKEY_SLEEP:
    case ui::VKEY_F13:  // Lock button on some chromebooks emits F13.
    case ui::VKEY_PRIVACY_SCREEN_TOGGLE:
    case ui::VKEY_SETTINGS:
    case ui::VKEY_DO_NOT_DISTURB:
      return true;
    case ui::VKEY_MEDIA_NEXT_TRACK:
    case ui::VKEY_MEDIA_PAUSE:
    case ui::VKEY_MEDIA_PLAY:
    case ui::VKEY_MEDIA_PLAY_PAUSE:
    case ui::VKEY_MEDIA_PREV_TRACK:
    case ui::VKEY_MEDIA_STOP:
    case ui::VKEY_OEM_103:  // KEYCODE_MEDIA_REWIND
    case ui::VKEY_OEM_104:  // KEYCODE_MEDIA_FAST_FORWARD
      return base::FeatureList::IsEnabled(media::kHardwareMediaKeyHandling);
    default:
      return false;
  }
}

void AcceleratorController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AcceleratorController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

AcceleratorController::AcceleratorController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AcceleratorController::~AcceleratorController() {
  for (auto& obs : observers_)
    obs.OnAcceleratorControllerWillBeDestroyed(this);

  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void AcceleratorController::NotifyActionPerformed(AcceleratorAction action) {
  for (Observer& observer : observers_)
    observer.OnActionPerformed(action);
}

}  // namespace ash
