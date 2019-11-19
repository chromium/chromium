// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_VIEW_H_

#include "chrome/browser/chromeos/login/oobe_screen.h"

namespace chromeos {

// Interface between auto-enrollment check screen and its representation.
// Note, do not forget to call OnViewDestroyed in the dtor.
class AutoEnrollmentCheckScreenView {
 public:
  // Allows us to get info from auto-enrollment check screen that we need.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // This method is called, when view is being destroyed. Note, if Delegate
    // is destroyed earlier then it has to call SetDelegate(NULL).
    virtual void OnViewDestroyed(AutoEnrollmentCheckScreenView* view) = 0;
  };

  constexpr static StaticOobeScreenId kScreenId{"auto-enrollment-check"};

  virtual ~AutoEnrollmentCheckScreenView() {}

  virtual void Show() = 0;
  virtual void SetDelegate(Delegate* delegate) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_VIEW_H_
