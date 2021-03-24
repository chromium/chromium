// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_INPUT_METHOD_PREFS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_INPUT_METHOD_PREFS_H_

#include <string>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "ui/base/ime/chromeos/input_method_descriptor.h"

namespace arc {

// A thin wrapper of the input method related prefs.
class InputMethodPrefs {
 public:
  explicit InputMethodPrefs(Profile* profile);
  ~InputMethodPrefs();

  InputMethodPrefs(const InputMethodPrefs& pref) = delete;
  InputMethodPrefs& operator=(const InputMethodPrefs& pref) = delete;

  // Updates input method related prefs according to the passed enabled ARC IME
  // list.
  void UpdateEnabledImes(
      chromeos::input_method::InputMethodDescriptors enabled_arc_imes);

  // Returns the list of IMEs on the pref.
  std::set<std::string> GetEnabledImes() const;

 private:
  Profile* const profile_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_INPUT_METHOD_PREFS_H_
