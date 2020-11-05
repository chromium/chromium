// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_REPORT_THROTTLER_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_REPORT_THROTTLER_H_

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/timer/timer.h"

namespace base {
class FilePath;
class TimeDelta;
}  // namespace base

namespace enterprise_reporting {

// A throttler to batch extension request report. This is used by real time
// extension request with device management service only as the new real time
// report pipeline already provides batch feature.
// A request will be uploaded when the previous request is uploaded at least
// one minute before and finished. Otherwise, the requests will be batched and
// wait.
class ExtensionRequestReportThrottler {
 public:
  // Returns the singleton instance of ExtensionRequestReportThrottler.
  static ExtensionRequestReportThrottler* Get();

  ExtensionRequestReportThrottler();
  ExtensionRequestReportThrottler(const ExtensionRequestReportThrottler&) =
      delete;
  ExtensionRequestReportThrottler& operator=(
      const ExtensionRequestReportThrottler&) = delete;
  ~ExtensionRequestReportThrottler();

  void Enable(base::TimeDelta throttle_time,
              base::RepeatingClosure report_trigger);
  void Disable();
  bool IsEnabled() const;

  // Adds a profile that has new request update.
  void AddProfile(const base::FilePath& profile_path);

  // Gets all profiles that has pending update.
  const base::flat_set<base::FilePath>& GetProfiles() const;

  // Clean all pending update. This should be called once a report contains
  // extension request has been generated, regardless the trigger.
  void ResetProfiles();

  // Resume the throttler once a real-time request is uploaded successfully. In
  // case of any transient or persistent error, the report throttler will be
  // paused by the |ongong_upload_| flag until it's recovered from a full
  // report.
  void OnExtensionRequestUploaded();

 private:
  bool ShouldUpload();
  void MaybeUpload();

  base::RepeatingClosure report_trigger_;

  // The batched profiles that has un-uploaded
  base::flat_set<base::FilePath> profiles_;
  std::unique_ptr<base::RetainingOneShotTimer> throttle_timer_;
  bool ongoing_upload_ = false;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_REPORT_THROTTLER_H_
