// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/saml_metric_utils.h"

#include <string>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "net/cert/x509_certificate.h"

namespace ash {
namespace metrics {
namespace {

// Must be kept consistent with ChromeOSSamlProvider in enums.xml
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused
enum class ChromeOSSamlProvider {
  kUnknown = 0,
  kFailure = 1,
  kAdfs = 2,
  kAzure = 3,
  kOkta = 4,
  kPing = 5,
  kOnelogin = 6,
  kMicrosoft = 7,
  kClever = 8,
  kCloudgate = 9,
  kWindows = 10,
  kSalesforce = 11,
  kMaxValue = kSalesforce,
};

struct Provider {
  const char* prefix;
  ChromeOSSamlProvider enum_value;
};

// when one item is a prefix of another one, then the shorter should come first.
const Provider kProviders[] = {
    {"adfs", ChromeOSSamlProvider::kAdfs},
    {"okta", ChromeOSSamlProvider::kOkta},
    {"ping identity", ChromeOSSamlProvider::kPing},
    {"onelogin", ChromeOSSamlProvider::kOnelogin},
    {"microsoft", ChromeOSSamlProvider::kMicrosoft},
    {"microsoft azure federated sso certificate", ChromeOSSamlProvider::kAzure},
    {"clever", ChromeOSSamlProvider::kClever},
    {"cloudgate", ChromeOSSamlProvider::kCloudgate},
    {"windows", ChromeOSSamlProvider::kWindows},
    {"salesforce", ChromeOSSamlProvider::kSalesforce}};

}  // namespace

void RecordSAMLProvider(const std::string& x509certificate) {
  net::CertificateList third_party_cert_list =
      net::X509Certificate::CreateCertificateListFromBytes(
          base::as_bytes(base::make_span(x509certificate)),
          net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);

  std::string provider_name;

  if (!third_party_cert_list.empty() && third_party_cert_list[0] != nullptr) {
    if (!third_party_cert_list[0]->subject().organization_names.empty()) {
      provider_name = third_party_cert_list[0]->subject().organization_names[0];
    } else {
      // Some providers don't include organization name in the certificate
      provider_name = third_party_cert_list[0]->subject().common_name;
    }
  }

  provider_name = base::ToLowerASCII(provider_name);

  ChromeOSSamlProvider saml_provider = ChromeOSSamlProvider::kUnknown;
  if (provider_name.empty()) {
    saml_provider = ChromeOSSamlProvider::kFailure;
    LOG(WARNING) << "Failed to parse SAML provider certificate";
  } else {
    for (const auto& provider : kProviders) {
      if (base::StartsWith(provider_name, provider.prefix)) {
        saml_provider = provider.enum_value;
      }
    }
  }

  if (saml_provider == ChromeOSSamlProvider::kUnknown) {
    LOG(WARNING) << "Unknown SAML provider: " << provider_name;
  }

  base::UmaHistogramEnumeration("ChromeOS.SAML.Provider", saml_provider);
}

}  // namespace metrics
}  // namespace ash
