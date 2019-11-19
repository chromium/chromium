// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CLOUD_PRINT_PRIVATE_CLOUD_PRINT_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CLOUD_PRINT_PRIVATE_CLOUD_PRINT_PRIVATE_API_H_

#include <string>
#include <vector>

#include "build/build_config.h"
#include "extensions/browser/extension_function.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PRINT_PREVIEW) || defined(OS_CHROMEOS)
#error "Print Preview must be enabled / Not supported on ChromeOS"
#endif

namespace extensions {

namespace api {
namespace cloud_print_private {

struct UserSettings;

}  // namespace cloud_print_private
}  // namespace api


// For use only in tests.
class CloudPrintTestsDelegate {
 public:
  static CloudPrintTestsDelegate* Get();

  CloudPrintTestsDelegate();
  virtual ~CloudPrintTestsDelegate();

  virtual void SetupConnector(
      const std::string& user_email,
      const std::string& robot_email,
      const std::string& credentials,
      const api::cloud_print_private::UserSettings& user_settings) = 0;

  virtual std::string GetHostName() = 0;

  virtual std::string GetClientId() = 0;

  virtual std::vector<std::string> GetPrinters() = 0;
};

class CloudPrintPrivateSetupConnectorFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cloudPrintPrivate.setupConnector",
                             CLOUDPRINTPRIVATE_SETUPCONNECTOR)

  CloudPrintPrivateSetupConnectorFunction();

 protected:
  ~CloudPrintPrivateSetupConnectorFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class CloudPrintPrivateGetHostNameFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cloudPrintPrivate.getHostName",
                             CLOUDPRINTPRIVATE_GETHOSTNAME)

  CloudPrintPrivateGetHostNameFunction();

 protected:
  ~CloudPrintPrivateGetHostNameFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class CloudPrintPrivateGetPrintersFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cloudPrintPrivate.getPrinters",
                             CLOUDPRINTPRIVATE_GETPRINTERS)

  CloudPrintPrivateGetPrintersFunction();

 protected:
  ~CloudPrintPrivateGetPrintersFunction() override;

 private:
  void SendResults(const std::vector<std::string>& printers);

  // ExtensionFunction:
  ResponseAction Run() override;
};

class CloudPrintPrivateGetClientIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("cloudPrintPrivate.getClientId",
                             CLOUDPRINTPRIVATE_GETCLIENTID)

  CloudPrintPrivateGetClientIdFunction();

 protected:
  ~CloudPrintPrivateGetClientIdFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CLOUD_PRINT_PRIVATE_CLOUD_PRINT_PRIVATE_API_H_
