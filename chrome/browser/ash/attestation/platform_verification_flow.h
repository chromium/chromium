// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_PLATFORM_VERIFICATION_FLOW_H_
#define CHROME_BROWSER_ASH_ATTESTATION_PLATFORM_VERIFICATION_FLOW_H_

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/account_id/account_id.h"
#include "url/gurl.h"

class AccountId;

namespace content {
class WebContents;
}  // namespace content

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {

class AttestationClient;

namespace attestation {

class AttestationFlow;
class PlatformVerificationFlowTest;

// This class allows platform verification for the content protection use case.
// All methods must only be called on the UI thread.  Example:
//   scoped_refptr<PlatformVerificationFlow> verifier =
//       new PlatformVerificationFlow();
//   PlatformVerificationFlow::ChallengeCallback callback =
//       base::BindOnce(&MyCallback);
//   verifier->ChallengePlatformKey(my_web_contents, "my_id", "some_challenge",
//                                  std::move(callback));
//
// This class is RefCountedThreadSafe because it may need to outlive its caller.
// The attestation flow that needs to happen to establish a certified platform
// key may take minutes on some hardware.  This class will timeout after a much
// shorter time so the caller can proceed without platform verification but it
// is important that the pending operation be allowed to finish.  If the
// attestation flow is aborted at any stage, it will need to start over.  If we
// use weak pointers, the attestation flow will stop when the next callback is
// run.  So we need the instance to stay alive until the platform key is fully
// certified so the next time ChallengePlatformKey() is invoked it will be
// quick.
class PlatformVerificationFlow
    : public base::RefCountedThreadSafe<PlatformVerificationFlow> {
 public:
  // These values are reported to UMA. DO NOT CHANGE THE EXISTING VALUES!
  enum Result {
    SUCCESS,                // The operation succeeded.
    INTERNAL_ERROR,         // The operation failed unexpectedly.
    PLATFORM_NOT_VERIFIED,  // The platform cannot be verified.  For example:
                            // - It is not a Chrome device.
                            // - It is not running a verified OS image.
    POLICY_REJECTED,        // The operation is not allowed by policy/settings.
    TIMEOUT,                // The operation timed out.
    RESULT_MAX
  };

  // These values are reported to UMA. DO NOT CHANGE THE EXISTING VALUES!
  enum ExpiryStatus {
    EXPIRY_STATUS_OK,
    EXPIRY_STATUS_EXPIRING_SOON,
    EXPIRY_STATUS_EXPIRED,
    EXPIRY_STATUS_INVALID_PEM_CHAIN,
    EXPIRY_STATUS_INVALID_X509,
    EXPIRY_STATUS_MAX
  };

  // An interface which allows settings and UI to be abstracted for testing
  // purposes.  For normal operation the default implementation should be used.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Returns true iff the device is in a mode that supports platform
    // verification. For example, platform verification is not supported in dev
    // mode unless overridden by a flag.
    virtual bool IsInSupportedMode() = 0;
  };

  // This callback will be called when a challenge operation completes.  If
  // |result| is SUCCESS then |signed_data| holds the data which was signed
  // by the platform key (this is the original challenge appended with a random
  // nonce) and |signature| holds the RSA-PKCS1-v1.5 signature.  The
  // |platform_key_certificate| certifies the key used to generate the
  // signature.  This key may be generated on demand and is not guaranteed to
  // persist across multiple calls to this method.  The browser does not check
  // the validity of |signature| or |platform_key_certificate|.
  using ChallengeCallback =
      base::OnceCallback<void(Result result,
                              const std::string& signed_data,
                              const std::string& signature,
                              const std::string& platform_key_certificate)>;

  // A constructor that uses the default implementation of all dependencies
  // including Delegate.
  PlatformVerificationFlow();

  // An alternate constructor which specifies dependent objects explicitly.
  // This is useful in testing.  The caller retains ownership of all pointers.
  PlatformVerificationFlow(AttestationFlow* attestation_flow,
                           AttestationClient* attestation_client,
                           Delegate* delegate);

  PlatformVerificationFlow(const PlatformVerificationFlow&) = delete;
  PlatformVerificationFlow& operator=(const PlatformVerificationFlow&) = delete;

  // Invokes an asynchronous operation to challenge a platform key.  Any user
  // interaction will be associated with |web_contents|.  The |service_id| is an
  // arbitrary value but it should uniquely identify the origin of the request
  // and should not be determined by that origin; its purpose is to prevent
  // collusion between multiple services.  The |challenge| is also an arbitrary
  // value but it should be time sensitive or associated to some kind of session
  // because its purpose is to prevent certificate replay.  The |callback| will
  // be called when the operation completes.  The duration of the operation can
  // vary depending on system state, hardware capabilities, and interaction with
  // the user.
  void ChallengePlatformKey(content::WebContents* web_contents,
                            const std::string& service_id,
                            const std::string& challenge,
                            ChallengeCallback callback);

  // Identical to ChallengePlatformKey above except the User has been extracted
  // from the input |web_contents|. The former is needed since non-Ash callsites
  // of this class cannot directly reference User*.
  void ChallengePlatformKey(const user_manager::User* user,
                            const std::string& service_id,
                            const std::string& challenge,
                            ChallengeCallback callback);

  void set_timeout_delay(const base::TimeDelta& timeout_delay) {
    timeout_delay_ = timeout_delay;
  }

  // Public for tests.
  static bool IsAttestationAllowedByPolicy();

 private:
  friend class base::RefCountedThreadSafe<PlatformVerificationFlow>;
  friend class PlatformVerificationFlowTest;

  // Holds the arguments of a ChallengePlatformKey call.  This is convenient for
  // use with base::Bind so we don't get too many arguments.
  struct ChallengeContext {
    ChallengeContext(const AccountId& account_id,
                     const std::string& service_id,
                     const std::string& challenge,
                     ChallengeCallback callback);
    ChallengeContext(ChallengeContext&& other);
    ~ChallengeContext();

    AccountId account_id;
    std::string service_id;
    std::string challenge;
    ChallengeCallback callback;
  };

  ~PlatformVerificationFlow();

  // Callback for attestation preparation. The arguments to ChallengePlatformKey
  // are in |context|, and |reply| is the result of |GetEnrollmentPreparations|.
  void OnAttestationPrepared(
      ChallengeContext context,
      const ::attestation::GetEnrollmentPreparationsReply& reply);

  // Initiates the flow to get a platform key certificate.  The arguments to
  // ChallengePlatformKey are in |context|.  If |force_new_key| is true then any
  // existing key for the same user and service will be ignored and a new key
  // will be generated and certified.
  void GetCertificate(
      scoped_refptr<base::RefCountedData<ChallengeContext>> context,
      bool force_new_key);

  // A callback called when an attestation certificate request operation
  // completes.  The arguments to ChallengePlatformKey are in |context|.
  // |account_id| identifies the user for which the certificate was requested.
  // |operation_success| is true iff the certificate request operation
  // succeeded.  |certificate_chain| holds the certificate for the platform
  // key on success.  If the certificate request was successful, this method
  // invokes a request to sign the challenge.  If the operation timed out
  // prior to this method being called, this method does nothing - notably,
  // the callback is not invoked.
  void OnCertificateReady(
      scoped_refptr<base::RefCountedData<ChallengeContext>> context,
      const AccountId& account_id,
      std::unique_ptr<base::OneShotTimer> timer,
      AttestationStatus operation_status,
      const std::string& certificate_chain);

  // A callback run after a constant delay to handle timeouts for lengthy
  // certificate requests.  |context.callback| will be invoked with a TIMEOUT
  // result.
  void OnCertificateTimeout(
      scoped_refptr<base::RefCountedData<ChallengeContext>> context);

  // A callback called when a challenge signing request has completed.  The
  // `certificate_chain` is the platform certificate chain for the key which
  // signed the `challenge`.  The arguments to ChallengePlatformKey are in
  // `context`. `account_id` identifies the user for which the certificate was
  // requested. `is_expiring_soon` will be set iff a certificate in the
  // `certificate_chain` is expiring soon. `reply` is returned from
  // `AttestationClient`. Upon success, the method will invoke
  // `context.callback`.
  void OnChallengeReady(ChallengeContext context,
                        const AccountId& account_id,
                        const std::string& certificate_chain,
                        bool is_expiring_soon,
                        const ::attestation::SignSimpleChallengeReply& reply);

  // Checks if |certificate_chain| is a PEM certificate chain that contains a
  // certificate this is expired or expiring soon. Returns the expiry status.
  ExpiryStatus CheckExpiry(const std::string& certificate_chain);

  // An AttestationFlow::CertificateCallback that handles renewal completion.
  // |old_certificate_chain| contains the chain that has been replaced.
  void RenewCertificateCallback(const std::string& old_certificate_chain,
                                AttestationStatus operation_status,
                                const std::string& certificate_chain);

  raw_ptr<AttestationFlow> attestation_flow_;
  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  const raw_ptr<AttestationClient, DanglingUntriaged> attestation_client_;
  raw_ptr<Delegate> delegate_;
  std::unique_ptr<Delegate> default_delegate_;
  base::TimeDelta timeout_delay_;
  std::set<std::string> renewals_in_progress_;
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_PLATFORM_VERIFICATION_FLOW_H_
