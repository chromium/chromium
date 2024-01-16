// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_PREF_CHANGE_RECORDER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_PREF_CHANGE_RECORDER_H_

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/input_method/autocorrect_enums.h"
#include "chrome/browser/ash/input_method/input_method_options_observer.h"
#include "components/prefs/pref_service.h"

namespace ash::input_method {

class PrefChangeRecorder {
 public:
  enum KeyboardType {
    kVirtualKeyboard,
    kPhysicalKeyboard,
  };

  struct AutocorrectPrefDetails {
    std::string engine_id;
    KeyboardType keyboard_type;
    AutocorrectPreference preference;
  };

  using AutocorrectPrefs = base::flat_map<std::string, AutocorrectPrefDetails>;

  // PrefService must outlive the lifetime of this class.
  explicit PrefChangeRecorder(PrefService* pref_service);
  ~PrefChangeRecorder();

 private:
  // This is called whenever a preference is changed in the input method
  // options page. It is used to catch any changes made by the user to the
  // relevant autocorrect level preference.
  void OnInputMethodOptionsChanged(const std::string& pref_path_changed);

  // Listens for any changes made to preferences found in the input method
  // options page.
  InputMethodOptionsObserver input_method_options_observer_;

  // This container holds all of the autocorrect preferences (for both PK and
  // VK) previously captured by this class. It is used to detect what has
  // changed when a user updates their autocorrect preferences in the settings
  // page.
  AutocorrectPrefs autocorrect_prefs_;

  // PrefService* must outlive the lifetime of this instance.
  raw_ptr<PrefService> pref_service_;

  base::WeakPtrFactory<PrefChangeRecorder> weak_ptr_factory_{this};
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_PREF_CHANGE_RECORDER_H_
