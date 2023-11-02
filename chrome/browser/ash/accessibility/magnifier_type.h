// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_MAGNIFIER_TYPE_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_MAGNIFIER_TYPE_H_

namespace ash {

// Note: Do not change these values; UMA and prefs depend on them.
enum class MagnifierType {
  kDisabled = 0,  // Used by enterprise policy.
  kFull = 1,
  kDocked = 2,
  // Never shipped. Deprioritized in 2013. http://crbug.com/170850
  // kPartial = 2,
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_MAGNIFIER_TYPE_H_
