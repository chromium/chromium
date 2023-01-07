// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

class GoogleServiceAuthError;

namespace policy {
class ActiveDirectoryJoinDelegate;
struct EnrollmentConfig;
enum class LicenseType;
class EnrollmentStatus;
}  // namespace policy

namespace ash {

// This class is capable to enroll the device into enterprise domain, using
// either a profile containing authentication data or OAuth token.
// It can also clear an authentication data from the profile and revoke tokens
// that are not longer needed.
class EnterpriseEnrollmentHelper {
 public:
  // Enumeration of the possible errors that can occur during enrollment which
  // are not covered by GoogleServiceAuthError or EnrollmentStatus.
  enum OtherError {
    // Existing enrollment domain doesn't match authentication user.
    OTHER_ERROR_DOMAIN_MISMATCH,
    // Unexpected error condition, indicates a bug in the code.
    OTHER_ERROR_FATAL
  };

  class EnrollmentStatusConsumer {
   public:
    virtual ~EnrollmentStatusConsumer() = default;

    // Called when an error happens on attempt to receive authentication tokens.
    virtual void OnAuthError(const GoogleServiceAuthError& error) = 0;

    // Called when an error happens during enrollment.
    virtual void OnEnrollmentError(policy::EnrollmentStatus status) = 0;

    // Called when some other error happens.
    virtual void OnOtherError(OtherError error) = 0;

    // Called when enrollment finishes successfully.
    virtual void OnDeviceEnrolled() = 0;

    // Called when device attribute update permission granted,
    // `granted` indicates whether permission granted or not.
    virtual void OnDeviceAttributeUpdatePermission(bool granted) = 0;

    // Called when device attribute upload finishes. `success` indicates
    // whether it is successful or not.
    virtual void OnDeviceAttributeUploadCompleted(bool success) = 0;
  };

  // Factory method. Caller takes ownership of the returned object.
  static std::unique_ptr<EnterpriseEnrollmentHelper> Create(
      EnrollmentStatusConsumer* status_consumer,
      policy::ActiveDirectoryJoinDelegate* ad_join_delegate,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain,
      policy::LicenseType license_type);

  // Sets up a mock object that would be returned by next Create call.
  // This call passes ownership of `mock`.
  static void SetEnrollmentHelperMock(
      std::unique_ptr<EnterpriseEnrollmentHelper> mock);

  EnterpriseEnrollmentHelper(const EnterpriseEnrollmentHelper&) = delete;
  EnterpriseEnrollmentHelper& operator=(const EnterpriseEnrollmentHelper&) =
      delete;

  virtual ~EnterpriseEnrollmentHelper();

  // Starts enterprise enrollment using `auth_code`. First tries to exchange the
  // auth code to authentication token, then tries to enroll the device with the
  // received token.
  // EnrollUsingAuthCode can be called only once during this object's lifetime,
  // and only if none of the EnrollUsing* methods was called before.
  virtual void EnrollUsingAuthCode(const std::string& auth_code) = 0;

  // Starts enterprise enrollment using `token`.
  // This flow is used when enrollment is controlled by the paired device.
  // EnrollUsingToken can be called only once during this object's lifetime, and
  // only if none of the EnrollUsing* was called before.
  virtual void EnrollUsingToken(const std::string& token) = 0;

  // Starts enterprise enrollment using PCA attestation.
  // EnrollUsingAttestation can be called only once during the object's
  // lifetime, and only if none of the EnrollUsing* was called before.
  virtual void EnrollUsingAttestation() = 0;

  // Starts device attribute update process. First tries to get
  // permission to update device attributes for current user
  // using stored during enrollment oauth token.
  virtual void GetDeviceAttributeUpdatePermission() = 0;

  // Uploads device attributes on DM server. `asset_id` - Asset Identifier
  // and `location` - Assigned Location, these attributes were typed by
  // current user on the device attribute prompt screen after successful
  // enrollment.
  virtual void UpdateDeviceAttributes(const std::string& asset_id,
                                      const std::string& location) = 0;

  // Clears authentication data from the profile (if EnrollUsingProfile was
  // used) and revokes fetched tokens.
  // Does not revoke the additional token if enrollment finished successfully.
  // Calls `callback` on completion.
  virtual void ClearAuth(base::OnceClosure callback) = 0;

  // Returns true if enrollment is in progress.
  virtual bool InProgress() const = 0;

 protected:
  // The user of this class is responsible for clearing auth data in some cases
  // (see comment for EnrollUsingProfile()).
  EnterpriseEnrollmentHelper();

  // This method is called once from Create method.
  virtual void Setup(policy::ActiveDirectoryJoinDelegate* ad_join_delegate,
                     const policy::EnrollmentConfig& enrollment_config,
                     const std::string& enrolling_user_domain,
                     policy::LicenseType license_type) = 0;

  // This method is used in Create method. `status_consumer` must outlive
  // `this`.
  void set_status_consumer(EnrollmentStatusConsumer* status_consumer);

  EnrollmentStatusConsumer* status_consumer() const { return status_consumer_; }

 private:
  EnrollmentStatusConsumer* status_consumer_;

  // If this is not nullptr, then it will be used to as next enrollment helper.
  static EnterpriseEnrollmentHelper* mock_enrollment_helper_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_HELPER_H_
