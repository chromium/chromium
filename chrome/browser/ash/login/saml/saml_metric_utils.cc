// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/saml_metric_utils.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {
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

}  // namespace

void RecordSAMLProvider(const std::string& x509certificate) {
  net::CertificateList third_party_cert_list =
      net::X509Certificate::CreateCertificateListFromBytes(
          x509certificate.data(), x509certificate.size(),
          net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);

  std::string provider;

  if (!third_party_cert_list.empty() && third_party_cert_list[0] != nullptr) {
    if (!third_party_cert_list[0]->subject().organization_names.empty()) {
      provider = third_party_cert_list[0]->subject().organization_names[0];
    } else {
      // Some providers don't include organization name in the certifcate
      provider = third_party_cert_list[0]->subject().common_name;
    }
  }

  provider = base::ToLowerASCII(provider);

  ChromeOSSamlProvider samlProvider;
  if (provider.empty()) {
    samlProvider = ChromeOSSamlProvider::kFailure;
    LOG(WARNING) << "Failed to parse SAML provider certificate";
  } else if (provider == "adfs") {
    samlProvider = ChromeOSSamlProvider::kAdfs;
  } else if (provider == "microsoft azure federated sso certificate") {
    samlProvider = ChromeOSSamlProvider::kAzure;
  } else if (provider == "okta") {
    samlProvider = ChromeOSSamlProvider::kOkta;
  } else if (provider == "ping identity") {
    samlProvider = ChromeOSSamlProvider::kPing;
  } else if (provider == "onelogin") {
    samlProvider = ChromeOSSamlProvider::kOnelogin;
  } else if (provider == "microsoft") {
    samlProvider = ChromeOSSamlProvider::kMicrosoft;
  } else if (provider == "clever") {
    samlProvider = ChromeOSSamlProvider::kClever;
  } else if (provider == "cloudgate") {
    samlProvider = ChromeOSSamlProvider::kCloudgate;
  } else if (provider == "windows") {
    samlProvider = ChromeOSSamlProvider::kWindows;
  } else if (provider == "salesforce") {
    samlProvider = ChromeOSSamlProvider::kSalesforce;
  } else {
    samlProvider = ChromeOSSamlProvider::kUnknown;
    LOG(WARNING) << "Unknown SAML provider: " << provider;
  }

  base::UmaHistogramEnumeration("ChromeOS.SAML.Provider", samlProvider);
}

}  // namespace metrics
}  // namespace chromeos
