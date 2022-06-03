// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/usage_reports_buffer_backend.h"

#include <inttypes.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "chrome/browser/android/history_report/usage_report_util.h"
#include "chrome/browser/android/proto/delta_file.pb.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace {
const base::FilePath::CharType kBufferFileName[] =
    FILE_PATH_LITERAL("UsageReportsBuffer");
}  // namespace

namespace history_report {

UsageReportsBufferBackend::UsageReportsBufferBackend(const base::FilePath& dir)
    : db_file_name_(dir.Append(kBufferFileName)) {}

UsageReportsBufferBackend::~UsageReportsBufferBackend() {}

bool UsageReportsBufferBackend::Init() {
  leveldb_env::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum number of files.
  options.write_buffer_size = 500 * 1024;
  std::string path = db_file_name_.value();
  leveldb::Status status = leveldb_env::OpenDB(options, path, &db_);
  if (status.IsCorruption()) {
    LOG(ERROR) << "Deleting corrupt database";
    status = leveldb_chrome::DeleteDB(db_file_name_, options);
    if (!status.ok()) {
      LOG(ERROR) << "Unable to delete " << db_file_name_
                 << ", error: " << status.ToString();
      return false;
    }
    status = leveldb_env::OpenDB(options, path, &db_);
  }
  if (!status.ok()) {
    LOG(WARNING) << "Unable to open " << path << ": " << status.ToString();
    return false;
  }
  CHECK(db_);

  UMA_HISTOGRAM_COUNTS_1M(
      "Search.HistoryReport.UsageReportsBuffer.LevelDBEntries",
      usage_report_util::DatabaseEntries(db_.get()));

  return true;
}

void UsageReportsBufferBackend::AddVisit(const std::string& id,
                                         int64_t timestamp_ms,
                                         bool typed_visit) {
  if (!db_.get()) {
    LOG(WARNING) << "AddVisit db not initilized.";
    return;
  }
  history_report::UsageReport report;
  report.set_id(id);
  report.set_timestamp_ms(timestamp_ms);
  report.set_typed_visit(typed_visit);
  leveldb::WriteOptions writeOptions;
  leveldb::Status status = db_->Put(
      writeOptions,
      leveldb::Slice(usage_report_util::ReportToKey(report)),
      leveldb::Slice(report.SerializeAsString()));
  if (!status.ok())
    LOG(WARNING) << "AddVisit failed " << status.ToString();
}

std::unique_ptr<std::vector<UsageReport>>
UsageReportsBufferBackend::GetUsageReportsBatch(int batch_size) {
  std::unique_ptr<std::vector<UsageReport>> reports(
      new std::vector<UsageReport>());
  if (!db_.get()) {
    return reports;
  }
  reports->reserve(batch_size);
  leveldb::ReadOptions options;
  std::unique_ptr<leveldb::Iterator> db_iter(db_->NewIterator(options));
  db_iter->SeekToFirst();
  while (batch_size > 0 && db_iter->Valid()) {
    history_report::UsageReport last_report;
    leveldb::Slice value_slice = db_iter->value();
    if (last_report.ParseFromArray(value_slice.data(), value_slice.size())) {
      reports->emplace_back(std::move(last_report));
      --batch_size;
    }
    db_iter->Next();
  }
  return reports;
}

void UsageReportsBufferBackend::Remove(
    const std::vector<std::string>& reports) {
  if (!db_.get()) {
    return;
  }
  // TODO(haaawk): investigate if it's worth sorting the keys here to improve
  // performance.
  leveldb::WriteBatch updates;
  for (const auto& report : reports) {
    updates.Delete(leveldb::Slice(report));
  }

  leveldb::WriteOptions write_options;
  leveldb::Status status = db_->Write(write_options, &updates);
  if (!status.ok()) {
    LOG(WARNING) << "Remove failed: " << status.ToString();
  }
}

void UsageReportsBufferBackend::Clear() {
  db_.reset();
  leveldb_chrome::DeleteDB(db_file_name_, leveldb_env::Options());
  Init();
}

std::string UsageReportsBufferBackend::Dump() {
  std::string dump("\n UsageReportsBuffer [");
  if (!db_.get()) {
    dump.append("not initialized]");
    return dump;
  }
  dump.append("num pending entries=");
  dump.append(
      base::NumberToString(usage_report_util::DatabaseEntries(db_.get())));
  dump.append("]");
  return dump;
}

bool UsageReportsBufferBackend::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (!db_)
    return true;

  // leveldb_env::DBTracker already records memory usage. Add ownership edge
  // to the dump.
  auto* tracker_dump =
      leveldb_env::DBTracker::GetOrCreateAllocatorDump(pmd, db_.get());
  if (!tracker_dump)
    return true;

  auto* dump = pmd->CreateAllocatorDump(
      base::StringPrintf("history/usage_reports_buffer/leveldb_0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(db_.get())));
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  tracker_dump->GetSizeInternal());
  pmd->AddOwnershipEdge(dump->guid(), tracker_dump->guid());
  return true;
}

}  // namespace history_report
