// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_
#define CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_

#include "chrome/browser/chromeos/app_mode/app_session.h"

namespace ash {

// AppSessionAsh maintains a kiosk session and handles its lifetime.
class AppSessionAsh : public chromeos::AppSession {
 public:
  AppSessionAsh() = default;
  AppSessionAsh(const AppSessionAsh&) = delete;
  AppSessionAsh& operator=(const AppSessionAsh&) = delete;
  ~AppSessionAsh() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_APP_SESSION_ASH_H_
