// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_PREFS_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_PREFS_CONTROLLER_LACROS_H_

#include "base/values.h"
#include "chrome/browser/chromeos/mahi/mahi_prefs_controller.h"
#include "chromeos/lacros/crosapi_pref_observer.h"

namespace mahi {

// A lacros implementation of `MahiPrefsController`.
class MahiPrefsControllerLacros : public MahiPrefsController {
 public:
  MahiPrefsControllerLacros();

  MahiPrefsControllerLacros(const MahiPrefsControllerLacros&) = delete;
  MahiPrefsControllerLacros& operator=(const MahiPrefsControllerLacros&) =
      delete;

  ~MahiPrefsControllerLacros() override;

 private:
  // MahiPrefsController:
  void SetMahiEnabled(bool enabled) override;

  // Called when the related preferences are obtained from the pref service.
  void OnMahiEnableStateChanged(base::Value value);

  // Observers to track pref changes from ash.
  std::unique_ptr<CrosapiPrefObserver> mahi_enabled_observer_;
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_PREFS_CONTROLLER_LACROS_H_
