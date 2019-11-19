// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFIER_TYPE_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFIER_TYPE_H_

namespace chromeos {

// Note: Do not change these values; UMA and prefs depend on them.
enum MagnifierType {
  MAGNIFIER_DISABLED = 0,  // Used by enterprise policy.
  MAGNIFIER_FULL = 1,
  MAGNIFIER_DOCKED = 2,
  // Never shipped. Deprioritized in 2013. http://crbug.com/170850
  // MAGNIFIER_PARTIAL = 2,
  // TODO(afakhy): Consider adding Docked Magnifier type (shipped in M66) for
  // policy control.
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFIER_TYPE_H_
