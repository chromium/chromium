// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_options_observer.h"

#include "ash/constants/ash_pref_names.h"

namespace ash::input_method {

InputMethodOptionsObserver::InputMethodOptionsObserver(PrefService* prefs) {
  pref_change_registrar_.Init(prefs);
}

void InputMethodOptionsObserver::Observe(
    InputMethodOptionsObserver::OnInputMethodOptionsChanged callback) {
  pref_change_registrar_.Add(ash::prefs::kLanguageInputMethodSpecificSettings,
                             std::move(callback));
}

}  // namespace ash::input_method
