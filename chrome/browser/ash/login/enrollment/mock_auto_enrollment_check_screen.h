// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_AUTO_ENROLLMENT_CHECK_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_AUTO_ENROLLMENT_CHECK_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_check_screen_view.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockAutoEnrollmentCheckScreen : public AutoEnrollmentCheckScreen {
 public:
  MockAutoEnrollmentCheckScreen(
      base::WeakPtr<AutoEnrollmentCheckScreenView> view,
      ErrorScreen* error_screen,
      const base::RepeatingCallback<void(Result result)>& exit_callback);
  ~MockAutoEnrollmentCheckScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void RealShow();
  void ExitScreen();
};

class MockAutoEnrollmentCheckScreenView : public AutoEnrollmentCheckScreenView {
 public:
  MockAutoEnrollmentCheckScreenView();
  ~MockAutoEnrollmentCheckScreenView() override;

  MOCK_METHOD(void, Show, ());
  base::WeakPtr<AutoEnrollmentCheckScreenView> AsWeakPtr() override;

 private:
  base::WeakPtrFactory<MockAutoEnrollmentCheckScreenView> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_AUTO_ENROLLMENT_CHECK_SCREEN_H_
