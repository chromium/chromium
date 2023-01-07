// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_HISTORY_REPORT_DELTA_FILE_BACKEND_LEVELDB_H_
#define CHROME_BROWSER_ANDROID_HISTORY_REPORT_DELTA_FILE_BACKEND_LEVELDB_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/trace_event/memory_dump_provider.h"

class GURL;

namespace leveldb {
class DB;
}

namespace history_report {

class DeltaFileEntryWithData;

// Backend for delta file.
class DeltaFileBackend : public base::trace_event::MemoryDumpProvider {
 public:
  explicit DeltaFileBackend(const base::FilePath& dir);

  DeltaFileBackend(const DeltaFileBackend&) = delete;
  DeltaFileBackend& operator=(const DeltaFileBackend&) = delete;

  ~DeltaFileBackend() override;

  // Adds new addition entry to delta file
  void PageAdded(const GURL& url);
  // Adds new deletion entry to delta file
  void PageDeleted(const GURL& url);
  // Removes all delta file entries with
  // sequence number <= min(|lower_bound|, max sequence number - 1).
  // Returns max sequence number in delta file.
  int64_t Trim(int64_t lower_bound);
  // Recreates delta file using given |urls|.
  bool Recreate(const std::vector<std::string>& urls);
  // Provides up to |limit| delta file entries with
  // sequence number > |last_seq_no|.
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> Query(
      int64_t last_seq_no,
      int32_t limit);
  // Removes all entries from delta file
  void Clear();

  // Dumps internal state to string. For debuging.
  std::string Dump();

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  // Starts delta file backend.
  bool Init();

  bool EnsureInitialized();

  class DigitsComparator;

  base::FilePath path_;
  std::unique_ptr<leveldb::DB> db_;
  std::unique_ptr<DigitsComparator> leveldb_cmp_;
};

}  // namespace history_report

#endif  // CHROME_BROWSER_ANDROID_HISTORY_REPORT_DELTA_FILE_BACKEND_LEVELDB_H_
