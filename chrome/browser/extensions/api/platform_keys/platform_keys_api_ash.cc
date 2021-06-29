// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_api_ash.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api.h"
#include "chrome/browser/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "chrome/common/extensions/api/platform_keys_internal.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using PublicKeyInfo = chromeos::platform_keys::PublicKeyInfo;

namespace extensions {

namespace api_pk = api::platform_keys;
namespace api_pki = api::platform_keys_internal;

namespace {

using crosapi::keystore_service_util::kWebCryptoEcdsa;
using crosapi::keystore_service_util::kWebCryptoRsassaPkcs1v15;

const struct NameValuePair {
  const char* const name;
  const int value;
} kCertStatusErrors[] = {
#define CERT_STATUS_FLAG(name, value) {#name, value},
#include "net/cert/cert_status_flags_list.h"
#undef CERT_STATUS_FLAG
};

}  // namespace

namespace platform_keys {

const char kTokenIdUser[] = "user";
const char kTokenIdSystem[] = "system";

std::string PlatformKeysTokenIdToApiId(
    chromeos::platform_keys::TokenId platform_keys_token_id) {
  switch (platform_keys_token_id) {
    case chromeos::platform_keys::TokenId::kUser:
      return kTokenIdUser;
    case chromeos::platform_keys::TokenId::kSystem:
      return kTokenIdSystem;
  }
}

}  // namespace platform_keys

//------------------------------------------------------------------------------

PlatformKeysVerifyTLSServerCertificateFunction::
    ~PlatformKeysVerifyTLSServerCertificateFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysVerifyTLSServerCertificateFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<api_pk::VerifyTLSServerCertificate::Params> params(
      api_pk::VerifyTLSServerCertificate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  VerifyTrustAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->Verify(std::move(params), extension_id(),
               base::BindOnce(&PlatformKeysVerifyTLSServerCertificateFunction::
                                  FinishedVerification,
                              this));

  return RespondLater();
}

void PlatformKeysVerifyTLSServerCertificateFunction::FinishedVerification(
    const std::string& error,
    int verify_result,
    int cert_status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!error.empty()) {
    Respond(Error(error));
    return;
  }

  api_pk::VerificationResult result;
  result.trusted = verify_result == net::OK;
  if (net::IsCertificateError(verify_result)) {
    // Only report errors, not internal informational statuses.
    const int masked_cert_status = cert_status & net::CERT_STATUS_ALL_ERRORS;
    for (size_t i = 0; i < base::size(kCertStatusErrors); ++i) {
      if ((masked_cert_status & kCertStatusErrors[i].value) ==
          kCertStatusErrors[i].value) {
        result.debug_errors.push_back(kCertStatusErrors[i].name);
      }
    }
  }

  Respond(ArgumentList(
      api_pk::VerifyTLSServerCertificate::Results::Create(result)));
}

}  // namespace extensions
