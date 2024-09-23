// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/platform_verification_flow.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chrome/browser/ash/attestation/certificate_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/attestation/attestation_flow_adaptive.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/attestation.pb.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "media/base/media_switches.h"

namespace ash::attestation {

namespace {

const int kTimeoutInSeconds = 8;
const char kAttestationResultHistogram[] =
    "ChromeOS.PlatformVerification.Result2";
constexpr base::TimeDelta kOpportunisticRenewalThreshold = base::Days(30);

// A helper to call a ChallengeCallback with an error result.
void ReportError(PlatformVerificationFlow::ChallengeCallback callback,
                 PlatformVerificationFlow::Result error) {
  UMA_HISTOGRAM_ENUMERATION(kAttestationResultHistogram, error,
                            PlatformVerificationFlow::RESULT_MAX);
  std::move(callback).Run(error, std::string(), std::string(), std::string());
}

std::string GetKeyName(std::string_view request_origin) {
  return base::StrCat(
      {ash::attestation::kContentProtectionKeyPrefix, request_origin});
}

}  // namespace

// A default implementation of the Delegate interface.
class DefaultDelegate : public PlatformVerificationFlow::Delegate {
 public:
  DefaultDelegate() {}

  DefaultDelegate(const DefaultDelegate&) = delete;
  DefaultDelegate& operator=(const DefaultDelegate&) = delete;

  ~DefaultDelegate() override {}

  bool IsInSupportedMode() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    return !command_line->HasSwitch(chromeos::switches::kSystemDevMode) ||
           command_line->HasSwitch(::switches::kAllowRAInDevMode);
  }
};

PlatformVerificationFlow::ChallengeContext::ChallengeContext(
    const AccountId& account_id,
    const std::string& service_id,
    const std::string& challenge,
    ChallengeCallback callback)
    : account_id(account_id),
      service_id(service_id),
      challenge(challenge),
      callback(std::move(callback)) {}

PlatformVerificationFlow::ChallengeContext::ChallengeContext(
    ChallengeContext&& other) = default;

PlatformVerificationFlow::ChallengeContext::~ChallengeContext() = default;

PlatformVerificationFlow::PlatformVerificationFlow()
    : attestation_flow_(nullptr),
      attestation_client_(AttestationClient::Get()),
      delegate_(nullptr),
      timeout_delay_(base::Seconds(kTimeoutInSeconds)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<ServerProxy> attestation_ca_client(new AttestationCAClient());
  default_attestation_flow_ = std::make_unique<AttestationFlowAdaptive>(
      std::move(attestation_ca_client));
  attestation_flow_ = default_attestation_flow_.get();
  default_delegate_ = std::make_unique<DefaultDelegate>();
  delegate_ = default_delegate_.get();
}

PlatformVerificationFlow::PlatformVerificationFlow(
    AttestationFlow* attestation_flow,
    AttestationClient* attestation_client,
    Delegate* delegate)
    : attestation_flow_(attestation_flow),
      attestation_client_(attestation_client),
      delegate_(delegate),
      timeout_delay_(base::Seconds(kTimeoutInSeconds)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!delegate_) {
    default_delegate_ = std::make_unique<DefaultDelegate>();
    delegate_ = default_delegate_.get();
  }
}

PlatformVerificationFlow::~PlatformVerificationFlow() = default;

// static
bool PlatformVerificationFlow::IsAttestationAllowedByPolicy() {
  // Check the device policy for the feature.
  bool enabled_for_device = false;
  if (!CrosSettings::Get()->GetBoolean(kAttestationForContentProtectionEnabled,
                                       &enabled_for_device)) {
    LOG(ERROR) << "Failed to get device setting.";
    return false;
  }
  if (!enabled_for_device) {
    VLOG(1) << "Platform verification denied because Verified Access is "
            << "disabled for the device.";
    return false;
  }

  return true;
}

void PlatformVerificationFlow::ChallengePlatformKey(
    content::WebContents* web_contents,
    const std::string& service_id,
    const std::string& challenge,
    ChallengeCallback callback) {
  const user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  ChallengePlatformKey(user, service_id, challenge, std::move(callback));
}

void PlatformVerificationFlow::ChallengePlatformKey(
    const user_manager::User* user,
    const std::string& service_id,
    const std::string& challenge,
    ChallengeCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Note: The following checks are performed when use of the protected media
  // identifier is indicated. The first two in GetPermissionStatus and the third
  // in DecidePermission.
  // In Chrome, the result of the first and third could have changed in the
  // interim, but the mode cannot change.
  // TODO(ddorwin): Share more code for the first two checks with
  // ProtectedMediaIdentifierPermissionContext::
  // IsProtectedMediaIdentifierEnabled().

  if (!IsAttestationAllowedByPolicy()) {
    VLOG(1) << "Platform verification not allowed by device policy.";
    ReportError(std::move(callback), POLICY_REJECTED);
    return;
  }

  if (!delegate_->IsInSupportedMode()) {
    LOG(ERROR) << "Platform verification not supported in the current mode.";
    ReportError(std::move(callback), PLATFORM_NOT_VERIFIED);
    return;
  }

  if (!user) {
    LOG(ERROR) << "Profile does not map to a valid user.";
    ReportError(std::move(callback), INTERNAL_ERROR);
    return;
  }

  ChallengeContext context(user->GetAccountId(), service_id, challenge,
                           std::move(callback));

  // Check if the device has been prepared to use attestation.
  ::attestation::GetEnrollmentPreparationsRequest request;
  attestation_client_->GetEnrollmentPreparations(
      request, base::BindOnce(&PlatformVerificationFlow::OnAttestationPrepared,
                              this, std::move(context)));
}

void PlatformVerificationFlow::OnAttestationPrepared(
    ChallengeContext context,
    const ::attestation::GetEnrollmentPreparationsReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR)
        << "Platform verification failed to check if attestation is prepared.";
    ReportError(std::move(context).callback, INTERNAL_ERROR);
    return;
  }
  const bool attestation_prepared =
      AttestationClient::IsAttestationPrepared(reply);

  if (!attestation_prepared) {
    // This device is not currently able to use attestation features.
    ReportError(std::move(context).callback, PLATFORM_NOT_VERIFIED);
    return;
  }

  auto shared_context =
      base::MakeRefCounted<base::RefCountedData<ChallengeContext>>(
          std::move(context));
  GetCertificate(std::move(shared_context), false /* Don't force a new key */);
}

void PlatformVerificationFlow::GetCertificate(
    scoped_refptr<base::RefCountedData<ChallengeContext>> context,
    bool force_new_key) {
  auto timer = std::make_unique<base::OneShotTimer>();
  base::OnceClosure timeout_callback = base::BindOnce(
      &PlatformVerificationFlow::OnCertificateTimeout, this, context);
  timer->Start(FROM_HERE, timeout_delay_, std::move(timeout_callback));

  const std::string key_name =
      GetKeyName(/*request_origin=*/context->data.service_id);
  AttestationFlow::CertificateCallback certificate_callback =
      base::BindOnce(&PlatformVerificationFlow::OnCertificateReady, this,
                     context, context->data.account_id, std::move(timer));
  attestation_flow_->GetCertificate(
      /*certificate_profile=*/PROFILE_CONTENT_PROTECTION_CERTIFICATE,
      /*account_id=*/context->data.account_id,
      /*request_origin=*/context->data.service_id,
      /*force_new_key=*/force_new_key,
      /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/key_name, /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(certificate_callback));
}

void PlatformVerificationFlow::OnCertificateReady(
    scoped_refptr<base::RefCountedData<ChallengeContext>> context,
    const AccountId& account_id,
    std::unique_ptr<base::OneShotTimer> timer,
    AttestationStatus operation_status,
    const std::string& certificate_chain) {
  // Log failure before checking the timer so all failures are logged, even if
  // they took too long.
  if (operation_status != ATTESTATION_SUCCESS) {
    LOG(WARNING) << "PlatformVerificationFlow: Failed to certify platform.";
  }
  if (!timer->IsRunning()) {
    LOG(WARNING) << "PlatformVerificationFlow: Certificate ready but call has "
                 << "already timed out.";
    return;
  }
  timer->Stop();
  if (operation_status != ATTESTATION_SUCCESS) {
    ReportError(std::move(*context).data.callback, PLATFORM_NOT_VERIFIED);
    return;
  }
  // EXPIRY_STATUS_INVALID_PEM_CHAIN and EXPIRY_STATUS_INVALID_X509 are not
  // handled intentionally.
  // Renewal is expensive so we only renew certificates with good evidence that
  // they have expired or will soon expire; if we don't know, we don't renew.
  ExpiryStatus expiry_status = CheckExpiry(certificate_chain);
  if (expiry_status == EXPIRY_STATUS_EXPIRED) {
    GetCertificate(std::move(context), true /* Force a new key */);
    return;
  }
  bool is_expiring_soon = (expiry_status == EXPIRY_STATUS_EXPIRING_SOON);
  std::string key_name = kContentProtectionKeyPrefix + context->data.service_id;
  std::string challenge = context->data.challenge;
  ::attestation::SignSimpleChallengeRequest request;
  request.set_username(cryptohome::Identification(account_id).id());
  request.set_key_label(std::move(key_name));
  request.set_challenge(std::move(challenge));
  AttestationClient::Get()->SignSimpleChallenge(
      request, base::BindOnce(&PlatformVerificationFlow::OnChallengeReady, this,
                              std::move(*context).data, account_id,
                              certificate_chain, is_expiring_soon));
}

void PlatformVerificationFlow::OnCertificateTimeout(
    scoped_refptr<base::RefCountedData<ChallengeContext>> context) {
  LOG(WARNING) << "PlatformVerificationFlow: Timing out.";
  ReportError(std::move(*context).data.callback, TIMEOUT);
}

void PlatformVerificationFlow::OnChallengeReady(
    ChallengeContext context,
    const AccountId& account_id,
    const std::string& certificate_chain,
    bool is_expiring_soon,
    const ::attestation::SignSimpleChallengeReply& reply) {
  if (reply.status() != ::attestation::STATUS_SUCCESS) {
    LOG(ERROR) << "PlatformVerificationFlow: Failed to sign challenge: "
               << reply.status();
    ReportError(std::move(context).callback, INTERNAL_ERROR);
    return;
  }
  SignedData signed_data_pb;
  if (reply.challenge_response().empty() ||
      !signed_data_pb.ParseFromString(reply.challenge_response())) {
    LOG(ERROR) << "PlatformVerificationFlow: Failed to parse response data.";
    ReportError(std::move(context).callback, INTERNAL_ERROR);
    return;
  }
  VLOG(1) << "Platform verification successful.";
  UMA_HISTOGRAM_ENUMERATION(kAttestationResultHistogram, SUCCESS, RESULT_MAX);
  std::move(context.callback)
      .Run(SUCCESS, signed_data_pb.data(), signed_data_pb.signature(),
           certificate_chain);
  if (is_expiring_soon && renewals_in_progress_.count(certificate_chain) == 0) {
    renewals_in_progress_.insert(certificate_chain);
    // Fire off a certificate request so next time we'll have a new one.
    const std::string key_name =
        GetKeyName(/*request_origin=*/context.service_id);
    AttestationFlow::CertificateCallback renew_callback =
        base::BindOnce(&PlatformVerificationFlow::RenewCertificateCallback,
                       this, std::move(certificate_chain));
    attestation_flow_->GetCertificate(
        /*certificate_profile=*/PROFILE_CONTENT_PROTECTION_CERTIFICATE,
        /*account_id=*/context.account_id,
        /*request_origin=*/context.service_id,
        /*force_new_key=*/true,  // force_new_key
        /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
        /*key_name=*/key_name,
        /*profile_specific_data=*/std::nullopt,
        /*callback=*/std::move(renew_callback));
  }
}

PlatformVerificationFlow::ExpiryStatus PlatformVerificationFlow::CheckExpiry(
    const std::string& certificate_chain) {
  CertificateExpiryStatus cert_status =
      CheckCertificateExpiry(certificate_chain, kOpportunisticRenewalThreshold);
  LOG_IF(ERROR, cert_status != CertificateExpiryStatus::kValid)
      << "Failed to parse certificate, cannot check expiry: "
      << CertificateExpiryStatusToString(cert_status);
  switch (cert_status) {
    case CertificateExpiryStatus::kValid:
      return EXPIRY_STATUS_OK;
    case CertificateExpiryStatus::kExpiringSoon:
      return EXPIRY_STATUS_EXPIRING_SOON;
    case CertificateExpiryStatus::kExpired:
      return EXPIRY_STATUS_EXPIRED;
    case CertificateExpiryStatus::kInvalidPemChain:
      return EXPIRY_STATUS_INVALID_PEM_CHAIN;
    case CertificateExpiryStatus::kInvalidX509:
      return EXPIRY_STATUS_INVALID_X509;
  }

  NOTREACHED_IN_MIGRATION() << "Unknown certificate status";
}

void PlatformVerificationFlow::RenewCertificateCallback(
    const std::string& old_certificate_chain,
    AttestationStatus operation_status,
    const std::string& certificate_chain) {
  renewals_in_progress_.erase(old_certificate_chain);
  if (operation_status != ATTESTATION_SUCCESS) {
    LOG(WARNING) << "PlatformVerificationFlow: Failed to renew platform "
                    "certificate.";
    return;
  }
  VLOG(1) << "Certificate successfully renewed.";
}

}  // namespace ash::attestation
