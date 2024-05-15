// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOAD_FAILED_OBSERVER_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOAD_FAILED_OBSERVER_H_

#include "base/observer_list_types.h"

namespace ash {

class KioskProfileLoadFailedObserver : public base::CheckedObserver {
 public:
  ~KioskProfileLoadFailedObserver() override = default;

  virtual void OnKioskProfileLoadFailed() = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_PROFILE_LOAD_FAILED_OBSERVER_H_
