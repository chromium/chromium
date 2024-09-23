// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_LAUNCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"

class GoogleServiceAuthError;

namespace policy {
struct EnrollmentConfig;
class EnrollmentStatus;
}  // namespace policy

namespace ash {

namespace attestation {
class AttestationFlow;
}

// This class is capable to enroll the device into enterprise domain, using
// either a profile containing authentication data or OAuth token.
// It can also clear an authentication data from the profile and revoke tokens
// that are not longer needed.
class EnrollmentLauncher {
 public:
  class EnrollmentStatusConsumer;

  using AttestationFlowFactory = base::RepeatingCallback<
      std::unique_ptr<ash::attestation::AttestationFlow>()>;

  using Factory = base::RepeatingCallback<std::unique_ptr<EnrollmentLauncher>(
      EnrollmentStatusConsumer*,
      const policy::EnrollmentConfig&,
      const std::string&)>;

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
  static std::unique_ptr<EnrollmentLauncher> Create(
      EnrollmentStatusConsumer* status_consumer,
      const policy::EnrollmentConfig& enrollment_config,
      const std::string& enrolling_user_domain);

  EnrollmentLauncher(const EnrollmentLauncher&) = delete;
  EnrollmentLauncher& operator=(const EnrollmentLauncher&) = delete;

  virtual ~EnrollmentLauncher();

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
  //
  // TODO(b/331285209): Rename this method to EnrollUsingOAuthToken, to
  // distinguish from EnrollUsingEnrollmentToken.
  virtual void EnrollUsingToken(const std::string& token) = 0;

  // Starts enterprise enrollment using PCA attestation.
  // EnrollUsingAttestation can be called only once during the object's
  // lifetime, and only if none of the EnrollUsing* was called before.
  virtual void EnrollUsingAttestation() = 0;

  // Starts enterprise enrollment using the enrollment token passed via
  // `EnrollmentConfig`.
  // EnrollUsingEnrollmentToken can be called only once during this object's
  // lifetime, and only if none of the EnrollUsing* was called before.
  virtual void EnrollUsingEnrollmentToken() = 0;

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
  // used) and conditionally revokes fetched tokens.
  // Does not revoke the additional token if enrollment finished successfully.
  // Calls `callback` on completion.
  virtual void ClearAuth(base::OnceClosure callback,
                         bool revoke_oauth2_tokens) = 0;

  // Returns true if enrollment is in progress.
  virtual bool InProgress() const = 0;

  // Returns the OAuth2 Refresh Token fetched during enrollment.
  // Make sure to call this after OnDeviceEnrolled() and before ClearAuth().
  virtual std::string GetOAuth2RefreshToken() const = 0;

 protected:
  // The user of this class is responsible for clearing auth data in some cases
  // (see comment for EnrollUsingProfile()).
  EnrollmentLauncher();

  // This method is called once from Create method.
  virtual void Setup(const policy::EnrollmentConfig& enrollment_config,
                     const std::string& enrolling_user_domain) = 0;
};

class ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting {
 public:
  explicit ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting(
      EnrollmentLauncher::AttestationFlowFactory testing_factory);
  ~ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting();

  ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting(
      const ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting&) =
      delete;
  ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting& operator=(
      const ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting&) =
      delete;
};

// Overrides `EnrollmentLauncher::Create` factory method for the lifetime
// created override.
class ScopedEnrollmentLauncherFactoryOverrideForTesting {
 public:
  // When created, `EnrollmentLauncher::Create` returns objects created by
  // `testing_factory` calls.
  explicit ScopedEnrollmentLauncherFactoryOverrideForTesting(
      EnrollmentLauncher::Factory testing_factory);
  ~ScopedEnrollmentLauncherFactoryOverrideForTesting();

  ScopedEnrollmentLauncherFactoryOverrideForTesting(
      const ScopedEnrollmentLauncherFactoryOverrideForTesting&) = delete;
  ScopedEnrollmentLauncherFactoryOverrideForTesting& operator=(
      const ScopedEnrollmentLauncherFactoryOverrideForTesting&) = delete;

  void Reset(EnrollmentLauncher::Factory testing_factory);

  ScopedEnrollmentLauncherFactoryOverrideForTesting& operator=(
      EnrollmentLauncher::Factory testing_factory);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_ENROLLMENT_LAUNCHER_H_
