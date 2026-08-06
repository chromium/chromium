// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_GENERATOR_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_GENERATOR_DELEGATE_H_

#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_generator.h"
#endif
#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_generator.h"

namespace enterprise_reporting {

class RealTimeReportGeneratorDelegate
    : public RealTimeReportGenerator::Delegate {
 public:
  RealTimeReportGeneratorDelegate();
  RealTimeReportGeneratorDelegate(const RealTimeReportGeneratorDelegate&) =
      delete;
  RealTimeReportGeneratorDelegate& operator=(
      const RealTimeReportGeneratorDelegate&) = delete;
  ~RealTimeReportGeneratorDelegate() override;

  // RealTimeReportGenerator::Delegate
  std::vector<std::unique_ptr<google::protobuf::MessageLite>> Generate(
      RealTimeReportType type,
      const RealTimeReportGenerator::Data& data) override;

 private:
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  ExtensionRequestReportGenerator extension_request_report_generator_;
#endif
  LegacyTechReportGenerator legacy_tech_report_generator_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REAL_TIME_REPORT_GENERATOR_DELEGATE_H_
