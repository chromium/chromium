// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/real_time_report_generator_delegate.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "components/enterprise/browser/reporting/real_time_report_type.h"
#include "components/enterprise/common/proto/synced/extensions_workflow_events.pb.h"
#include "extensions/buildflags/buildflags.h"

namespace enterprise_reporting {

RealTimeReportGeneratorDelegate::RealTimeReportGeneratorDelegate() = default;
RealTimeReportGeneratorDelegate::~RealTimeReportGeneratorDelegate() = default;

std::vector<std::unique_ptr<google::protobuf::MessageLite>>
RealTimeReportGeneratorDelegate::Generate(
    RealTimeReportType type,
    const RealTimeReportGenerator::Data& data) {
  std::vector<std::unique_ptr<google::protobuf::MessageLite>> reports;
  switch (type) {
    case RealTimeReportType::kExtensionRequest:
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      for (auto& report : extension_request_report_generator_.Generate(data)) {
        reports.push_back(std::move(report));
      }
#endif
      break;
    case RealTimeReportType::kLegacyTech:
      reports.push_back(legacy_tech_report_generator_.Generate(data));
      break;
  }
  return reports;
}

}  // namespace enterprise_reporting
