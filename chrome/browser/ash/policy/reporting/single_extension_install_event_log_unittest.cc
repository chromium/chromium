// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/single_extension_install_event_log.h"

#include <stdint.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

static const char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
static const int64_t kTimestamp = 12345;
static const char kFileName[] = "event.log";

}  // namespace

class SingleExtensionInstallEventLogTest : public testing::Test {
 protected:
  SingleExtensionInstallEventLogTest() {}

  // testing::Test:
  void SetUp() override {
    log_.reset(new SingleExtensionInstallEventLog(kExtensionId));
  }

  void VerifyHeader(bool incomplete) {
    EXPECT_TRUE(report_.has_extension_id());
    EXPECT_EQ(kExtensionId, report_.extension_id());
    EXPECT_TRUE(report_.has_incomplete());
    EXPECT_EQ(incomplete, report_.incomplete());
  }

  void CreateFile() {
    temp_dir_.reset(new base::ScopedTempDir);
    ASSERT_TRUE(temp_dir_->CreateUniqueTempDir());
    file_.reset(new base::File(temp_dir_->GetPath().Append(kFileName),
                               base::File::FLAG_CREATE_ALWAYS |
                                   base::File::FLAG_WRITE |
                                   base::File::FLAG_READ));
  }

  std::unique_ptr<SingleExtensionInstallEventLog> log_;
  em::ExtensionInstallReport report_;
  std::unique_ptr<base::ScopedTempDir> temp_dir_;
  std::unique_ptr<base::File> file_;
};

// Verify that the extension id is returned correctly.
TEST_F(SingleExtensionInstallEventLogTest, GetPackage) {
  EXPECT_EQ(kExtensionId, log_->id());
}

// Do not add any log entries. Serialize the log. Verify that the serialization
// contains the the correct header data (extension id, incomplete flag) and no
// log entries.
TEST_F(SingleExtensionInstallEventLogTest, SerializeEmpty) {
  EXPECT_TRUE(log_->empty());
  EXPECT_EQ(0, log_->size());

  log_->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  EXPECT_EQ(0, report_.logs_size());
}

// Add a log entry. Verify that the entry is serialized correctly.
TEST_F(SingleExtensionInstallEventLogTest, AddAndSerialize) {
  em::ExtensionInstallReportLogEvent event;
  event.set_timestamp(kTimestamp);
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  log_->Add(event);
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(1, log_->size());

  log_->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  ASSERT_EQ(1, report_.logs_size());
  std::string original_event;
  event.SerializeToString(&original_event);
  std::string log_event;
  report_.logs(0).SerializeToString(&log_event);
  EXPECT_EQ(original_event, log_event);
}

// Add 10 log entries. Verify that they are serialized correctly. Then, clear
// the serialized log entries and verify that the log becomes empty.
TEST_F(SingleExtensionInstallEventLogTest, SerializeAndClear) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(event);
  }
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(10, log_->size());

  log_->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  ASSERT_EQ(10, report_.logs_size());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(i, report_.logs(i).timestamp());
  }

  log_->ClearSerialized();
  EXPECT_TRUE(log_->empty());
  EXPECT_EQ(0, log_->size());

  report_.Clear();
  log_->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  EXPECT_EQ(0, report_.logs_size());
}

// Add 10 log entries. Serialize the log. Add 10 more log entries. Clear the
// serialized log entries. Verify that the log now contains the last 10 entries.
TEST_F(SingleExtensionInstallEventLogTest, SerializeAddAndClear) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(event);
  }
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(10, log_->size());

  log_->Serialize(&report_);

  for (int i = 10; i < 20; ++i) {
    event.set_timestamp(i);
    log_->Add(event);
  }
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(20, log_->size());

  log_->ClearSerialized();
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(10, log_->size());

  log_->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  ASSERT_EQ(10, report_.logs_size());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(i + 10, report_.logs(i).timestamp());
  }
}

// Add more entries than the log has capacity for. Serialize the log. Verify
// that the serialization contains the most recent log entries and the
// incomplete flag is set. Then, clear the serialized log entries. Verify that
// the log becomes empty and the incomplete flag is unset.
TEST_F(SingleExtensionInstallEventLogTest, OverflowSerializeAndClear) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < SingleExtensionInstallEventLog::kLogCapacity + 1; ++i) {
    event.set_timestamp(i);
    log_->Add(event);
  }
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity, log_->size());

  log_->Serialize(&report_);
  VerifyHeader(true /* incomplete */);
  ASSERT_EQ(SingleExtensionInstallEventLog::kLogCapacity, report_.logs_size());
  for (int i = 0; i < SingleExtensionInstallEventLog::kLogCapacity; ++i) {
    EXPECT_EQ(i + 1, report_.logs(i).timestamp());
  }

  log_->ClearSerialized();
  EXPECT_TRUE(log_->empty());
  EXPECT_EQ(0, log_->size());

  report_.Clear();
  log_->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  EXPECT_EQ(0, report_.logs_size());
}

// Add more entries than the log has capacity for. Serialize the log. Add one
// more log entry. Clear the serialized log entries. Verify that the log now
// contains the most recent entry and the incomplete flag is unset.
TEST_F(SingleExtensionInstallEventLogTest, OverflowSerializeAddAndClear) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < SingleExtensionInstallEventLog::kLogCapacity + 1; ++i) {
    event.set_timestamp(i);
    log_->Add(event);
  }
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity, log_->size());

  log_->Serialize(&report_);

  event.set_timestamp(SingleExtensionInstallEventLog::kLogCapacity + 1);
  log_->Add(event);
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity, log_->size());

  log_->ClearSerialized();
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(1, log_->size());

  report_.Clear();
  log_->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  ASSERT_EQ(1, report_.logs_size());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity + 1,
            report_.logs(0).timestamp());
}

// Add more entries than the log has capacity for. Serialize the log. Add
// exactly as many entries as the log has capacity for. Clear the serialized log
// entries. Verify that the log now contains the most recent entries and the
// incomplete flag is unset.
TEST_F(SingleExtensionInstallEventLogTest, OverflowSerializeFillAndClear) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < SingleExtensionInstallEventLog::kLogCapacity + 1; ++i) {
    event.set_timestamp(i);
    log_->Add(event);
  }
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity, log_->size());

  log_->Serialize(&report_);

  for (int i = 0; i < SingleExtensionInstallEventLog::kLogCapacity; ++i) {
    event.set_timestamp(SingleExtensionInstallEventLog::kLogCapacity + i);
    log_->Add(event);
  }
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity, log_->size());

  log_->ClearSerialized();
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity, log_->size());

  report_.Clear();
  log_->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  ASSERT_EQ(SingleExtensionInstallEventLog::kLogCapacity, report_.logs_size());
  for (int i = 0; i < SingleExtensionInstallEventLog::kLogCapacity; ++i) {
    EXPECT_EQ(i + SingleExtensionInstallEventLog::kLogCapacity,
              report_.logs(i).timestamp());
  }
}

// Add more entries than the log has capacity for. Serialize the log. Add more
// entries than the log has capacity for. Clear the serialized log entries.
// Verify that the log now contains the most recent entries and the incomplete
// flag is set.
TEST_F(SingleExtensionInstallEventLogTest, OverflowSerializeOverflowAndClear) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < SingleExtensionInstallEventLog::kLogCapacity + 1; ++i) {
    event.set_timestamp(i);
    log_->Add(event);
  }
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity, log_->size());

  log_->Serialize(&report_);

  for (int i = 0; i < SingleExtensionInstallEventLog::kLogCapacity + 1; ++i) {
    event.set_timestamp(SingleExtensionInstallEventLog::kLogCapacity + i);
    log_->Add(event);
  }
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity, log_->size());

  log_->ClearSerialized();
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity, log_->size());

  report_.Clear();
  log_->Serialize(&report_);
  VerifyHeader(true /* incomplete */);
  ASSERT_EQ(SingleExtensionInstallEventLog::kLogCapacity, report_.logs_size());
  for (int i = 0; i < SingleExtensionInstallEventLog::kLogCapacity; ++i) {
    EXPECT_EQ(i + SingleExtensionInstallEventLog::kLogCapacity + 1,
              report_.logs(i).timestamp());
  }
}

// Load log from a file that is not open. Verify that the operation fails.
TEST_F(SingleExtensionInstallEventLogTest, FailLoad) {
  base::File invalid_file;
  std::unique_ptr<SingleExtensionInstallEventLog> log =
      std::make_unique<SingleExtensionInstallEventLog>(kExtensionId);
  EXPECT_FALSE(SingleExtensionInstallEventLog::Load(&invalid_file, &log));
  EXPECT_FALSE(log);
}

// Add a log entry. Store the log to a file that is not open. Verify that the
// operation fails and the log is not modified.
TEST_F(SingleExtensionInstallEventLogTest, FailStore) {
  em::ExtensionInstallReportLogEvent event;
  event.set_timestamp(0);
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  log_->Add(event);
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(1, log_->size());

  base::File invalid_file;
  EXPECT_FALSE(log_->Store(&invalid_file));
  EXPECT_EQ(kExtensionId, log_->id());
  EXPECT_FALSE(log_->empty());
  EXPECT_EQ(1, log_->size());

  log_->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  ASSERT_EQ(1, report_.logs_size());
  EXPECT_EQ(0, report_.logs(0).timestamp());
}

// Store an empty log. Load the log. Verify that that the log contents are
// loaded correctly.
TEST_F(SingleExtensionInstallEventLogTest, StoreEmptyAndLoad) {
  ASSERT_NO_FATAL_FAILURE(CreateFile());

  log_->Store(file_.get());
  file_->Seek(base::File::FROM_BEGIN, 0);

  std::unique_ptr<SingleExtensionInstallEventLog> log;
  EXPECT_TRUE(SingleExtensionInstallEventLog::Load(file_.get(), &log));
  ASSERT_TRUE(log);
  EXPECT_EQ(kExtensionId, log->id());
  EXPECT_TRUE(log->empty());
  EXPECT_EQ(0, log->size());

  log->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  ASSERT_EQ(0, report_.logs_size());
}

// Populate and store a log. Load the log. Verify that that the log contents are
// loaded correctly.
TEST_F(SingleExtensionInstallEventLogTest, StoreAndLoad) {
  ASSERT_NO_FATAL_FAILURE(CreateFile());

  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(event);
  }

  log_->Store(file_.get());
  file_->Seek(base::File::FROM_BEGIN, 0);

  std::unique_ptr<SingleExtensionInstallEventLog> log;
  EXPECT_TRUE(SingleExtensionInstallEventLog::Load(file_.get(), &log));
  ASSERT_TRUE(log);
  EXPECT_EQ(kExtensionId, log->id());
  EXPECT_FALSE(log->empty());
  EXPECT_EQ(10, log->size());

  log->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  ASSERT_EQ(10, report_.logs_size());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(i, report_.logs(i).timestamp());
  }
}

// Add more entries than the log has capacity for. Store the log. Load the log.
// Verify that the log is marked as incomplete.
TEST_F(SingleExtensionInstallEventLogTest, OverflowStoreAndLoad) {
  ASSERT_NO_FATAL_FAILURE(CreateFile());

  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < SingleExtensionInstallEventLog::kLogCapacity + 1; ++i) {
    event.set_timestamp(i);
    log_->Add(event);
  }

  log_->Store(file_.get());
  file_->Seek(base::File::FROM_BEGIN, 0);

  std::unique_ptr<SingleExtensionInstallEventLog> log;
  EXPECT_TRUE(SingleExtensionInstallEventLog::Load(file_.get(), &log));
  ASSERT_TRUE(log);
  EXPECT_EQ(kExtensionId, log->id());
  EXPECT_FALSE(log->empty());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity, log->size());

  log->Serialize(&report_);
  VerifyHeader(true /* incomplete */);
}

// Populate and serialize a log. Store the log. Load the log. Clear serialized
// entries in the loaded log. Verify that no entries are removed.
TEST_F(SingleExtensionInstallEventLogTest, SerializeStoreLoadAndClear) {
  ASSERT_NO_FATAL_FAILURE(CreateFile());

  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(event);
  }

  log_->Serialize(&report_);

  log_->Store(file_.get());
  file_->Seek(base::File::FROM_BEGIN, 0);

  std::unique_ptr<SingleExtensionInstallEventLog> log;
  EXPECT_TRUE(SingleExtensionInstallEventLog::Load(file_.get(), &log));
  ASSERT_TRUE(log);
  EXPECT_EQ(kExtensionId, log->id());
  EXPECT_FALSE(log->empty());
  EXPECT_EQ(10, log->size());

  log->ClearSerialized();
  EXPECT_FALSE(log->empty());
  EXPECT_EQ(10, log->size());

  report_.Clear();
  log->Serialize(&report_);
  VerifyHeader(false /* incomplete */);
  ASSERT_EQ(10, report_.logs_size());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(i, report_.logs(i).timestamp());
  }
}

// Add 20 log entries. Store the log. Truncate the file to the length of a log
// containing 10 log entries plus one byte. Load the log. Verify that the log
// contains the first 10 log entries and is marked as incomplete.
TEST_F(SingleExtensionInstallEventLogTest, LoadTruncated) {
  ASSERT_NO_FATAL_FAILURE(CreateFile());

  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(event);
  }

  log_->Store(file_.get());
  file_->Seek(base::File::FROM_BEGIN, 0);
  const ssize_t size = file_->GetLength();

  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i + 10);
    log_->Add(event);
  }

  log_->Store(file_.get());
  file_->Seek(base::File::FROM_BEGIN, 0);
  file_->SetLength(size + 1);

  std::unique_ptr<SingleExtensionInstallEventLog> log;
  EXPECT_FALSE(SingleExtensionInstallEventLog::Load(file_.get(), &log));
  ASSERT_TRUE(log);
  EXPECT_EQ(kExtensionId, log->id());
  EXPECT_FALSE(log->empty());
  EXPECT_EQ(10, log->size());

  report_.Clear();
  log->Serialize(&report_);
  VerifyHeader(true /* incomplete */);
  ASSERT_EQ(10, report_.logs_size());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(i, report_.logs(i).timestamp());
  }
}

}  // namespace policy
