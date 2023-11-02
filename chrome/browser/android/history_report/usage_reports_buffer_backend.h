// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORTS_BUFFER_BACKEND_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORTS_BUFFER_BACKEND_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/trace_event/memory_dump_provider.h"

namespace base {
class FilePath;
}  // namespace base

namespace leveldb {
class DB;
}  // namespace leveldb

namespace history_report {

class UsageReport;

// Stores usage reports which will be sent for history reporting in batches.
class UsageReportsBufferBackend : public base::trace_event::MemoryDumpProvider {
 public:
  explicit UsageReportsBufferBackend(const base::FilePath& dir);

  UsageReportsBufferBackend(const UsageReportsBufferBackend&) = delete;
  UsageReportsBufferBackend& operator=(const UsageReportsBufferBackend&) =
      delete;

  ~UsageReportsBufferBackend() override;

  // Creates and initializes the internal data structures.
  bool Init();

  void AddVisit(const std::string& id, int64_t timestamp_ms, bool typed_visit);

  // Returns a set of up to |amount| usage reports.
  std::unique_ptr<std::vector<UsageReport>> GetUsageReportsBatch(int amount);

  void Remove(const std::vector<std::string>& reports);

  // Clears the buffer by removing all its usage reports.
  void Clear();

  // Dumps internal state to string. For debuging.
  std::string Dump();

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  // NULL until Init method is called.
  std::unique_ptr<leveldb::DB> db_;
  base::FilePath db_file_name_;
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_USAGE_REPORTS_BUFFER_BACKEND_H_
