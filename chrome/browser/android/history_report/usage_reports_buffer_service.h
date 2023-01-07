// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORTS_BUFFER_SERVICE_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORTS_BUFFER_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"

namespace base {
class FilePath;
}  // namespace base

namespace history_report {

class UsageReport;
class UsageReportsBufferBackend;

// This class is intended to be created once and not destroyed until process is
// killed. |backend_| is assumed to be a long lived pointer.
class UsageReportsBufferService {
 public:
  explicit UsageReportsBufferService(const base::FilePath& dir);

  UsageReportsBufferService(const UsageReportsBufferService&) = delete;
  UsageReportsBufferService& operator=(const UsageReportsBufferService&) =
      delete;

  virtual ~UsageReportsBufferService();

  // Init buffer. All calls to buffer before it's initialized are ignored. It's
  // asynchronous.
  void Init();

  // Add report about page visit to the buffer. It's asynchronous.
  virtual void AddVisit(const std::string& id,
                        int64_t timestamp_ms,
                        bool typed_visit);

  // Get a batch of usage reports of size up to |batch_size|. It's synchronous.
  std::unique_ptr<std::vector<UsageReport>> GetUsageReportsBatch(
      int32_t batch_size);

  // Remove given usage reports from buffer. It's synchronous.
  void Remove(const std::vector<std::string>& report_ids);

  // Clears buffer by removing all usage reports from it.
  void Clear();

  // Dumps internal state to string.
  std::string Dump();

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Non thread safe backend.
  std::unique_ptr<UsageReportsBufferBackend> backend_;
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORTS_BUFFER_SERVICE_H_
