// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_INPUT_METHOD_PREFS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_INPUT_METHOD_PREFS_H_

#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/base/ime/ash/input_method_descriptor.h"

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
      ash::input_method::InputMethodDescriptors enabled_arc_imes);

  // Returns the list of IMEs on the pref.
  std::set<std::string> GetEnabledImes() const;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_INPUT_METHOD_PREFS_H_
