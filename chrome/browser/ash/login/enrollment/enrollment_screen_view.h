// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_VIEW_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_VIEW_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chromeos/dbus/authpolicy/active_directory_info.pb.h"

class GoogleServiceAuthError;

namespace policy {
struct EnrollmentConfig;
class EnrollmentStatus;
}  // namespace policy

namespace chromeos {

// Interface class for the enterprise enrollment screen view.
class EnrollmentScreenView {
 public:
  // This defines the interface for controllers which will be called back when
  // something happens on the UI.
  class Controller {
   public:
    virtual ~Controller() {}

    virtual void OnLoginDone(const std::string& user,
                             const std::string& auth_code) = 0;
    virtual void OnRetry() = 0;
    virtual void OnCancel() = 0;
    virtual void OnConfirmationClosed() = 0;
    virtual void OnActiveDirectoryCredsProvided(
        const std::string& machine_name,
        const std::string& distinguished_name,
        int encryption_types,
        const std::string& username,
        const std::string& password) = 0;

    virtual void OnDeviceAttributeProvided(const std::string& asset_id,
                                           const std::string& location) = 0;
  };

  constexpr static StaticOobeScreenId kScreenId{"enterprise-enrollment"};

  virtual ~EnrollmentScreenView() {}

  // Initializes the view with parameters.
  virtual void SetEnrollmentConfig(Controller* controller,
                                   const policy::EnrollmentConfig& config) = 0;

  // Sets the enterprise manager and the device type to be shown for the user.
  virtual void SetEnterpriseDomainInfo(const std::string& manager,
                                       const std::u16string& device_type) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Shows the signin screen.
  virtual void ShowSigninScreen() = 0;

  // Shows the Active Directory domain joining screen.
  virtual void ShowActiveDirectoryScreen(const std::string& domain_join_config,
                                         const std::string& machine_name,
                                         const std::string& username,
                                         authpolicy::ErrorType error) = 0;

  // Shows the device attribute prompt screen.
  virtual void ShowAttributePromptScreen(const std::string& asset_id,
                                         const std::string& location) = 0;

  // Shows the success screen
  virtual void ShowEnrollmentSuccessScreen() = 0;

  // Shows the spinner screen for enrollment.
  virtual void ShowEnrollmentSpinnerScreen() = 0;

  // Show an authentication error.
  virtual void ShowAuthError(const GoogleServiceAuthError& error) = 0;

  // Show non-authentication error.
  virtual void ShowOtherError(EnterpriseEnrollmentHelper::OtherError error) = 0;

  // Update the UI to report the `status` of the enrollment procedure.
  virtual void ShowEnrollmentStatus(policy::EnrollmentStatus status) = 0;

  virtual void Shutdown() = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_VIEW_H_
