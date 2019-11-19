// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/certificate_provider/certificate_provider_api.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/certificate_provider/pin_dialog_manager.h"
#include "chrome/browser/chromeos/certificate_provider/security_token_pin_dialog_host.h"
#include "chrome/common/extensions/api/certificate_provider.h"
#include "chrome/common/extensions/api/certificate_provider_internal.h"
#include "chromeos/constants/security_token_pin_types.h"
#include "extensions/browser/quota_service.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace api_cp = extensions::api::certificate_provider;
namespace api_cpi = extensions::api::certificate_provider_internal;
using PinCodeType = chromeos::SecurityTokenPinCodeType;
using PinErrorLabel = chromeos::SecurityTokenPinErrorLabel;

namespace {

PinErrorLabel GetErrorLabelForDialog(api_cp::PinRequestErrorType error_type) {
  switch (error_type) {
    case api_cp::PinRequestErrorType::PIN_REQUEST_ERROR_TYPE_INVALID_PIN:
      return PinErrorLabel::kInvalidPin;
    case api_cp::PinRequestErrorType::PIN_REQUEST_ERROR_TYPE_INVALID_PUK:
      return PinErrorLabel::kInvalidPuk;
    case api_cp::PinRequestErrorType::
        PIN_REQUEST_ERROR_TYPE_MAX_ATTEMPTS_EXCEEDED:
      return PinErrorLabel::kMaxAttemptsExceeded;
    case api_cp::PinRequestErrorType::PIN_REQUEST_ERROR_TYPE_UNKNOWN_ERROR:
      return PinErrorLabel::kUnknown;
    case api_cp::PinRequestErrorType::PIN_REQUEST_ERROR_TYPE_NONE:
      return PinErrorLabel::kNone;
  }

  NOTREACHED();
  return PinErrorLabel::kNone;
}

}  // namespace

namespace extensions {

namespace {

const char kCertificateProviderErrorInvalidX509Cert[] =
    "Certificate is not a valid X.509 certificate.";
const char kCertificateProviderErrorECDSANotSupported[] =
    "Key type ECDSA not supported.";
const char kCertificateProviderErrorUnknownKeyType[] = "Key type unknown.";
const char kCertificateProviderErrorAborted[] = "Request was aborted.";
const char kCertificateProviderErrorTimeout[] =
    "Request timed out, reply rejected.";

// requestPin constants.
const char kCertificateProviderNoActiveDialog[] =
    "No active dialog from extension.";
const char kCertificateProviderInvalidId[] = "Invalid signRequestId";
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
  ~RequestPinExceptFirstQuotaBucketMapper() override = default;

  void GetBucketsForArgs(const base::ListValue* args,
                         QuotaLimitHeuristic::BucketList* buckets) override {
    if (args->GetList().empty())
      return;
    const base::Value& details = args->GetList()[0];
    if (!details.is_dict())
      return;
    const base::Value* sign_request_id =
        details.FindKeyOfType("signRequestId", base::Value::Type::INTEGER);
    if (!sign_request_id)
      return;
    if (sign_request_id->GetInt() > biggest_request_id_) {
      // Either it's the first request with the newly issued requestId, or it's
      // an invalid requestId (bigger than the real one). Return a new bucket in
      // order to apply no quota for the former case; for the latter case the
      // quota doesn't matter much, except that we're maybe making it stricter
      // for future requests (which is bearable).
      biggest_request_id_ = sign_request_id->GetInt();
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

  DISALLOW_COPY_AND_ASSIGN(RequestPinExceptFirstQuotaBucketMapper);
};

}  // namespace

const int api::certificate_provider::kMaxClosedDialogsPerMinute = 10;
const int api::certificate_provider::kMaxClosedDialogsPer10Minutes = 30;

CertificateProviderInternalReportCertificatesFunction::
    ~CertificateProviderInternalReportCertificatesFunction() {}

ExtensionFunction::ResponseAction
CertificateProviderInternalReportCertificatesFunction::Run() {
  std::unique_ptr<api_cpi::ReportCertificates::Params> params(
      api_cpi::ReportCertificates::Params::Create(*args_));
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

    if (ParseCertificateInfo(input_cert_info, &parsed_cert_info))
      cert_infos.push_back(parsed_cert_info);
    else
      rejected_certificates.push_back(input_cert_info.certificate);
  }

  if (service->SetCertificatesProvidedByExtension(
          extension_id(), params->request_id, cert_infos)) {
    return RespondNow(ArgumentList(
        api_cpi::ReportCertificates::Results::Create(rejected_certificates)));
  } else {
    // The custom binding already checks for multiple reports to the same
    // request. The only remaining case, why this reply can fail is that the
    // request timed out.
    return RespondNow(Error(kCertificateProviderErrorTimeout));
  }
}

bool CertificateProviderInternalReportCertificatesFunction::
    ParseCertificateInfo(
        const api_cp::CertificateInfo& info,
        chromeos::certificate_provider::CertificateInfo* out_info) {
  const std::vector<uint8_t>& cert_der = info.certificate;
  if (cert_der.empty()) {
    WriteToConsole(blink::mojom::ConsoleMessageLevel::kError,
                   kCertificateProviderErrorInvalidX509Cert);
    return false;
  }

  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323 and crbug.com/788655.
  net::X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  out_info->certificate = net::X509Certificate::CreateFromBytesUnsafeOptions(
      reinterpret_cast<const char*>(cert_der.data()), cert_der.size(), options);
  if (!out_info->certificate) {
    WriteToConsole(blink::mojom::ConsoleMessageLevel::kError,
                   kCertificateProviderErrorInvalidX509Cert);
    return false;
  }

  size_t public_key_length_in_bits = 0;
  net::X509Certificate::PublicKeyType type =
      net::X509Certificate::kPublicKeyTypeUnknown;
  net::X509Certificate::GetPublicKeyInfo(out_info->certificate->cert_buffer(),
                                         &public_key_length_in_bits, &type);

  switch (type) {
    case net::X509Certificate::kPublicKeyTypeRSA:
      break;
    case net::X509Certificate::kPublicKeyTypeECDSA:
      WriteToConsole(blink::mojom::ConsoleMessageLevel::kError,
                     kCertificateProviderErrorECDSANotSupported);
      return false;
    default:
      WriteToConsole(blink::mojom::ConsoleMessageLevel::kError,
                     kCertificateProviderErrorUnknownKeyType);
      return false;
  }

  for (const api_cp::Hash hash : info.supported_hashes) {
    switch (hash) {
      case api_cp::HASH_MD5_SHA1:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_MD5_SHA1);
        break;
      case api_cp::HASH_SHA1:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA1);
        break;
      case api_cp::HASH_SHA256:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA256);
        break;
      case api_cp::HASH_SHA384:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA384);
        break;
      case api_cp::HASH_SHA512:
        out_info->supported_algorithms.push_back(SSL_SIGN_RSA_PKCS1_SHA512);
        break;
      case api_cp::HASH_NONE:
        NOTREACHED();
        return false;
    }
  }
  return true;
}

CertificateProviderStopPinRequestFunction::
    ~CertificateProviderStopPinRequestFunction() {}

ExtensionFunction::ResponseAction
CertificateProviderStopPinRequestFunction::Run() {
  std::unique_ptr<api_cp::RequestPin::Params> params(
      api_cp::RequestPin::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);
  if (params->details.error_type ==
      api_cp::PinRequestErrorType::PIN_REQUEST_ERROR_TYPE_NONE) {
    bool dialog_closed =
        service->pin_dialog_manager()->CloseDialog(extension_id());
    if (!dialog_closed) {
      // This might happen if the user closed the dialog while extension was
      // processing the input.
      return RespondNow(Error(kCertificateProviderNoActiveDialog));
    }

    return RespondNow(NoArguments());
  }

  // Extension provided an error, which means it intends to notify the user with
  // the error and not allow any more input.
  const PinErrorLabel error_label =
      GetErrorLabelForDialog(params->details.error_type);
  const chromeos::PinDialogManager::StopPinRequestResult stop_request_result =
      service->pin_dialog_manager()->StopPinRequestWithError(
          extension()->id(), error_label,
          base::BindOnce(
              &CertificateProviderStopPinRequestFunction::OnPinRequestStopped,
              this));
  switch (stop_request_result) {
    case chromeos::PinDialogManager::StopPinRequestResult::kNoActiveDialog:
      return RespondNow(Error(kCertificateProviderNoActiveDialog));
    case chromeos::PinDialogManager::StopPinRequestResult::kNoUserInput:
      return RespondNow(Error(kCertificateProviderNoUserInput));
    case chromeos::PinDialogManager::StopPinRequestResult::kSuccess:
      return RespondLater();
  }

  NOTREACHED();
  return RespondLater();
}

void CertificateProviderStopPinRequestFunction::OnPinRequestStopped() {
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
      base::TimeDelta::FromMinutes(1)};
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      short_limit_config, new RequestPinExceptFirstQuotaBucketMapper,
      "MAX_PIN_DIALOGS_CLOSED_PER_MINUTE"));

  QuotaLimitHeuristic::Config long_limit_config = {
      api::certificate_provider::kMaxClosedDialogsPer10Minutes - 1,
      base::TimeDelta::FromMinutes(10)};
  heuristics->push_back(std::make_unique<QuotaService::TimedLimit>(
      long_limit_config, new RequestPinExceptFirstQuotaBucketMapper,
      "MAX_PIN_DIALOGS_CLOSED_PER_10_MINUTES"));
}

ExtensionFunction::ResponseAction CertificateProviderRequestPinFunction::Run() {
  std::unique_ptr<api_cp::RequestPin::Params> params(
      api_cp::RequestPin::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const api_cp::PinRequestType pin_request_type =
      params->details.request_type ==
              api_cp::PinRequestType::PIN_REQUEST_TYPE_NONE
          ? api_cp::PinRequestType::PIN_REQUEST_TYPE_PIN
          : params->details.request_type;

  const PinErrorLabel error_label =
      GetErrorLabelForDialog(params->details.error_type);

  const PinCodeType code_type =
      (pin_request_type == api_cp::PinRequestType::PIN_REQUEST_TYPE_PIN)
          ? PinCodeType::kPin
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

  const chromeos::PinDialogManager::RequestPinResult result =
      service->pin_dialog_manager()->RequestPin(
          extension()->id(), extension()->name(),
          params->details.sign_request_id, code_type, error_label,
          attempts_left,
          base::BindOnce(
              &CertificateProviderRequestPinFunction::OnInputReceived, this));
  switch (result) {
    case chromeos::PinDialogManager::RequestPinResult::kSuccess:
      return RespondLater();
    case chromeos::PinDialogManager::RequestPinResult::kInvalidId:
      return RespondNow(Error(kCertificateProviderInvalidId));
    case chromeos::PinDialogManager::RequestPinResult::kOtherFlowInProgress:
      return RespondNow(Error(kCertificateProviderOtherFlowInProgress));
    case chromeos::PinDialogManager::RequestPinResult::kDialogDisplayedAlready:
      return RespondNow(Error(kCertificateProviderPreviousDialogActive));
  }

  NOTREACHED();
  return RespondNow(Error(kCertificateProviderPreviousDialogActive));
}

void CertificateProviderRequestPinFunction::OnInputReceived(
    const std::string& value) {
  std::unique_ptr<base::ListValue> create_results(new base::ListValue());
  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);
  if (!value.empty()) {
    api::certificate_provider::PinResponseDetails details;
    details.user_input = std::make_unique<std::string>(value);
    create_results->Append(details.ToValue());
  }

  Respond(ArgumentList(std::move(create_results)));
}

CertificateProviderInternalReportSignatureFunction::
    ~CertificateProviderInternalReportSignatureFunction() {}

ExtensionFunction::ResponseAction
CertificateProviderInternalReportSignatureFunction::Run() {
  std::unique_ptr<api_cpi::ReportSignature::Params> params(
      api_cpi::ReportSignature::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::CertificateProviderService* const service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  std::vector<uint8_t> signature;
  // If an error occurred, |signature| will not be set.
  if (params->signature)
    signature.assign(params->signature->begin(), params->signature->end());

  service->ReplyToSignRequest(extension_id(), params->request_id, signature);
  return RespondNow(NoArguments());
}

}  // namespace extensions
