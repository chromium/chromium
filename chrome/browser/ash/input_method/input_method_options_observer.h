// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_OPTIONS_OBSERVER_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_OPTIONS_OBSERVER_H_

#include <string>

#include "base/functional/callback.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace ash::input_method {

// Used to observe any changes to the values found in the input method specific
// options page. The callback given in the ctor will be invoked whenever there
// is a change to an input method options preference.
class InputMethodOptionsObserver {
 public:
  using OnInputMethodOptionsChanged =
      base::RepeatingCallback<void(const std::string&)>;

  // PrefService must outlive the lifetime of this instance.
  explicit InputMethodOptionsObserver(PrefService* prefs);

  // Start observing for any changes in the input method options page.
  void Observe(OnInputMethodOptionsChanged callback);

 private:
  // Used to listen for changes on a slice of pref values.
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_OPTIONS_OBSERVER_H_
