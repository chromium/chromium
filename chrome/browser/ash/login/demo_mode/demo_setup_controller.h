// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

class PrefRegistrySimple;

namespace policy {
class EnrollmentStatus;
}

namespace ash {

class DemoComponents;

// Controls enrollment flow for setting up Demo Mode.
class DemoSetupController
    : public EnrollmentLauncher::EnrollmentStatusConsumer {
 public:
  // All steps required for setup.
  enum class DemoSetupStep {
    // Downloading Demo Mode resources.
    kDownloadResources,
    // Enrolling in Demo Mode.
    kEnrollment,
    // Setup is complete.
    kComplete
  };

  // Contains information related to setup error.
  class DemoSetupError {
   public:
    // Type of setup error.
    enum class ErrorCode {
      // Cannot perform offline setup without online FRE check.
      kOnlineFRECheckRequired,
      // Cannot load online component.
      kOnlineComponentError,
      // Invalid request to DMServer.
      kInvalidRequest,
      // Request to DMServer failed, because of network error.
      kRequestNetworkError,
      // DMServer temporary unavailable.
      kTemporaryUnavailable,
      // DMServer returned abnormal response code.
      kResponseError,
      // DMServer response cannot be decoded.
      kResponseDecodingError,
      // Device management not supported for demo account.
      kDemoAccountError,
      // DMServer cannot find the device.
      kDeviceNotFound,
      // Invalid device management token.
      kInvalidDMToken,
      // Serial number invalid or unknown to DMServer,
      kInvalidSerialNumber,
      // Device id conflict.
      kDeviceIdError,
      // Not enough licenses or domain expired.
      kLicenseError,
      // Device was deprovisioned.ec
      kDeviceDeprovisioned,
      // Device belongs to different domain (FRE).
      kDomainMismatch,
      // Management request could not be signed by the client.
      kSigningError,
      // DMServer could not find policy for the device.
      kPolicyNotFound,
      // ARC disabled for demo domain.
      kArcError,
      // Cannot determine server-backed state keys.
      kNoStateKeys,
      // Failed to fetch robot account auth or refresh token.
      kRobotFetchError,
      // Failed to fetch robot account refresh token.
      kRobotStoreError,
      // Unsuppored device mode returned by the server.
      kBadMode,
      // Could not fetch registration cert,
      kCertFetchError,
      // Could not fetch the policy.
      kPolicyFetchError,
      // Policy validation failed.
      kPolicyValidationError,
      // Timeout during locking the device.
      kLockTimeout,
      // Error during locking the device.
      kLockError,
      // Device locked to different domain on mode.
      kAlreadyLocked,
      // Error while installing online policy.
      kOnlineStoreError,
      // Could not determine device model or serial number.
      kMachineIdentificationError,
      // Could not store DM token.
      kDMTokenStoreError,
      // Unexpected/fatal error.
      kUnexpectedError,
      // Too many requests error.
      kTooManyRequestsError,
    };

    // Type of recommended recovery from the setup error.
    enum class RecoveryMethod {
      // Retry demo setup.
      kRetry,
      // Reboot and retry demo setup.
      kReboot,
      // Powerwash and retry demo setup.
      kPowerwash,
      // Check network and retry demo setup.
      kCheckNetwork,
      // Cannot perform offline setup - online setup might work.
      kOnlineOnly,
      // Unknown recovery method.
      kUnknown,
    };

    static DemoSetupError CreateFromEnrollmentStatus(
        const policy::EnrollmentStatus& status);

    static DemoSetupError CreateFromOtherEnrollmentError(
        EnrollmentLauncher::OtherError error);

    static DemoSetupError CreateFromComponentError(
        component_updater::CrOSComponentManager::Error error,
        std::string component_name);

    DemoSetupError(ErrorCode error_code, RecoveryMethod recovery_method);
    DemoSetupError(ErrorCode error_code,
                   RecoveryMethod recovery_method,
                   const std::string& debug_message);
    ~DemoSetupError();

    ErrorCode error_code() const { return error_code_; }
    RecoveryMethod recovery_method() const { return recovery_method_; }

    std::u16string GetLocalizedErrorMessage() const;
    std::u16string GetLocalizedRecoveryMessage() const;
    std::string GetDebugDescription() const;

   private:
    ErrorCode error_code_;
    RecoveryMethod recovery_method_;
    std::string debug_message_;
  };

  // Demo mode setup callbacks.
  using OnSetupSuccess = base::OnceClosure;
  using OnSetupError = base::OnceCallback<void(const DemoSetupError&)>;
  using OnSetCurrentSetupStep =
      base::RepeatingCallback<void(const DemoSetupStep)>;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Clears demo device enrollment requisition if it is set.
  static void ClearDemoRequisition();

  // Utility method that returns whether demo mode is allowed on the device.
  static bool IsDemoModeAllowed();

  // Utility method that returns whether demo mode setup flow is in progress in
  // OOBE.
  static bool IsOobeDemoSetupFlowInProgress();

  // If the current country requires customization, returns an user email that
  // corresponds to the sub organization the device should be enrolled into.
  // If chrome flag "--demo-mode-enrolling-username" is set for test, it
  // will override the current country-derived user. If neither of above is
  // true, returns an empty string.
  static std::string GetSubOrganizationEmail();

  // Returns a dictionary mapping setup steps to step indices.
  static base::Value::Dict GetDemoSetupSteps();

  // Converts a step enum to a string e.g. to sent to JavaScript.
  static std::string GetDemoSetupStepString(const DemoSetupStep step_enum);

  DemoSetupController();

  DemoSetupController(const DemoSetupController&) = delete;
  DemoSetupController& operator=(const DemoSetupController&) = delete;

  ~DemoSetupController() override;

  // Sets demo mode config that will be used to setup the device. It has to be
  // set before calling Enroll().
  void set_demo_config(DemoSession::DemoModeConfig demo_config) {
    demo_config_ = demo_config;
  }

  // Set a canonicalized (whitespace and punctuation removed, case homogenized)
  // version of the retailer name string.
  void SetAndCanonicalizeRetailerName(const std::string& retailer_name);

  std::string get_retailer_name_for_testing() { return retailer_name_; }

  void set_store_number(const std::string& store_number) {
    store_number_ = store_number;
  }

  // Initiates enrollment that sets up the device in the demo mode domain. The
  // `enrollment_type_` determines whether online or offline setup will be
  // performed and it should be set with set_enrollment_type() before calling
  // Enroll(). `on_setup_success` will be called when enrollment finishes
  // successfully. `on_setup_error` will be called when enrollment finishes with
  // an error. `set_current_setup_step` will be called when an enrollment step
  // completes.
  void Enroll(OnSetupSuccess on_setup_success,
              OnSetupError on_setup_error,
              const OnSetCurrentSetupStep& set_current_setup_step);

  // Converts a relative path to an absolute path under the preinstalled demo
  // resources mount. Returns an empty string if the preinstalled demo resources
  // are not mounted.
  base::FilePath GetPreinstalledDemoResourcesPath(
      const base::FilePath& relative_path);

  // EnrollmentLauncher::EnrollmentStatusConsumer:
  void OnDeviceEnrolled() override;
  void OnEnrollmentError(policy::EnrollmentStatus status) override;
  void OnAuthError(const GoogleServiceAuthError& error) override;
  void OnOtherError(EnrollmentLauncher::OtherError error) override;
  void OnDeviceAttributeUploadCompleted(bool success) override;
  void OnDeviceAttributeUpdatePermission(bool granted) override;

  void SetCrOSComponentLoadErrorForTest(
      component_updater::CrOSComponentManager::Error error);

  void EnableLoadRealComponentsForTest();

 private:
  // Attempts to load the demo SWA and demo resources ChromeOS components  for
  // online enrollment and pass the results to OnDemoComponentsLoaded().
  void LoadDemoComponents();

  // Callback to initiate online enrollment once both the demo-mode-resources
  // (sample photos, Android APKs) and demo-mode-app (demo SWA content) ChromeOS
  // components have loaded.
  // If the components loaded successfully, registers and sets up the device in
  // the demo mode domain. If the component couldn't be loaded, demo setup
  // will fail.
  void OnDemoComponentsLoaded();

  // Called when device is marked as registered and the second part of OOBE flow
  // is completed. This is the last step of demo mode setup flow.
  void OnDeviceRegistered();

  // Sets current setup step.
  void SetCurrentSetupStep(DemoSetupStep current_step);

  // Finish the flow with an error.
  void SetupFailed(const DemoSetupError& error);

  // Clears the internal state.
  void Reset();

  // Keeps track of when downloading demo mode resources begins.
  base::TimeTicks download_start_time_;

  // Keeps track of when enrolling in enterprise) begins.
  base::TimeTicks enroll_start_time_;

  // Keeps track of how many times an operator has been required to retry
  // setup.
  int num_setup_retries_ = 0;

  // Name of retailer entered during setup flow. Corresponds to the
  // kDemoModeRetailerId pref.
  std::string retailer_name_;
  // Store number entered during setup flow. Corresponds to the kDemoModeStoreId
  // pref.
  std::string store_number_;

  // Demo mode configuration type that will be setup when Enroll() is called.
  // Should be set explicitly.
  DemoSession::DemoModeConfig demo_config_ = DemoSession::DemoModeConfig::kNone;

  // Error code to use when attempting to load the demo resources CrOS
  // component.
  component_updater::CrOSComponentManager::Error component_error_for_tests_ =
      component_updater::CrOSComponentManager::Error::NONE;

  // Callback to call when setup step is updated.
  OnSetCurrentSetupStep set_current_setup_step_;

  // Callback to call when enrollment finishes with an error.
  OnSetupError on_setup_error_;

  // Callback to call when enrollment finishes successfully.
  OnSetupSuccess on_setup_success_;

  std::unique_ptr<EnrollmentLauncher> enrollment_launcher_;

  // The Demo Mode Resources ChromeOS Component downloaded for online Demo Mode.
  std::unique_ptr<DemoComponents> demo_components_;

  bool load_real_components_for_test_ = false;

  base::WeakPtrFactory<DemoSetupController> weak_ptr_factory_{this};
};

}  //  namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
