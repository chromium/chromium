// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_PREFS_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_PREFS_CONTROLLER_H_

namespace mahi {

// A class that controls Mahi related prefs.
class MahiPrefsController {
 public:
  static MahiPrefsController* Get();

  MahiPrefsController();

  MahiPrefsController(const MahiPrefsController&) = delete;
  MahiPrefsController& operator=(const MahiPrefsController&) = delete;

  virtual ~MahiPrefsController();

  // Sets/gets the enable state of Mahi.
  virtual void SetMahiEnabled(bool enabled) = 0;
  bool GetMahiEnabled();
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_PREFS_CONTROLLER_H_
