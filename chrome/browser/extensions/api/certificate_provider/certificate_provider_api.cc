// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/certificate_provider/certificate_provider_api.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/certificate_provider/pin_dialog_manager.h"
#include "chrome/browser/certificate_provider/security_token_pin_dialog_host.h"
#include "chrome/common/extensions/api/certificate_provider.h"
#include "chrome/common/extensions/api/certificate_provider_internal.h"
#include "chromeos/components/security_token_pin/constants.h"
#include "extensions/browser/quota_service.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace {

namespace api_cp = ::extensions::api::certificate_provider;
namespace api_cpi = ::extensions::api::certificate_provider_internal;
using PinCodeType = ::chromeos::security_token_pin::CodeType;
using PinErrorLabel = ::chromeos::security_token_pin::ErrorLabel;
using RequestPinResult = ::chromeos::PinDialogManager::RequestPinResult;
using StopPinRequestResult = ::chromeos::PinDialogManager::StopPinRequestResult;

PinErrorLabel GetErrorLabelForDialog(api_cp::PinRequestErrorType error_type) {
  switch (error_type) {
    case api_cp::PinRequestErrorType::kInvalidPin:
      return PinErrorLabel::kInvalidPin;
    case api_cp::PinRequestErrorType::kInvalidPuk:
      return PinErrorLabel::kInvalidPuk;
    case api_cp::PinRequestErrorType::kMaxAttemptsExceeded:
      return PinErrorLabel::kMaxAttemptsExceeded;
    case api_cp::PinRequestErrorType::kUnknownError:
      return PinErrorLabel::kUnknown;
    case api_cp::PinRequestErrorType::kNone:
      return PinErrorLabel::kNone;
  }

  NOTREACHED_IN_MIGRATION();
  return PinErrorLabel::kNone;
}

}  // namespace

namespace extensions {

namespace {

const char kCertificateProviderErrorEmptyChain[] =
    "Certificate chain is empty.";
const char kCertificateProviderErrorChainTooLong[] =
    "Certificate chain should contain exactly one item.";
const char kCertificateProviderErrorInvalidX509Cert[] =
    "Certificate is not a valid X.509 certificate.";
const char kCertificateProviderErrorECDSANotSupported[] =
    "Key type ECDSA not supported.";
const char kCertificateProviderErrorUnknownKeyType[] = "Key type unknown.";
const char kCertificateProviderErrorAborted[] = "Request was aborted.";
const char kCertificateProviderErrorTimeout[] =
    "Request timed out, reply rejected.";
const char kCertificateProviderErrorInvalidId[] = "Invalid requestId";
const char kCertificateProviderErrorUnexpectedError[] =
    "Error supplied with non-empty data.";
const char kCertificateProviderErrorNeitherResultNorError[] =
    "Neither the result nor an error supplied.";
const char kCertificateProviderErrorNoAlgorithms[] = "Algorithm list is empty.";

// requestPin constants.
const char kCertificateProviderNoActiveDialog[] =
    "No active dialog from extension.";
const char kCertificateProviderInvalidSignId[] = "Invalid signRequestId";
const char kCertificateProviderInvalidAttemptsLeft[] = "Invalid attemptsLeft";
const char kCertificateProviderOtherFlowInProgress[] = "Other flow in progress";
const char kCertificateProviderPreviousDialogActive[] =
    "Previous request not finished";
const char kCertificateProviderNoUserInput[] = "No user input received";

// The BucketMapper implementation for the requestPin API that avoids using the
// quota when the current request uses the requestId that is strictly greater
// than all previous ones.
class RequestPinExceptFirstQuotaBucketMapper final
    : public QuotaLimitHeuristic::BucketMapper {
 public:
  RequestPinExceptFirstQuotaBucketMapper() = default;
  RequestPinExceptFirstQuotaBucketMapper(
      const RequestPinExceptFirstQuotaBucketMapper&) = delete;
  RequestPinExceptFirstQuotaBucketMapper& operator=(
      const RequestPinExceptFirstQuotaBucketMapper&) = delete;
  ~RequestPinExceptFirstQuotaBucketMapper() override = default;

  void GetBucketsForArgs(const base::Value::List& args,
                         QuotaLimitHeuristic::BucketList* buckets) override {
    if (args.empty())
      return;
    const base::Value::Dict* details = args.front().GetIfDict();
    if (!details)
      return;
    std::optional<int> sign_request_id = details->FindInt("signRequestId");
    if (!sign_request_id.has_value())
      return;
    if (*sign_request_id > biggest_request_id_) {
      // Either it's the first request with the newly issued requestId, or it's
      // an invalid requestId (bigger than the real one). Return a new bucket in
      // order to apply no quota for the former case; for the latter case the
      // quota doesn't matter much, except that we're maybe making it stricter
      // for future requests (which is bearable).
      biggest_request_id_ = *sign_request_id;
      new_request_bucket_ = std::make_unique<QuotaLimitHeuristic::Bucket>();
      buckets->push_back(new_request_bucket_.get());
      return;
    }
    // Either it's a repeatitive request for the given requestId, or the
    // extension reordered the requests. Fall back to the default bucket (shared
    // between all requests) in that case.
    buckets->push_back(&default_bucket_);
  }

 private:
  int biggest_request_id_ = -1;
  QuotaLimitHeuristic::Bucket default_bucket_;
  std::unique_ptr<QuotaLimitHeuristic::Bucket> new_request_bucket_;
};

scoped_refptr<net::X509Certificate> ParseCertificateDer(
    const std::vector<uint8_t>& cert_der,
    std::string* out_error_message) {
  if (cert_der.empty()) {
    *out_error_message = kCertificateProviderErrorInvalidX509Cert;
    return nullptr;
  }

  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323 and crbug.com/788655.
  net::X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  scoped_refptr<net::X509Certificate> certificate =
      net::X509Certificate::CreateFromBytesUnsafeOptions(cert_der, options);
  if (!certificate) {
    *out_error_message = kCertificateProviderErrorInvalidX509Cert;
    return nullptr;
  }

  size_t public_key_length_in_bits = 0;
  net::X509Certificate::PublicKeyType type =
      net::X509Certificate::kPublicKeyTypeUnknown;
  net::X509Certificate::GetPublicKeyInfo(certificate->cert_buffer(),
                                         &public_key_length_in_bits, &type);

  switch (type) {
    case net::X509Certificate::kPublicKeyTypeRSA:
      break;
    case net::X509Certificate::kPublicKeyTypeECDSA:
      *out_error_message = kCertificateProviderErrorECDSANotSupported;
      return nullptr;
    case net::X509Certificate::kPublicKeyTypeUnknown:
      *out_error_message = kCertificateProviderErrorUnknownKeyType;
      return nullptr;
  }
  return certificate;
}

bool ParseCertificateInfo(
    const api_cp::CertificateInfo& info,
    chromeos::certificate_provider::CertificateInfo* out_info,
    std::string* out_error_message) {
  out_info->certificate =
      ParseCertificateDer(info.certificate, out_error_message);
  if (!out_info->certificate)
    return false;

  out_info->supported_algorithms.reserve(info.supported_hashes.size());
  for (const api_cp::Hash hash : info.supported_hashes) {
    switch (hash) {
      case api_cp::Hash::kMd5Sha1:
        // Ignore `HASH_MD5_SHA1`. This is only used in TLS 1.0 and 1.1, which
        // we no longer support.
        break;
      case api_cp::Hash::kSha1:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA1);
        break;
      case api_cp::Hash::kSha256:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA256);
        break;
      case api_cp::Hash::kSha384:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA384);
        break;
      case api_cp::Hash::kSha512:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA512);
        break;
      case api_cp::Hash::kNone:
        NOTREACHED_IN_MIGRATION();
        return false;
    }
  }
  if (out_info->supported_algorithms.empty()) {
    *out_error_message = kCertificateProviderErrorNoAlgorithms;
    return false;
  }
  return true;
}

bool ParseClientCertificateInfo(
    const api_cp::ClientCertificateInfo& info,
    chromeos::certificate_provider::CertificateInfo* out_info,
    std::string* out_error_message) {
  if (info.certificate_chain.empty()) {
    *out_error_message = kCertificateProviderErrorEmptyChain;
    return false;
  }
  if (info.certificate_chain.size() > 1) {
    // TODO(crbug.com/40703788): Support passing certificate chains.
    *out_error_message = kCertificateProviderErrorChainTooLong;
    return false;
  }
  out_info->certificate =
      ParseCertificateDer(info.certificate_chain[0], out_error_message);
  if (!out_info->certificate)
    return false;

  out_info->supported_algorithms.reserve(info.supported_algorithms.size());
  for (const api_cp::Algorithm algorithm : info.supported_algorithms) {
    switch (algorithm) {
      case api_cp::Algorithm::kRsassaPkcs1V1_5Md5Sha1:
        // Ignore `ALGORITHM_RSASSA_PKCS1_V1_5_MD5_SHA1`. This is only used in
        // TLS 1.0 and 1.1, which we no longer support.
        break;
      case api_cp::Algorithm::kRsassaPkcs1V1_5Sha1:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA1);
        break;
      case api_cp::Algorithm::kRsassaPkcs1V1_5Sha256:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA256);
        break;
      case api_cp::Algorithm::kRsassaPkcs1V1_5Sha384:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA384);
        break;
      case api_cp::Algorithm::kRsassaPkcs1V1_5Sha512:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA512);
        break;
      case api_cp::Algorithm::kRsassaPssSha256:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PSS_RSAE_SHA256);
        break;
      case api_cp::Algorithm::kRsassaPssSha384:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PSS_RSAE_SHA384);
        break;
      case api_cp::Algorithm::kRsassaPssSha512:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PSS_RSAE_SHA512);
        break;
      case api_cp::Algorithm::kNone:
        NOTREACHED_IN_MIGRATION();
        return false;
    }
  }
  if (out_info->supported_algorithms.empty()) {
    *out_error_message = kCertificateProviderErrorNoAlgorithms;
    return false;
  }
  return true;
}

}  // namespace

const int api::certificate_provider::kMaxClosedDialogsPerMinute = 10;
const int api::certificate_provider::kMaxClosedDialogsPer10Minutes = 30;

CertificateProviderInternalReportCertificatesFunction::
    ~CertificateProviderInternalReportCertificatesFunction() {}

ExtensionFunction::ResponseAction
CertificateProviderInternalReportCertificatesFunction::Run() {
  std::optional<api_cpi::ReportCertificates::Params> params =
      api_cpi::ReportCertificates::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  if (!params->certificates) {
    // In the public API, the certificates parameter is mandatory. We only run
    // into this case, if the custom binding rejected the reply by the
    // extension.
    return RespondNow(Error(kCertificateProviderErrorAborted));
  }

  chromeos::certificate_provider::CertificateInfoList cert_infos;
  std::vector<std::vector<uint8_t>> rejected_certificates;
  for (const api_cp::CertificateInfo& input_cert_info : *params->certificates) {
    chromeos::certificate_provider::CertificateInfo parsed_cert_info;
    std::string error_message;
    if (ParseCertificateInfo(input_cert_info, &parsed_cert_info,
                             &error_message)) {
      cert_infos.push_back(parsed_cert_info);
    } else {
      rejected_certificates.push_back(input_cert_info.certificate);
      WriteToConsole(blink::mojom::ConsoleMessageLevel::kError, error_message);
    }
  }

  // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
  LOG(WARNING) << "Certificates provided by extension " << extension()->id()
               << ": " << cert_infos.size() << ", rejected "
               << rejected_certificates.size();

  service->SetCertificatesProvidedByExtension(extension_id(), cert_infos);

  if (service->SetExtensionCertificateReplyReceived(extension_id(),
                                                    params->request_id))
    return RespondNow(ArgumentList(
        api_cpi::ReportCertificates::Results::Create(rejected_certificates)));

  // The custom binding already checks for multiple reports to the same
  // request. The only remaining case, why this reply can fail is that the
  // request timed out.
  return RespondNow(Error(kCertificateProviderErrorTimeout));
}

CertificateProviderStopPinRequestFunction::
    ~CertificateProviderStopPinRequestFunction() = default;

ExtensionFunction::ResponseAction
CertificateProviderStopPinRequestFunction::Run() {
  std::optional<api_cp::StopPinRequest::Params> params =
      api_cp::StopPinRequest::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
  LOG(WARNING) << "Handling PIN stop request from extension "
               << extension()->id() << " error "
               << api_cp::ToString(params->details.error_type);

  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);
  if (params->details.error_type == api_cp::PinRequestErrorType::kNone) {
    bool dialog_closed =
        service->pin_dialog_manager()->CloseDialog(extension_id());
    if (!dialog_closed) {
      // This might happen if the user closed the dialog while extension was
      // processing the input.
      // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
      LOG(WARNING) << "PIN stop request failed: "
                   << kCertificateProviderNoActiveDialog;
      return RespondNow(Error(kCertificateProviderNoActiveDialog));
    }

    // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
    LOG(WARNING) << "PIN stop request succeeded";
    return RespondNow(NoArguments());
  }

  // Extension provided an error, which means it intends to notify the user with
  // the error and not allow any more input.
  const PinErrorLabel error_label =
      GetErrorLabelForDialog(params->details.error_type);
  const StopPinRequestResult stop_request_result =
      service->pin_dialog_manager()->StopPinRequestWithError(
          extension()->id(), error_label,
          base::BindOnce(
              &CertificateProviderStopPinRequestFunction::OnPinRequestStopped,
              this));
  std::string error_result;
  switch (stop_request_result) {
    case StopPinRequestResult::kNoActiveDialog:
      error_result = kCertificateProviderNoActiveDialog;
      break;
    case StopPinRequestResult::kNoUserInput:
      error_result = kCertificateProviderNoUserInput;
      break;
    case StopPinRequestResult::kSuccess:
      return RespondLater();
  }
  // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
  LOG(WARNING) << "PIN stop request failed: " << error_result;
  return RespondNow(Error(std::move(error_result)));
}

void CertificateProviderStopPinRequestFunction::OnPinRequestStopped() {
  // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
  LOG(WARNING) << "PIN stop request succeeded";
  Respond(NoArguments());
}

CertificateProviderRequestPinFunction::
    ~CertificateProviderRequestPinFunction() {}

bool CertificateProviderRequestPinFunction::ShouldSkipQuotaLimiting() const {
  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  return !service->pin_dialog_manager()->LastPinDialogClosed(extension_id());
}

void CertificateProviderRequestPinFunction::GetQuotaLimitHeuristics(
    QuotaLimitHeuristics* heuristics) const {
  // Apply a 1-minute and a 10-minute quotas. A special bucket mapper is used in
  // order to, approximately, skip applying quotas to the first request for each
  // requestId (such logic cannot be done in ShouldSkipQuotaLimiting(), since
  // it's not called with the request's parameters). The limitation constants
  // are decremented below to account the first request.

  QuotaLimitHeuristic::Config short_limit_config = {
      api::certificate_provider::kMaxClosedDialogsPerMinute - 1,
      base::Minutes(1)};
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      short_limit_config,
      std::make_unique<RequestPinExceptFirstQuotaBucketMapper>(),
      "MAX_PIN_DIALOGS_CLOSED_PER_MINUTE"));

  QuotaLimitHeuristic::Config long_limit_config = {
      api::certificate_provider::kMaxClosedDialogsPer10Minutes - 1,
      base::Minutes(10)};
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      long_limit_config,
      std::make_unique<RequestPinExceptFirstQuotaBucketMapper>(),
      "MAX_PIN_DIALOGS_CLOSED_PER_10_MINUTES"));
}

ExtensionFunction::ResponseAction CertificateProviderRequestPinFunction::Run() {
  std::optional<api_cp::RequestPin::Params> params =
      api_cp::RequestPin::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const api_cp::PinRequestType pin_request_type =
      params->details.request_type == api_cp::PinRequestType::kNone
          ? api_cp::PinRequestType::kPin
          : params->details.request_type;

  const PinErrorLabel error_label =
      GetErrorLabelForDialog(params->details.error_type);

  const PinCodeType code_type =
      (pin_request_type == api_cp::PinRequestType::kPin) ? PinCodeType::kPin
                                                         : PinCodeType::kPuk;

  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  int attempts_left = -1;
  if (params->details.attempts_left) {
    if (*params->details.attempts_left < 0)
      return RespondNow(Error(kCertificateProviderInvalidAttemptsLeft));
    attempts_left = *params->details.attempts_left;
  }

  // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
  LOG(WARNING) << "Starting PIN request from extension " << extension()->id()
               << " signRequestId " << params->details.sign_request_id
               << " type " << api_cp::ToString(params->details.request_type)
               << " error " << api_cp::ToString(params->details.error_type)
               << " attempts " << attempts_left;

  const RequestPinResult result = service->pin_dialog_manager()->RequestPin(
      extension()->id(), extension()->name(), params->details.sign_request_id,
      code_type, error_label, attempts_left,
      base::BindOnce(&CertificateProviderRequestPinFunction::OnInputReceived,
                     this));
  std::string error_result;
  switch (result) {
    case RequestPinResult::kSuccess:
      return RespondLater();
    case RequestPinResult::kInvalidId:
      error_result = kCertificateProviderInvalidSignId;
      break;
    case RequestPinResult::kOtherFlowInProgress:
      error_result = kCertificateProviderOtherFlowInProgress;
      break;
    case RequestPinResult::kDialogDisplayedAlready:
      error_result = kCertificateProviderPreviousDialogActive;
      break;
  }
  // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
  LOG(WARNING) << "PIN request failed: " << error_result;
  return RespondNow(Error(std::move(error_result)));
}

void CertificateProviderRequestPinFunction::OnInputReceived(
    const std::string& value) {
  base::Value::List create_results;
  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);
  if (!value.empty()) {
    // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
    LOG(WARNING) << "PIN request succeeded";
    api::certificate_provider::PinResponseDetails details;
    details.user_input = value;
    create_results.Append(details.ToValue());
  } else {
    // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
    LOG(WARNING) << "PIN request canceled";
  }

  Respond(ArgumentList(std::move(create_results)));
}

CertificateProviderSetCertificatesFunction::
    ~CertificateProviderSetCertificatesFunction() = default;

ExtensionFunction::ResponseAction
CertificateProviderSetCertificatesFunction::Run() {
  std::optional<api_cp::SetCertificates::Params> params =
      api_cp::SetCertificates::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->details.client_certificates.empty() &&
      params->details.error != api_cp::Error::kNone) {
    return RespondNow(Error(kCertificateProviderErrorUnexpectedError));
  }

  chromeos::certificate_provider::CertificateInfoList accepted_certificates;
  uint32_t rejected_certificates_count = 0;
  for (const api_cp::ClientCertificateInfo& input_cert_info :
       params->details.client_certificates) {
    chromeos::certificate_provider::CertificateInfo parsed_cert_info;
    std::string parsing_error_message;
    if (ParseClientCertificateInfo(input_cert_info, &parsed_cert_info,
                                   &parsing_error_message)) {
      accepted_certificates.push_back(parsed_cert_info);
    } else {
      rejected_certificates_count++;
      WriteToConsole(blink::mojom::ConsoleMessageLevel::kError,
                     parsing_error_message);
    }
  }

  // TODO(crbug.com/40671053): Remove logging after stabilizing the feature.
  LOG(WARNING) << "Certificates provided by extension " << extension()->id()
               << ": " << accepted_certificates.size() << ", rejected "
               << rejected_certificates_count;

  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);
  service->SetCertificatesProvidedByExtension(extension_id(),
                                              accepted_certificates);

  if (params->details.certificates_request_id &&
      !service->SetExtensionCertificateReplyReceived(
          extension_id(), *params->details.certificates_request_id)) {
    // The extension supplied invalid request ID: it could be an unknown value,
    // or a value that was already reported before, or the request timed out.
    return RespondNow(Error(kCertificateProviderErrorInvalidId));
  }

  return RespondNow(NoArguments());
}

CertificateProviderInternalReportSignatureFunction::
    ~CertificateProviderInternalReportSignatureFunction() {}

ExtensionFunction::ResponseAction
CertificateProviderInternalReportSignatureFunction::Run() {
  std::optional<api_cpi::ReportSignature::Params> params =
      api_cpi::ReportSignature::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  std::vector<uint8_t> signature;
  // If an error occurred, |signature| will not be set.
  if (params->signature)
    signature.assign(params->signature->begin(), params->signature->end());

  if (!service->ReplyToSignRequest(extension_id(), params->request_id,
                                   signature)) {
    // The request was aborted before, or the extension managed to bypass the
    // checks in the API bindings and specified a bad or an already used id.
    DLOG(WARNING) << "Unexpected reply of extension " << extension_id()
                  << " to sign request " << params->request_id;
  }
  return RespondNow(NoArguments());
}

CertificateProviderReportSignatureFunction::
    ~CertificateProviderReportSignatureFunction() = default;

ExtensionFunction::ResponseAction
CertificateProviderReportSignatureFunction::Run() {
  std::optional<api_cp::ReportSignature::Params> params =
      api_cp::ReportSignature::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->details.signature && !params->details.signature->empty() &&
      params->details.error != api_cp::Error::kNone) {
    return RespondNow(Error(kCertificateProviderErrorUnexpectedError));
  }
  if ((!params->details.signature || params->details.signature->empty()) &&
      params->details.error == api_cp::Error::kNone) {
    // It's not allowed to supply empty result without an error code.
    return RespondNow(Error(kCertificateProviderErrorNeitherResultNorError));
  }

  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  std::vector<uint8_t> signature;
  // If an error occurred, |signature| will not be set.
  if (params->details.signature) {
    signature.assign(params->details.signature->begin(),
                     params->details.signature->end());
  }

  if (!service->ReplyToSignRequest(
          extension_id(), params->details.sign_request_id, signature)) {
    return RespondNow(Error(kCertificateProviderInvalidSignId));
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
