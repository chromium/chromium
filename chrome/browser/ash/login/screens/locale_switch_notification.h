// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_LOCALE_SWITCH_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_LOCALE_SWITCH_NOTIFICATION_H_

#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/ash/base/locale_util.h"

class Profile;

namespace ash {

// Utility class to display locale switch notification.
class LocaleSwitchNotification {
 public:
  // Show locale switch notification.
  static void Show(Profile* profile,
                   std::string new_locale,
                   locale_util::SwitchLanguageCallback locale_switch_callback);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_LOCALE_SWITCH_NOTIFICATION_H_
