// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_SMART_CARD_PROVIDER_PRIVATE_SMART_CARD_PROVIDER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_SMART_CARD_PROVIDER_PRIVATE_SMART_CARD_PROVIDER_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class SmartCardProviderPrivateReportEstablishContextResultFunction
    : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~SmartCardProviderPrivateReportEstablishContextResultFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION(
      "smartCardProviderPrivate.reportEstablishContextResult",
      SMARTCARDPROVIDERPRIVATE_REPORTESTABLISHCONTEXTRESULT)
};

class SmartCardProviderPrivateReportReleaseContextResultFunction
    : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~SmartCardProviderPrivateReportReleaseContextResultFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION(
      "smartCardProviderPrivate.reportReleaseContextResult",
      SMARTCARDPROVIDERPRIVATE_REPORTRELEASECONTEXTRESULT)
};

class SmartCardProviderPrivateReportListReadersResultFunction
    : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~SmartCardProviderPrivateReportListReadersResultFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("smartCardProviderPrivate.reportListReadersResult",
                             SMARTCARDPROVIDERPRIVATE_REPORTLISTREADERSRESULT)
};

class SmartCardProviderPrivateReportGetStatusChangeResultFunction
    : public ExtensionFunction {
 private:
  // ExtensionFunction:
  ~SmartCardProviderPrivateReportGetStatusChangeResultFunction() override;
  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION(
      "smartCardProviderPrivate.reportGetStatusChangeResult",
      SMARTCARDPROVIDERPRIVATE_REPORTGETSTATUSCHANGERESULT)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_SMART_CARD_PROVIDER_PRIVATE_SMART_CARD_PROVIDER_PRIVATE_API_H_
