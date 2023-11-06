// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_SCREEN_H_

#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockEnrollmentScreen : public EnrollmentScreen {
 public:
  MockEnrollmentScreen(base::WeakPtr<EnrollmentScreenView> view,
                       ErrorScreen* error_screen,
                       const ScreenExitCallback& exit_callback);
  ~MockEnrollmentScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(Result result);
};

class MockEnrollmentScreenView : public EnrollmentScreenView {
 public:
  MockEnrollmentScreenView();
  ~MockEnrollmentScreenView() override;

  MOCK_METHOD(void,
              SetEnrollmentConfig,
              (const policy::EnrollmentConfig& config));
  MOCK_METHOD(void, SetEnrollmentController, (Controller*));
  MOCK_METHOD(void,
              SetEnterpriseDomainInfo,
              (const std::string& manager, const std::u16string& device_type));
  MOCK_METHOD(void, SetFlowType, (FlowType flow_type));
  MOCK_METHOD(void, SetGaiaButtonsType, (GaiaButtonsType buttons_type));
  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, MockBind, (EnrollmentScreen * screen));
  MOCK_METHOD(void, MockUnbind, ());
  MOCK_METHOD(void, ShowSigninScreen, ());
  MOCK_METHOD(void, ReloadSigninScreen, ());
  MOCK_METHOD(void, ResetEnrollmentScreen, ());
  MOCK_METHOD(void, ShowUserError, (const std::string& email));
  MOCK_METHOD(void, ShowEnrollmentDuringTrialNotAllowedError, ());
  MOCK_METHOD(void, ShowSkipConfirmationDialog, ());
  MOCK_METHOD(void,
              ShowAttributePromptScreen,
              (const std::string& asset_id, const std::string& location));
  MOCK_METHOD(void, ShowEnrollmentSuccessScreen, ());
  MOCK_METHOD(void, ShowEnrollmentTPMCheckingScreen, ());
  MOCK_METHOD(void, ShowEnrollmentWorkingScreen, ());
  MOCK_METHOD(void, ShowAuthError, (const GoogleServiceAuthError&));
  MOCK_METHOD(void, ShowOtherError, (EnrollmentLauncher::OtherError));
  MOCK_METHOD(void, ShowEnrollmentStatus, (policy::EnrollmentStatus status));
  MOCK_METHOD(void, Shutdown, ());

  base::WeakPtr<EnrollmentScreenView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<EnrollmentScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_SCREEN_H_
