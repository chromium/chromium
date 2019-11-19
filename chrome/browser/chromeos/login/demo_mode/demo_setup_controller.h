// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

class PrefRegistrySimple;

namespace policy {
class DeviceCloudPolicyManagerChromeOS;
}

namespace chromeos {

class DemoResources;

// Controlls enrollment flow for setting up Demo Mode.
class DemoSetupController
    : public EnterpriseEnrollmentHelper::EnrollmentStatusConsumer,
      public policy::CloudPolicyStore::Observer {
 public:
  // Contains information related to setup error.
  class DemoSetupError {
   public:
    // Type of setup error.
    enum class ErrorCode {
      // Cannot load or parse offline policy.
      kOfflinePolicyError,
      // Local account policy store error.
      kOfflinePolicyStoreError,
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
        EnterpriseEnrollmentHelper::OtherError error);

    static DemoSetupError CreateFromComponentError(
        component_updater::CrOSComponentManager::Error error);

    DemoSetupError(ErrorCode error_code, RecoveryMethod recovery_method);
    DemoSetupError(ErrorCode error_code,
                   RecoveryMethod recovery_method,
                   const std::string& debug_message);
    ~DemoSetupError();

    ErrorCode error_code() const { return error_code_; }
    RecoveryMethod recovery_method() const { return recovery_method_; }

    base::string16 GetLocalizedErrorMessage() const;
    base::string16 GetLocalizedRecoveryMessage() const;
    std::string GetDebugDescription() const;

   private:
    ErrorCode error_code_;
    RecoveryMethod recovery_method_;
    std::string debug_message_;
  };

  // Demo mode setup callbacks.
  using OnSetupSuccess = base::OnceClosure;
  using OnSetupError = base::OnceCallback<void(const DemoSetupError&)>;
  using HasPreinstalledDemoResourcesCallback = base::OnceCallback<void(bool)>;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Clears demo device enrollment requisition on the given |policy_manager| if
  // it is set.
  static void ClearDemoRequisition(
      policy::DeviceCloudPolicyManagerChromeOS* policy_manager);

  // Utility method that returns whether demo mode is allowed on the device.
  static bool IsDemoModeAllowed();

  // Utility method that returns whether demo mode setup flow is in progress in
  // OOBE.
  static bool IsOobeDemoSetupFlowInProgress();

  // If the current country requires customization, returns an user email that
  // corresponds to the sub organization the device should be enrolled into.
  // Otherwise, returns an empty string.
  static std::string GetSubOrganizationEmail();

  DemoSetupController();
  ~DemoSetupController() override;

  // Sets demo mode config that will be used to setup the device. It has to be
  // set before calling Enroll().
  void set_demo_config(DemoSession::DemoModeConfig demo_config) {
    demo_config_ = demo_config;
  }

  // Whether offline enrollment is used for setup.
  bool IsOfflineEnrollment() const;

  // Initiates enrollment that sets up the device in the demo mode domain. The
  // |enrollment_type_| determines whether online or offline setup will be
  // performed and it should be set with set_enrollment_type() before calling
  // Enroll(). |on_setup_success| will be called when enrollment finishes
  // successfully. |on_setup_error| will be called when enrollment finishes with
  // an error.
  void Enroll(OnSetupSuccess on_setup_success, OnSetupError on_setup_error);

  // Tries to mount the preinstalled offline resources necessary for offline
  // Demo Mode.
  void TryMountPreinstalledDemoResources(
      HasPreinstalledDemoResourcesCallback callback);

  // Converts a relative path to an absolute path under the preinstalled demo
  // resources mount. Returns an empty string if the preinstalled demo resources
  // are not mounted.
  base::FilePath GetPreinstalledDemoResourcesPath(
      const base::FilePath& relative_path);

  // EnterpriseEnrollmentHelper::EnrollmentStatusConsumer:
  void OnDeviceEnrolled() override;
  void OnEnrollmentError(policy::EnrollmentStatus status) override;
  void OnAuthError(const GoogleServiceAuthError& error) override;
  void OnOtherError(EnterpriseEnrollmentHelper::OtherError error) override;
  void OnDeviceAttributeUploadCompleted(bool success) override;
  void OnDeviceAttributeUpdatePermission(bool granted) override;
  void OnMultipleLicensesAvailable(
      const EnrollmentLicenseMap& licenses) override;
  void OnRestoreAfterRollbackCompleted() override;

  void SetCrOSComponentLoadErrorForTest(
      component_updater::CrOSComponentManager::Error error);
  void SetPreinstalledOfflineResourcesPathForTesting(
      const base::FilePath& path);
  void SetDeviceLocalAccountPolicyStoreForTest(policy::CloudPolicyStore* store);
  void SetOfflineDataDirForTest(const base::FilePath& offline_dir);

 private:
  // Attempts to load the CrOS component with demo resources for online
  // enrollment and passes the result to OnDemoResourcesCrOSComponentLoaded().
  void LoadDemoResourcesCrOSComponent();

  // Callback to initiate online enrollment once the CrOS component has loaded.
  // If the component loaded successfully, registers and sets up the device in
  // the demo mode domain. If the component couldn't be loaded, demo setup
  // will fail.
  void OnDemoResourcesCrOSComponentLoaded();

  // Callback after attempting to load preinstalled demo resources. If the
  // resources were loaded, offline Demo Mode should be available.
  void OnPreinstalledDemoResourcesLoaded(
      HasPreinstalledDemoResourcesCallback callback);

  // Initiates offline enrollment that locks the device and sets up offline
  // policies required by demo mode. It requires no network connectivity since
  // all setup will be done locally. The policy files will be loaded from the
  // preinstalled demo resources.
  void EnrollOffline();

  // Called when the device local account policy for the offline demo mode is
  // loaded.
  void OnDeviceLocalAccountPolicyLoaded(base::Optional<std::string> blob);

  // Called when device is marked as registered and the second part of OOBE flow
  // is completed. This is the last step of demo mode setup flow.
  void OnDeviceRegistered();

  // Finish the flow with an error.
  void SetupFailed(const DemoSetupError& error);

  // Clears the internal state.
  void Reset();

  // policy::CloudPolicyStore::Observer:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

  // Demo mode configuration type that will be setup when Enroll() is called.
  // Should be set explicitly.
  DemoSession::DemoModeConfig demo_config_ = DemoSession::DemoModeConfig::kNone;

  // Error code to use when attempting to load the demo resources CrOS
  // component.
  component_updater::CrOSComponentManager::Error component_error_for_tests_ =
      component_updater::CrOSComponentManager::Error::NONE;

  // Path at which to mount preinstalled offline demo resources for tests.
  base::FilePath preinstalled_offline_resources_path_for_tests_;

  // Callback to call when enrollment finishes with an error.
  OnSetupError on_setup_error_;

  // Callback to call when enrollment finishes successfully.
  OnSetupSuccess on_setup_success_;

  // The CloudPolicyStore for the device local account for the offline policy.
  policy::CloudPolicyStore* device_local_account_policy_store_ = nullptr;

  std::unique_ptr<EnterpriseEnrollmentHelper> enrollment_helper_;

  // The preinstalled Demo Mode Resources for offline Demo Mode.
  std::unique_ptr<DemoResources> preinstalled_demo_resources_;

  // The Demo Mode Resources CrOS Component downloaded for online Demo Mode.
  std::unique_ptr<DemoResources> demo_resources_;

  base::WeakPtrFactory<DemoSetupController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DemoSetupController);
};

}  //  namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_SETUP_CONTROLLER_H_
