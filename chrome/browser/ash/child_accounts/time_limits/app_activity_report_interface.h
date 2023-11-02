// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REPORT_INTERFACE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REPORT_INTERFACE_H_

#include "base/time/time.h"

class Profile;

namespace enterprise_management {
class ChildStatusReportRequest;
}  // namespace enterprise_management

namespace ash {
namespace app_time {

// Interface of the object generating app activity for child user.
class AppActivityReportInterface {
 public:
  // Parameters of the generated report.
  struct ReportParams {
    // Time the report was generated.
    base::Time generation_time;

    // Whether any data were added to the report.
    bool anything_reported = false;
  };

  // Factory method that returns object generating app activity for child user.
  // feature. Provided to reduce the dependencies between API consumer and child
  // user related code. AppActivityReportInterface object has a lifetime of a
  // KeyedService.
  static AppActivityReportInterface* Get(Profile* profile);

  virtual ~AppActivityReportInterface();

  // Populates child status |report| with collected app activity.
  // Returns whether any data were populated.
  virtual ReportParams GenerateAppActivityReport(
      enterprise_management::ChildStatusReportRequest* report) = 0;

  // Clears the stored app activity older than |report_generation_timestamp|.
  // Should be called when child status report was successfully submitted.
  virtual void AppActivityReportSubmitted(
      base::Time report_generation_timestamp) = 0;
};

}  // namespace app_time
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_TIME_LIMITS_APP_ACTIVITY_REPORT_INTERFACE_H_
