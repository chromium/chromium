// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/extension_install_event_log.h"

#include <memory>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

static const char kFirstExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
static const char kSecondExtensionId[] = "bcdefghijklmnopabcdefghijklmnopa";
static const char kFileName[] = "event.log";
constexpr int kNumOfCharacters = 26;
constexpr int kExtensionIdLen = 32;

// Helper function that returns a unique extension id for a log. Returns
// lexicographically |log_index|th alphabetical string of length
// |kExtensionIdLen|.
extensions::ExtensionId GetUniqueExtensionId(const int log_index) {
  extensions::ExtensionId extension_id;
  int rem_strings = log_index;
  for (int i = 0; i < kExtensionIdLen; i++)
    extension_id += 'a';
  for (int i = 0; i < kExtensionIdLen; i++) {
    int num_of_strings = 1;
    // Check if number of possible strings after this index are less than
    // |rem_strings| or not.
    for (int j = i + 1; j < kExtensionIdLen; j++) {
      num_of_strings *= kNumOfCharacters;
      if (num_of_strings > rem_strings)
        break;
    }
    if (num_of_strings <= rem_strings) {
      for (char ch = 'a'; ch <= 'z'; ch++) {
        int char_id = ch - 'a';
        // Check first character where number of distinct strings after this
        // index exceeds |rem_strings|.
        if ((char_id + 1) * num_of_strings > rem_strings) {
          extension_id[i] = ch;
          rem_strings -= char_id * num_of_strings;
          break;
        }
      }
    }
  }
  return extension_id;
}
}  // namespace

class ExtensionInstallEventLogTest : public testing::Test {
 protected:
  ExtensionInstallEventLogTest() {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_name_ = temp_dir_.GetPath().Append(kFileName);
    log_ = std::make_unique<ExtensionInstallEventLog>(file_name_);
  }

  void VerifyTenLogEntriesEach(int first_extension_timestamp_offset,
                               int second_extension_timestamp_offset) {
    ASSERT_EQ(2, report_.extension_install_reports_size());
    const int first_extension_index =
        report_.extension_install_reports(0).extension_id() == kFirstExtensionId
            ? 0
            : 1;
    const int second_extension_index = 1 - first_extension_index;

    const em::ExtensionInstallReport& first_extension_log =
        report_.extension_install_reports(first_extension_index);
    EXPECT_EQ(kFirstExtensionId, first_extension_log.extension_id());
    ASSERT_EQ(10, first_extension_log.logs_size());
    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(i + first_extension_timestamp_offset,
                first_extension_log.logs(i).timestamp());
    }

    const em::ExtensionInstallReport& second_extension_log =
        report_.extension_install_reports(second_extension_index);
    EXPECT_EQ(kSecondExtensionId, second_extension_log.extension_id());
    ASSERT_EQ(10, second_extension_log.logs_size());
    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(i + second_extension_timestamp_offset,
                second_extension_log.logs(i).timestamp());
    }
  }

  void OverflowMaxLogs() {
    em::ExtensionInstallReportLogEvent event;
    event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
    for (int i = 0; i < ExtensionInstallEventLog::kMaxLogs - 1; ++i) {
      event.set_timestamp(i);
      log_->Add(GetUniqueExtensionId(i), event);
    }
    for (int i = 0; i < 10; ++i) {
      event.set_timestamp(i + ExtensionInstallEventLog::kMaxLogs - 1);
      log_->Add(kFirstExtensionId, event);
    }
    for (int i = 0; i < 20; ++i) {
      event.set_timestamp(i + ExtensionInstallEventLog::kMaxLogs + 29);
      log_->Add(kSecondExtensionId, event);
    }
  }

  void VerifyOneLogEntryEachPlusFirstExtension(
      int first_extension_log_entries) {
    ASSERT_EQ(ExtensionInstallEventLog::kMaxLogs,
              report_.extension_install_reports_size());
    std::map<std::string, em::ExtensionInstallReport> logs;
    for (int i = 0; i < ExtensionInstallEventLog::kMaxLogs; ++i) {
      logs[report_.extension_install_reports(i).extension_id()] =
          report_.extension_install_reports(i);
    }

    for (int i = 0; i < ExtensionInstallEventLog::kMaxLogs - 1; ++i) {
      std::string extension_id = GetUniqueExtensionId(i);
      const auto log = logs.find(extension_id);
      ASSERT_NE(logs.end(), log);
      EXPECT_EQ(extension_id, log->second.extension_id());
      ASSERT_EQ(1, log->second.logs_size());
      EXPECT_EQ(i, log->second.logs(0).timestamp());
      EXPECT_EQ(em::ExtensionInstallReportLogEvent::SUCCESS,
                log->second.logs(0).event_type());
    }

    const auto log = logs.find(kFirstExtensionId);
    ASSERT_NE(logs.end(), log);
    EXPECT_EQ(kFirstExtensionId, log->second.extension_id());
    ASSERT_EQ(first_extension_log_entries, log->second.logs_size());
    for (int i = 0; i < first_extension_log_entries; ++i) {
      EXPECT_EQ(i + ExtensionInstallEventLog::kMaxLogs - 1,
                log->second.logs(i).timestamp());
      EXPECT_EQ(em::ExtensionInstallReportLogEvent::SUCCESS,
                log->second.logs(i).event_type());
    }

    EXPECT_EQ(logs.end(), logs.find(kSecondExtensionId));
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath file_name_;
  std::unique_ptr<ExtensionInstallEventLog> log_;
  em::ExtensionInstallReportRequest report_;
};

// Do not add any log entries. Serialize the log. Verify that the serialization
// contains no log entries.
TEST_F(ExtensionInstallEventLogTest, SerializeEmpty) {
  EXPECT_EQ(0, log_->total_size());
  EXPECT_EQ(0, log_->max_size());

  log_->Serialize(&report_);
  EXPECT_EQ(0, report_.extension_install_reports_size());
}

// Populate the logs for two extensions. Verify that the entries are serialized
// correctly.
TEST_F(ExtensionInstallEventLogTest, AddAndSerialize) {
  em::ExtensionInstallReportLogEvent event;
  event.set_timestamp(0);
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  log_->Add(kFirstExtensionId, event);
  event.set_timestamp(1);
  log_->Add(kSecondExtensionId, event);
  event.set_timestamp(2);
  log_->Add(kFirstExtensionId, event);
  EXPECT_EQ(3, log_->total_size());
  EXPECT_EQ(2, log_->max_size());

  log_->Serialize(&report_);
  ASSERT_EQ(2, report_.extension_install_reports_size());
  const int first_extension_index =
      report_.extension_install_reports(0).extension_id() == kFirstExtensionId
          ? 0
          : 1;
  const int second_extension_index = 1 - first_extension_index;

  const em::ExtensionInstallReport& first_extension_log =
      report_.extension_install_reports(first_extension_index);
  EXPECT_EQ(kFirstExtensionId, first_extension_log.extension_id());
  ASSERT_EQ(2, first_extension_log.logs_size());
  EXPECT_EQ(0, first_extension_log.logs(0).timestamp());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::SUCCESS,
            first_extension_log.logs(0).event_type());
  EXPECT_EQ(2, first_extension_log.logs(1).timestamp());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::SUCCESS,
            first_extension_log.logs(1).event_type());

  const em::ExtensionInstallReport& second_extension_log =
      report_.extension_install_reports(second_extension_index);
  EXPECT_EQ(kSecondExtensionId, second_extension_log.extension_id());
  ASSERT_EQ(1, second_extension_log.logs_size());
  EXPECT_EQ(1, second_extension_log.logs(0).timestamp());
  EXPECT_EQ(em::ExtensionInstallReportLogEvent::SUCCESS,
            second_extension_log.logs(0).event_type());
}

// Add 10 log entries for an extension. Serialize the log. Clear the serialized
// log entries and verify that the log becomes empty. Then, serialize the log
// again and verify it contains no log entries.
TEST_F(ExtensionInstallEventLogTest, SerializeAndClear) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(kFirstExtensionId, event);
  }
  EXPECT_EQ(10, log_->total_size());
  EXPECT_EQ(10, log_->max_size());

  log_->Serialize(&report_);

  log_->ClearSerialized();
  EXPECT_EQ(0, log_->total_size());
  EXPECT_EQ(0, log_->max_size());

  report_.Clear();
  log_->Serialize(&report_);
  EXPECT_EQ(0, report_.extension_install_reports_size());
}

// Add 10 log entries for a first extension. Serialize the log. Add 10 more log
// entries each for the first and a second extension. Clear the serialized log
// entries. Then, serialize the log again. Verify that it now contains the
// entries added after the first serialization.
TEST_F(ExtensionInstallEventLogTest, SerializeAddClearAndSerialize) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(kFirstExtensionId, event);
  }
  EXPECT_EQ(10, log_->total_size());
  EXPECT_EQ(10, log_->max_size());

  log_->Serialize(&report_);

  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i + 10);
    log_->Add(kFirstExtensionId, event);
  }
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i + 20);
    log_->Add(kSecondExtensionId, event);
  }
  EXPECT_EQ(30, log_->total_size());
  EXPECT_EQ(20, log_->max_size());

  log_->ClearSerialized();
  EXPECT_EQ(20, log_->total_size());
  EXPECT_EQ(10, log_->max_size());

  log_->Serialize(&report_);
  VerifyTenLogEntriesEach(10 /* first_extension_timestamp_offset */,
                          20 /* second_extension_timestamp_offset*/);
}

// Add entries for as many extensions as the log has capacity for. Add entries
// for one more extension. Serialize the log. Verify that the log entries for
// the last extension were ignored. Then, clear the serialized log entries.
// Verify that the log becomes empty.
TEST_F(ExtensionInstallEventLogTest, OverflowSerializeAndClear) {
  OverflowMaxLogs();
  EXPECT_EQ(ExtensionInstallEventLog::kMaxLogs + 9, log_->total_size());
  EXPECT_EQ(10, log_->max_size());

  log_->Serialize(&report_);
  VerifyOneLogEntryEachPlusFirstExtension(10 /* first_extension_log_entries */);

  log_->ClearSerialized();
  EXPECT_EQ(0, log_->total_size());
  EXPECT_EQ(0, log_->max_size());

  report_.Clear();
  log_->Serialize(&report_);
  EXPECT_EQ(0, report_.extension_install_reports_size());
}

// Add entries for as many extensions as the log has capacity for. Add entries
// for one more extension. Add more entries for one of the extensions already in
// the log. Serialize the log. Verify that the log entries for the last
// extension were ignored. Then, clear the serialized log entries. Verify that
// the log becomes empty.
TEST_F(ExtensionInstallEventLogTest, OverflowAddSerializeAndClear) {
  OverflowMaxLogs();
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 20; ++i) {
    event.set_timestamp(i + ExtensionInstallEventLog::kMaxLogs + 9);
    log_->Add(kFirstExtensionId, event);
  }
  EXPECT_EQ(ExtensionInstallEventLog::kMaxLogs + 29, log_->total_size());
  EXPECT_EQ(30, log_->max_size());

  log_->Serialize(&report_);
  VerifyOneLogEntryEachPlusFirstExtension(30 /* first_extension_log_entries */);

  log_->ClearSerialized();
  EXPECT_EQ(0, log_->total_size());
  EXPECT_EQ(0, log_->max_size());

  report_.Clear();
  log_->Serialize(&report_);
  EXPECT_EQ(0, report_.extension_install_reports_size());
}

// Add entries for as many extensions as the log has capacity for. Add entries
// for one more extension. Serialize the log. Add more entries for one of the
// extensions already in the log and another extension. Clear the log. Verify
// that the log now contains the entries added after serialization for the
// extension that was already in the log.
TEST_F(ExtensionInstallEventLogTest, OverflowSerializeAddAndClear) {
  OverflowMaxLogs();
  EXPECT_EQ(ExtensionInstallEventLog::kMaxLogs + 9, log_->total_size());
  EXPECT_EQ(10, log_->max_size());

  log_->Serialize(&report_);

  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 20; ++i) {
    event.set_timestamp(i + ExtensionInstallEventLog::kMaxLogs + 9);
    log_->Add(kFirstExtensionId, event);
  }
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i + ExtensionInstallEventLog::kMaxLogs + 49);
    log_->Add(kSecondExtensionId, event);
  }

  log_->ClearSerialized();
  EXPECT_EQ(20, log_->total_size());
  EXPECT_EQ(20, log_->max_size());

  report_.Clear();
  log_->Serialize(&report_);
  ASSERT_EQ(1, report_.extension_install_reports_size());
  const em::ExtensionInstallReport& extension_log =
      report_.extension_install_reports(0);
  EXPECT_EQ(kFirstExtensionId, extension_log.extension_id());
  ASSERT_EQ(20, extension_log.logs_size());
  for (int i = 0; i < 20; ++i) {
    EXPECT_EQ(i + ExtensionInstallEventLog::kMaxLogs + 9,
              extension_log.logs(i).timestamp());
  }
}

// Add 10 log entries for a first extension and more entries than the log has
// capacity for for a second extension. Verify that the total and maximum log
// sizes are reported correctly.
TEST_F(ExtensionInstallEventLogTest, OverflowSingleExtension) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(kFirstExtensionId, event);
  }
  for (int i = 0; i < SingleExtensionInstallEventLog::kLogCapacity + 1; ++i) {
    event.set_timestamp(i + 10);
    log_->Add(kSecondExtensionId, event);
  }
  EXPECT_EQ(10 + SingleExtensionInstallEventLog::kLogCapacity,
            log_->total_size());
  EXPECT_EQ(SingleExtensionInstallEventLog::kLogCapacity, log_->max_size());
}

// Create an empty log. Store the log. Verify that no log file is created.
TEST_F(ExtensionInstallEventLogTest, Store) {
  log_->Store();
  EXPECT_FALSE(base::PathExists(file_name_));
}

// Add a log entry. Store the log. Verify that a log file is created. Then,
// delete the log file. Store the log. Verify that no log file is created.
TEST_F(ExtensionInstallEventLogTest, AddStoreAndStore) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  event.set_timestamp(0);
  log_->Add(kFirstExtensionId, event);
  log_->Store();
  EXPECT_TRUE(base::PathExists(file_name_));

  EXPECT_TRUE(base::DeleteFile(file_name_));

  log_->Store();
  EXPECT_FALSE(base::PathExists(file_name_));
}

// Serialize the log. Clear the serialized log entries. Store the log. Verify
// that no log file is created.
TEST_F(ExtensionInstallEventLogTest, SerializeClearAndStore) {
  log_->Serialize(&report_);
  log_->ClearSerialized();
  log_->Store();
  EXPECT_FALSE(base::PathExists(file_name_));
}

// Add a log entry. Serialize the log. Clear the serialized log entries. Store
// the log. Verify that a log file is created. Verify that that the log contents
// are loaded correctly.
TEST_F(ExtensionInstallEventLogTest, AddSerializeCleaStoreAndLoad) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  event.set_timestamp(0);
  log_->Add(kFirstExtensionId, event);
  log_->Serialize(&report_);
  log_->ClearSerialized();
  log_->Store();
  EXPECT_TRUE(base::PathExists(file_name_));

  ExtensionInstallEventLog log(file_name_);
  EXPECT_EQ(0, log.total_size());
  EXPECT_EQ(0, log.max_size());

  report_.Clear();
  log.Serialize(&report_);
  EXPECT_EQ(0, report_.extension_install_reports_size());
}

// Populate and store a log. Load the log. Verify that that the log contents are
// loaded correctly. Then, delete the log file. Store the log. Verify that no
// log file is created.
TEST_F(ExtensionInstallEventLogTest, StoreLoadAndStore) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(kFirstExtensionId, event);
  }
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i + 10);
    log_->Add(kSecondExtensionId, event);
  }

  log_->Store();

  ExtensionInstallEventLog log(file_name_);
  EXPECT_EQ(20, log.total_size());
  EXPECT_EQ(10, log.max_size());

  log.Serialize(&report_);
  VerifyTenLogEntriesEach(0 /* first_extension_timestamp_offset */,
                          10 /* second_extension_timestamp_offset*/);

  EXPECT_TRUE(base::DeleteFile(file_name_));

  log.Store();
  EXPECT_FALSE(base::PathExists(file_name_));
}

// Populate and serialize a log. Store the log. Load the log. Clear serialized
// entries in the loaded log. Verify that no entries are removed.
TEST_F(ExtensionInstallEventLogTest, SerializeStoreLoadAndClear) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(kFirstExtensionId, event);
  }

  log_->Serialize(&report_);

  log_->Store();

  ExtensionInstallEventLog log(file_name_);
  EXPECT_EQ(10, log.total_size());
  EXPECT_EQ(10, log.max_size());

  log.ClearSerialized();
  EXPECT_EQ(10, log.total_size());
  EXPECT_EQ(10, log.max_size());

  report_.Clear();
  log.Serialize(&report_);
  ASSERT_EQ(1, report_.extension_install_reports_size());
  const em::ExtensionInstallReport& extension_log =
      report_.extension_install_reports(0);
  EXPECT_EQ(kFirstExtensionId, extension_log.extension_id());
  ASSERT_EQ(10, extension_log.logs_size());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(i, extension_log.logs(i).timestamp());
  }
}

// Populate and serialize a log. Store the log. Change the version in the log
// file. Load the log. Verify that the log contents are not loaded.
TEST_F(ExtensionInstallEventLogTest, LoadVersionMismatch) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(kFirstExtensionId, event);
  }

  log_->Store();

  std::unique_ptr<base::File> file = std::make_unique<base::File>(
      file_name_,
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_WRITE);
  int64_t version;
  EXPECT_EQ(static_cast<ssize_t>(sizeof(version)),
            file->Read(0, reinterpret_cast<char*>(&version), sizeof(version)));
  --version;
  EXPECT_EQ(
      static_cast<ssize_t>(sizeof(version)),
      file->Write(0, reinterpret_cast<const char*>(&version), sizeof(version)));
  file.reset();

  ExtensionInstallEventLog log(file_name_);
  EXPECT_EQ(0, log.total_size());
  EXPECT_EQ(0, log.max_size());

  log.Serialize(&report_);
  EXPECT_EQ(0, report_.extension_install_reports_size());
}

// Add 10 log entries each for two extensions. Store the log. Truncate the file
// to the length of a log containing 10 log entries for one extension plus one
// byte. Load the log. Verify that the log contains 10 logs for one extension.
TEST_F(ExtensionInstallEventLogTest, LoadTruncated) {
  em::ExtensionInstallReportLogEvent event;
  event.set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(kFirstExtensionId, event);
  }

  log_->Store();

  std::unique_ptr<base::File> file = std::make_unique<base::File>(
      file_name_, base::File::FLAG_OPEN | base::File::FLAG_READ);
  const ssize_t size = file->GetLength();
  file.reset();

  for (int i = 0; i < 10; ++i) {
    event.set_timestamp(i);
    log_->Add(kSecondExtensionId, event);
  }

  file = std::make_unique<base::File>(
      file_name_, base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  file->SetLength(size + 1);
  file.reset();

  ExtensionInstallEventLog log(file_name_);
  EXPECT_EQ(10, log.total_size());
  EXPECT_EQ(10, log.max_size());

  log.Serialize(&report_);
  ASSERT_EQ(1, report_.extension_install_reports_size());
  const std::string& extension_id_name =
      report_.extension_install_reports(0).extension_id();
  ASSERT_TRUE(extension_id_name == kFirstExtensionId ||
              extension_id_name == kSecondExtensionId);

  const em::ExtensionInstallReport& extension_log =
      report_.extension_install_reports(0);
  EXPECT_EQ(extension_id_name, extension_log.extension_id());
  ASSERT_EQ(10, extension_log.logs_size());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(i, extension_log.logs(i).timestamp());
  }
}

}  // namespace policy
