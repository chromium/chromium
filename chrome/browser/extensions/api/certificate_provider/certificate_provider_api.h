// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_API_H_

#include <string>

#include "extensions/browser/extension_function.h"

namespace chromeos {
namespace certificate_provider {
struct CertificateInfo;
}  // namespace certificate_provider
}  // namespace chromeos

namespace extensions {

namespace api {
namespace certificate_provider {
// The maximum number of times in the given interval the extension is allowed to
// show the PIN dialog again after user closed the previous one.
extern const int kMaxClosedDialogsPerMinute;
extern const int kMaxClosedDialogsPer10Minutes;

struct CertificateInfo;
}
}

class CertificateProviderInternalReportCertificatesFunction
    : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~CertificateProviderInternalReportCertificatesFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("certificateProviderInternal.reportCertificates",
                             CERTIFICATEPROVIDERINTERNAL_REPORTCERTIFICATES)
};

class CertificateProviderInternalReportSignatureFunction
    : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~CertificateProviderInternalReportSignatureFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("certificateProviderInternal.reportSignature",
                             CERTIFICATEPROVIDERINTERNAL_REPORTSIGNATURE)
};

class CertificateProviderRequestPinFunction : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~CertificateProviderRequestPinFunction() override;
  ResponseAction Run() override;
  bool ShouldSkipQuotaLimiting() const override;
  void GetQuotaLimitHeuristics(
      extensions::QuotaLimitHeuristics* heuristics) const override;

  void OnInputReceived(const std::string& value);

  DECLARE_EXTENSION_FUNCTION("certificateProvider.requestPin",
                             CERTIFICATEPROVIDER_REQUESTPIN)
};

class CertificateProviderStopPinRequestFunction : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~CertificateProviderStopPinRequestFunction() override;
  ResponseAction Run() override;

  void OnPinRequestStopped();

  DECLARE_EXTENSION_FUNCTION("certificateProvider.stopPinRequest",
                             CERTIFICATEPROVIDER_STOPPINREQUEST)
};

class CertificateProviderSetCertificatesFunction : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~CertificateProviderSetCertificatesFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("certificateProvider.setCertificates",
                             CERTIFICATEPROVIDER_SETCERTIFICATES)
};

class CertificateProviderReportSignatureFunction : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~CertificateProviderReportSignatureFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("certificateProvider.reportSignature",
                             CERTIFICATEPROVIDER_REPORTSIGNATURE)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_API_H_
