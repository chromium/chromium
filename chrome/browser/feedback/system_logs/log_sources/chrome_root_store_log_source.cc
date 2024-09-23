// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/chrome_root_store_log_source.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/cert/x509_util.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

namespace system_logs {

namespace {

constexpr char kChromeRootStoreKey[] = "chrome_root_store";

// Process returned Chrome Root Store certs. Certs that are processed here are
// (a) compiled into the Chrome binary or (b) from Component Updater, which
// makes parsing these bytes not a Rule of 2 violation
// (https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rule-of-2.md).
void PopulateChromeRootStoreLogsAsync(
    system_logs::SysLogsSourceCallback callback,
    cert_verifier::mojom::ChromeRootStoreInfoPtr info) {
  auto response = std::make_unique<system_logs::SystemLogsResponse>();

  std::string entry;
  entry += "version: " + base::NumberToString(info->version) + "\n\n";
  for (auto const& cert_info : info->root_cert_info) {
    x509_certificate_model::X509CertificateModel model(
        net::x509_util::CreateCryptoBuffer(cert_info->cert), "");
    entry += "hash: " + cert_info->sha256hash_hex +
             "  name: " + model.GetTitle() + "\n";
  }
  response->emplace(kChromeRootStoreKey, std::move(entry));
  std::move(callback).Run(std::move(response));
}

}  // namespace

ChromeRootStoreLogSource::ChromeRootStoreLogSource()
    : SystemLogsSource("ChromeRootStore") {}

ChromeRootStoreLogSource::~ChromeRootStoreLogSource() {}

void ChromeRootStoreLogSource::Fetch(
    system_logs::SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  cert_verifier::mojom::CertVerifierServiceFactory* factory =
      content::GetCertVerifierServiceFactory();
  DCHECK(factory);
  factory->GetChromeRootStoreInfo(
      base::BindOnce(&PopulateChromeRootStoreLogsAsync, std::move(callback)));
}

}  // namespace system_logs
