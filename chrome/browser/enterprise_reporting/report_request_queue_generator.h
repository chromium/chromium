// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_REQUEST_QUEUE_GENERATOR_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_REQUEST_QUEUE_GENERATOR_H_

#include <memory>
#include <queue>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise_reporting/profile_report_generator.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace enterprise_reporting {

// Generate a report request queue that contains full profile information. The
// request number in the queue is decided by the maximum report size setting.
class ReportRequestQueueGenerator {
#if defined(OS_CHROMEOS)
  using ReportRequest = em::ChromeOsUserReportRequest;
#else
  using ReportRequest = em::ChromeDesktopReportRequest;
#endif

  using ReportRequests = std::queue<std::unique_ptr<ReportRequest>>;
  using BrowserReportGetter =
      base::RepeatingCallback<em::BrowserReport*(ReportRequest*)>;

 public:
  explicit ReportRequestQueueGenerator();
  ReportRequestQueueGenerator(const ReportRequestQueueGenerator&) = delete;
  ReportRequestQueueGenerator& operator=(const ReportRequestQueueGenerator&) =
      delete;
  ~ReportRequestQueueGenerator();

  // Get the maximum report size.
  size_t GetMaximumReportSizeForTesting() const;

  // Set the maximum report size. The full profile info will be skipped or moved
  // to another new request if its size exceeds the limit.
  void SetMaximumReportSizeForTesting(size_t maximum_report_size);

  // Generate a queue of requests including full profile info based on given
  // basic request.
  ReportRequests Generate(const ReportRequest& basic_request);

 private:
  // Generate request with full profile info at |profile_index| according to
  // |basic_request|, then store it into |requests|.
  void GenerateProfileReportWithIndex(const ReportRequest& basic_request,
                                      int profile_index,
                                      ReportRequests* requests);

 private:
  size_t maximum_report_size_;
  ProfileReportGenerator profile_report_generator_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_REQUEST_QUEUE_GENERATOR_H_
