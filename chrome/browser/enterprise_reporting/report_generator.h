// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_GENERATOR_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_GENERATOR_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/enterprise_reporting/browser_report_generator.h"
#include "chrome/browser/enterprise_reporting/report_request_queue_generator.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

class ReportGenerator {
 public:
#if defined(OS_CHROMEOS)
  using Request = em::ChromeOsUserReportRequest;
#else
  using Request = em::ChromeDesktopReportRequest;
#endif
  using Requests = std::queue<std::unique_ptr<Request>>;
  using ReportCallback = base::OnceCallback<void(Requests)>;

  ReportGenerator();
  virtual ~ReportGenerator();

  virtual void Generate(ReportCallback callback);

  void SetMaximumReportSizeForTesting(size_t size);

 protected:
  // Creates a basic request that will be used by all Profiles.
  void CreateBasicRequest();

  // Returns an OS report contains basic OS information includes OS name, OS
  // architecture and OS version.
  virtual std::unique_ptr<em::OSReport> GetOSReport();

  // Returns the name of computer.
  virtual std::string GetMachineName();

  // Returns the name of OS user.
  virtual std::string GetOSUserName();

  // Returns the Serial number of the device. It's Windows only field and empty
  // on other platforms.
  virtual std::string GetSerialNumber();

 private:
  void OnBrowserReportReady(std::unique_ptr<em::BrowserReport> browser_report);

  ReportRequestQueueGenerator report_request_queue_generator_;
  BrowserReportGenerator browser_report_generator_;
  ReportCallback callback_;
  // Basic information that is shared among requests.
  Request basic_request_;

  base::WeakPtrFactory<ReportGenerator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ReportGenerator);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_GENERATOR_H_
