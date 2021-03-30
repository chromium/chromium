// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_API_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/enterprise/signals/context_info_fetcher.h"
#include "chrome/browser/enterprise/signals/device_info_fetcher.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"
#include "chrome/common/extensions/api/enterprise_reporting_private.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

#if !defined(OS_CHROMEOS)
namespace enterprise_reporting {

extern const char kDeviceIdNotFound[];

}  // namespace enterprise_reporting


class EnterpriseReportingPrivateGetDeviceIdFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getDeviceId",
                             ENTERPRISEREPORTINGPRIVATE_GETDEVICEID)
  EnterpriseReportingPrivateGetDeviceIdFunction();

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

 private:
  ~EnterpriseReportingPrivateGetDeviceIdFunction() override;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseReportingPrivateGetDeviceIdFunction);
};

class EnterpriseReportingPrivateGetPersistentSecretFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getPersistentSecret",
                             ENTERPRISEREPORTINGPRIVATE_GETPERSISTENTSECRET)

  EnterpriseReportingPrivateGetPersistentSecretFunction();
  EnterpriseReportingPrivateGetPersistentSecretFunction(
      const EnterpriseReportingPrivateGetPersistentSecretFunction&) = delete;
  EnterpriseReportingPrivateGetPersistentSecretFunction& operator=(
      const EnterpriseReportingPrivateGetPersistentSecretFunction&) = delete;

 private:
  ~EnterpriseReportingPrivateGetPersistentSecretFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the data was retrieved from the file.
  void OnDataRetrieved(scoped_refptr<base::SequencedTaskRunner> task_runner,
                       const std::string& data,
                       long int status);

  void SendResponse(const std::string& data, long int status);
};

class EnterpriseReportingPrivateGetDeviceDataFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getDeviceData",
                             ENTERPRISEREPORTINGPRIVATE_GETDEVICEDATA)

  EnterpriseReportingPrivateGetDeviceDataFunction();
  EnterpriseReportingPrivateGetDeviceDataFunction(
      const EnterpriseReportingPrivateGetDeviceDataFunction&) = delete;
  EnterpriseReportingPrivateGetDeviceDataFunction& operator=(
      const EnterpriseReportingPrivateGetDeviceDataFunction&) = delete;

 private:
  ~EnterpriseReportingPrivateGetDeviceDataFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the data was retrieved from the file.
  void OnDataRetrieved(scoped_refptr<base::SequencedTaskRunner> task_runner,
                       const std::string& data,
                       RetrieveDeviceDataStatus status);

  void SendResponse(const std::string& data, RetrieveDeviceDataStatus status);
};

class EnterpriseReportingPrivateSetDeviceDataFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.setDeviceData",
                             ENTERPRISEREPORTINGPRIVATE_SETDEVICEDATA)

  EnterpriseReportingPrivateSetDeviceDataFunction();
  EnterpriseReportingPrivateSetDeviceDataFunction(
      const EnterpriseReportingPrivateSetDeviceDataFunction&) = delete;
  EnterpriseReportingPrivateSetDeviceDataFunction& operator=(
      const EnterpriseReportingPrivateSetDeviceDataFunction&) = delete;

 private:
  ~EnterpriseReportingPrivateSetDeviceDataFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the data was stored to the file.
  void OnDataStored(scoped_refptr<base::SequencedTaskRunner> task_runner,
                    bool status);

  void SendResponse(bool status);
};

class EnterpriseReportingPrivateGetDeviceInfoFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getDeviceInfo",
                             ENTERPRISEREPORTINGPRIVATE_GETDEVICEINFO)

  EnterpriseReportingPrivateGetDeviceInfoFunction();
  EnterpriseReportingPrivateGetDeviceInfoFunction(
      const EnterpriseReportingPrivateGetDeviceInfoFunction&) = delete;
  EnterpriseReportingPrivateGetDeviceInfoFunction& operator=(
      const EnterpriseReportingPrivateGetDeviceInfoFunction&) = delete;

 private:
  ~EnterpriseReportingPrivateGetDeviceInfoFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the data was retrieved.
  void OnDeviceInfoRetrieved(
      const ::enterprise_signals::DeviceInfo& device_info);
};

#endif  // !defined(OS_CHROMEOS)

class EnterpriseReportingPrivateGetContextInfoFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("enterprise.reportingPrivate.getContextInfo",
                             ENTERPRISEREPORTINGPRIVATE_GETCONTEXTINFO)

  EnterpriseReportingPrivateGetContextInfoFunction();
  EnterpriseReportingPrivateGetContextInfoFunction(
      const EnterpriseReportingPrivateGetContextInfoFunction&) = delete;
  EnterpriseReportingPrivateGetContextInfoFunction& operator=(
      const EnterpriseReportingPrivateGetContextInfoFunction&) = delete;

 private:
  ~EnterpriseReportingPrivateGetContextInfoFunction() override;

  // ExtensionFunction
  ExtensionFunction::ResponseAction Run() override;

  // Callback once the context data is retrieved.
  void OnContextInfoRetrieved(enterprise_signals::ContextInfo context_info);

  std::unique_ptr<enterprise_signals::ContextInfoFetcher> context_info_fetcher_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_ENTERPRISE_REPORTING_PRIVATE_API_H_
