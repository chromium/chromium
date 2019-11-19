// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/report_generator.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"

#if defined(OS_WIN)
#include "base/win/wmi.h"
#endif

namespace em = enterprise_management;

namespace enterprise_reporting {

ReportGenerator::ReportGenerator() = default;

ReportGenerator::~ReportGenerator() = default;

void ReportGenerator::Generate(ReportCallback callback) {
  DCHECK(!callback_);
  callback_ = std::move(callback);
  CreateBasicRequest();
}

void ReportGenerator::SetMaximumReportSizeForTesting(size_t size) {
  report_request_queue_generator_.SetMaximumReportSizeForTesting(size);
}

void ReportGenerator::CreateBasicRequest() {
#if !defined(OS_CHROMEOS)
  basic_request_.set_computer_name(this->GetMachineName());
  basic_request_.set_os_user_name(GetOSUserName());
  basic_request_.set_serial_number(GetSerialNumber());
  basic_request_.set_allocated_os_report(GetOSReport().release());
#endif

  browser_report_generator_.Generate(base::BindOnce(
      &ReportGenerator::OnBrowserReportReady, weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<em::OSReport> ReportGenerator::GetOSReport() {
  auto report = std::make_unique<em::OSReport>();
  report->set_name(policy::GetOSPlatform());
  report->set_arch(policy::GetOSArchitecture());
  report->set_version(policy::GetOSVersion());
  return report;
}

std::string ReportGenerator::GetMachineName() {
  return policy::GetMachineName();
}

std::string ReportGenerator::GetOSUserName() {
  return policy::GetOSUsername();
}

std::string ReportGenerator::GetSerialNumber() {
#if defined(OS_WIN)
  return base::UTF16ToUTF8(
      base::win::WmiComputerSystemInfo::Get().serial_number());
#else
  return std::string();
#endif
}

void ReportGenerator::OnBrowserReportReady(
    std::unique_ptr<em::BrowserReport> browser_report) {
  basic_request_.set_allocated_browser_report(browser_report.release());
  Requests requests = report_request_queue_generator_.Generate(basic_request_);
  std::move(callback_).Run(std::move(requests));
}

}  // namespace enterprise_reporting
