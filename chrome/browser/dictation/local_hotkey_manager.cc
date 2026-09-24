// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/local_hotkey_manager.h"

#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/command.h"

namespace dictation {

ui::Accelerator GetDictationHotkeyFromPrefs(Profile* profile) {
  CHECK(profile);

  const std::string accelerator_str =
      profile->GetPrefs()->GetString(prefs::kVoiceTypingHotkey);
  if (accelerator_str.empty()) {
    return ui::Accelerator();
  }

  const ui::Accelerator accelerator =
      ui::Command::StringToAccelerator(accelerator_str);

  // StringToAccelerator() returns an empty accelerator when the pref can't be
  // parsed, and it already requires parsed hotkeys to have a modifier. This is
  // defense in depth: never register a modifier-less hotkey, which would
  // swallow ordinary keystrokes.
  if (ui::Accelerator::MaskOutKeyEventFlags(accelerator.modifiers()) == 0) {
    return ui::Accelerator();
  }

  return accelerator;
}

LocalHotkeyManager::LocalHotkeyManager(
    Profile* profile,
    std::unique_ptr<RegistrationDelegate> registration_delegate)
    : profile_(profile),
      registration_delegate_(std::move(registration_delegate)) {
  CHECK(profile_);
  CHECK(registration_delegate_);

  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kVoiceTypingHotkey,
      base::BindRepeating(&LocalHotkeyManager::OnHotkeyPrefChanged,
                          base::Unretained(this)));

  UpdateRegistration();
}

LocalHotkeyManager::~LocalHotkeyManager() = default;

bool LocalHotkeyManager::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DictationKeyedService* service = DictationKeyedService::Get(profile_);
  CHECK(service);
  service->ToggleHotkeyHandler();
  return true;
}

bool LocalHotkeyManager::CanHandleAccelerators() const {
  return true;
}

void LocalHotkeyManager::OnHotkeyPrefChanged() {
  UpdateRegistration();
}

void LocalHotkeyManager::UpdateRegistration() {
  hotkey_registration_.reset();

  const ui::Accelerator accelerator = GetDictationHotkeyFromPrefs(profile_);
  if (accelerator.IsEmpty()) {
    return;
  }

  hotkey_registration_ = registration_delegate_->CreateScopedHotkeyRegistration(
      profile_, accelerator, *this);
}

}  // namespace dictation
