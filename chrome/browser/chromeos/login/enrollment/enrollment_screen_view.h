// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_VIEW_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chromeos/dbus/auth_policy/active_directory_info.pb.h"

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
    virtual void OnLicenseTypeSelected(const std::string& license_type) = 0;
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

  constexpr static StaticOobeScreenId kScreenId{"oauth-enrollment"};

  virtual ~EnrollmentScreenView() {}

  // Initializes the view with parameters.
  virtual void SetEnrollmentConfig(Controller* controller,
                                   const policy::EnrollmentConfig& config) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Shows the signin screen.
  virtual void ShowSigninScreen() = 0;

  // Shows the license type selection screen.
  virtual void ShowLicenseTypeSelectionScreen(
      const base::DictionaryValue& license_types) = 0;

  // Shows the Active Directory domain joining screen.
  virtual void ShowActiveDirectoryScreen(const std::string& domain_join_config,
                                         const std::string& machine_name,
                                         const std::string& username,
                                         authpolicy::ErrorType error) = 0;

  // Shows the device attribute prompt screen.
  virtual void ShowAttributePromptScreen(const std::string& asset_id,
                                         const std::string& location) = 0;

  // Shows a success string for attestation-based enrollment.
  virtual void ShowAttestationBasedEnrollmentSuccessScreen(
      const std::string& enterprise_domain) = 0;

  // Shows the spinner screen for enrollment.
  virtual void ShowEnrollmentSpinnerScreen() = 0;

  // Show an authentication error.
  virtual void ShowAuthError(const GoogleServiceAuthError& error) = 0;

  // Show non-authentication error.
  virtual void ShowOtherError(EnterpriseEnrollmentHelper::OtherError error) = 0;

  // Update the UI to report the |status| of the enrollment procedure.
  virtual void ShowEnrollmentStatus(policy::EnrollmentStatus status) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_VIEW_H_
