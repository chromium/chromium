// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_VIEW_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_VIEW_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"

class GoogleServiceAuthError;

namespace policy {
struct EnrollmentConfig;
class EnrollmentStatus;
}  // namespace policy

namespace ash {

// Interface class for the enterprise enrollment screen view.
class EnrollmentScreenView {
 public:
  // This defines the interface for controllers which will be called back when
  // something happens on the UI.
  class Controller {
   public:
    virtual ~Controller() = default;

    virtual void OnLoginDone(login::OnlineSigninArtifacts signin_artifacts,
                             int license_type,
                             const std::string& auth_code) = 0;
    virtual void OnRetry() = 0;
    virtual void OnCancel() = 0;
    virtual void OnConfirmationClosed() = 0;
    virtual void OnDeviceAttributeProvided(const std::string& asset_id,
                                           const std::string& location) = 0;
    virtual void OnIdentifierEntered(const std::string& email) = 0;
    virtual void OnFirstShow() = 0;
    virtual void OnFrameLoadingCompleted() = 0;
  };

  inline constexpr static StaticOobeScreenId kScreenId{"enterprise-enrollment",
                                                       "OAuthEnrollmentScreen"};

  virtual ~EnrollmentScreenView() = default;

  enum class FlowType {
    kEnterprise,
    kCFM,
    kEnterpriseLicense,
    kEducationLicense,
    kDeviceEnrollment,
  };
  enum class GaiaButtonsType {
    kDefault,
    kEnterprisePreferred,
    kKioskPreferred
  };
  enum class UserErrorType { kConsumerDomain, kBusinessDomain };

  // Initializes the view with parameters.
  virtual void SetEnrollmentConfig(const policy::EnrollmentConfig& config) = 0;
  virtual void SetEnrollmentController(Controller* controller) = 0;

  // Sets the enterprise manager and the device type to be shown for the user.
  virtual void SetEnterpriseDomainInfo(const std::string& manager,
                                       const std::u16string& device_type) = 0;

  // Sets which flow should GAIA show.
  virtual void SetFlowType(FlowType flow_type) = 0;

  virtual void ShowSkipConfirmationDialog() = 0;

  // Sets which buttons should GAIA screen show.
  virtual void SetGaiaButtonsType(GaiaButtonsType buttons_type) = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Shows the signin screen.
  virtual void ShowSigninScreen() = 0;

  // Reloads the signin screen.
  virtual void ReloadSigninScreen() = 0;

  // Resets shown enrollment screen.
  virtual void ResetEnrollmentScreen() = 0;

  // Shows error related to user account eligibility.
  virtual void ShowUserError(const std::string& email) = 0;

  // Shows error that enrollment is not allowed during trial run.
  virtual void ShowEnrollmentDuringTrialNotAllowedError() = 0;

  // Shows the device attribute prompt screen.
  virtual void ShowAttributePromptScreen(const std::string& asset_id,
                                         const std::string& location) = 0;

  // Shows the success screen
  virtual void ShowEnrollmentSuccessScreen() = 0;

  // Shows the working spinner screen for enrollment.
  virtual void ShowEnrollmentWorkingScreen() = 0;

  // Shows the TPM checking spinner screen for enrollment.
  virtual void ShowEnrollmentTPMCheckingScreen() = 0;

  // Show an authentication error.
  virtual void ShowAuthError(const GoogleServiceAuthError& error) = 0;

  // Show non-authentication error.
  virtual void ShowOtherError(EnrollmentLauncher::OtherError error) = 0;

  // Update the UI to report the `status` of the enrollment procedure.
  virtual void ShowEnrollmentStatus(policy::EnrollmentStatus status) = 0;

  virtual void Shutdown() = 0;

  virtual base::WeakPtr<EnrollmentScreenView> AsWeakPtr() = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_VIEW_H_
