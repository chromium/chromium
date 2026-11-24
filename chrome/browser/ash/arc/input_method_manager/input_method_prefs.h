// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_INPUT_METHOD_PREFS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_INPUT_METHOD_PREFS_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "ui/base/ime/ash/input_method_descriptor.h"

class PrefService;

namespace arc {

// A thin wrapper of the input method related prefs.
class InputMethodPrefs {
 public:
  // `prefs` is the user's PrefService. It must be non-null and must outlive
  // `this`.
  explicit InputMethodPrefs(PrefService* prefs);
  ~InputMethodPrefs();

  InputMethodPrefs(const InputMethodPrefs& pref) = delete;
  InputMethodPrefs& operator=(const InputMethodPrefs& pref) = delete;

  // Updates input method related prefs according to the passed enabled ARC IME
  // list.
  void UpdateEnabledImes(
      ash::input_method::InputMethodDescriptors enabled_arc_imes);

  // Removes `ime_id` from the enabled IMEs on the pref. It's no-op if the IME
  // is not enabled.
  void DisableIme(const std::string& ime_id);

  // Returns the list of IMEs on the pref.
  std::set<std::string> GetEnabledImes() const;

 private:
  void SetEnabledImes(const std::vector<std::string>& enabled_ime_list);

  const raw_ref<PrefService> prefs_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_METHOD_MANAGER_INPUT_METHOD_PREFS_H_
