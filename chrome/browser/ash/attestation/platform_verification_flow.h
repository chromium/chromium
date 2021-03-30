// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_PLATFORM_VERIFICATION_FLOW_H_
#define CHROME_BROWSER_ASH_ATTESTATION_PLATFORM_VERIFICATION_FLOW_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
// TODO(https://crbug.com/1164001): forward declare AttestatoinFlow
// after //chromeos/attestation is moved to ash.
#include "chromeos/attestation/attestation_flow.h"
// TODO(https://crbug.com/1164001): forward declare AttestatoinClient
// before ChromeOS source migration.
#include "chromeos/dbus/attestation/attestation_client.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "url/gurl.h"

class AccountId;

namespace content {
class WebContents;
}  // namespace content

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {
namespace attestation {

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
// certified so the next time ChallegePlatformKey() is invoked it will be quick.
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
    USER_REJECTED,          // The user explicitly rejected the operation.
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

    // Gets the URL associated with the given |web_contents|.
    virtual const GURL& GetURL(content::WebContents* web_contents) = 0;

    // Gets the user associated with the given |web_contents|.  NULL may be
    // returned.
    virtual const user_manager::User* GetUser(
        content::WebContents* web_contents) = 0;

    // Checks whether attestation is permitted by user.
    virtual bool IsPermittedByUser(content::WebContents* web_contents) = 0;

    // Returns true iff the device is in a mode that supports platform
    // verification. For example, platform verification is not supported in
    // guest or incognito mode. It is also not supported in dev mode unless
    // overridden by a flag.
    virtual bool IsInSupportedMode(content::WebContents* web_contents) = 0;
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

  void set_timeout_delay(const base::TimeDelta& timeout_delay) {
    timeout_delay_ = timeout_delay;
  }

 private:
  friend class base::RefCountedThreadSafe<PlatformVerificationFlow>;
  friend class PlatformVerificationFlowTest;

  // Holds the arguments of a ChallengePlatformKey call.  This is convenient for
  // use with base::Bind so we don't get too many arguments.
  struct ChallengeContext {
    ChallengeContext(content::WebContents* web_contents,
                     const std::string& service_id,
                     const std::string& challenge,
                     ChallengeCallback callback);
    ChallengeContext(ChallengeContext&& other);
    ~ChallengeContext();

    content::WebContents* web_contents;
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
  // ChallengePlatformKey are in |context|.  |account_id| identifies the user
  // for which to get a certificate.  If |force_new_key| is true then any
  // existing key for the same user and service will be ignored and a new key
  // will be generated and certified.
  void GetCertificate(
      scoped_refptr<base::RefCountedData<ChallengeContext>> context,
      const AccountId& account_id,
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

  // Checks whether attestation for content protection is allowed by policy.
  bool IsAttestationAllowedByPolicy();

  // Checks if |certificate_chain| is a PEM certificate chain that contains a
  // certificate this is expired or expiring soon. Returns the expiry status.
  ExpiryStatus CheckExpiry(const std::string& certificate_chain);

  // An AttestationFlow::CertificateCallback that handles renewal completion.
  // |old_certificate_chain| contains the chain that has been replaced.
  void RenewCertificateCallback(const std::string& old_certificate_chain,
                                AttestationStatus operation_status,
                                const std::string& certificate_chain);

  AttestationFlow* attestation_flow_;
  std::unique_ptr<AttestationFlow> default_attestation_flow_;
  AttestationClient* const attestation_client_;
  Delegate* delegate_;
  std::unique_ptr<Delegate> default_delegate_;
  base::TimeDelta timeout_delay_;
  std::set<std::string> renewals_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(PlatformVerificationFlow);
};

}  // namespace attestation
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ATTESTATION_PLATFORM_VERIFICATION_FLOW_H_
