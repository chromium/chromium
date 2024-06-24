// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/artemis/log_source.h"

#include <filesystem>
#include <fstream>
#include <map>

#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/log_file.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/persistent_db.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cfm {
namespace {

constexpr size_t kTestFileNumLines = 10;
constexpr int kLargeOffset = 100000;

// We aren't actually polling, so this value doesn't matter.
constexpr base::TimeDelta kDefaultPollFrequency = base::Seconds(0);

// Default to reading all lines in test file.
constexpr size_t kDefaultBatchSize = kTestFileNumLines;

// Define a barebones db here that uses a std::map as the backing.
class PersistentDbForTesting : public PersistentDb {
 public:
  PersistentDbForTesting() = default;
  PersistentDbForTesting(const PersistentDbForTesting&) = delete;
  PersistentDbForTesting& operator=(const PersistentDbForTesting&) = delete;
  ~PersistentDbForTesting() override = default;

  // PersistentDb:
  int GetValueFromKey(int key, int default_value) override {
    if (map_.count(key) == 0) {
      return default_value;
    }
    return map_[key];
  }

  void SaveValueToKey(int key, int value) override { map_[key] = value; }

  void DeleteKeyIfExists(int key) override {
    if (map_.count(key) != 0) {
      map_.erase(key);
    }
  }

  size_t GetSize() const override { return map_.size(); }

 private:
  std::map<int, int> map_;
};

// Define a testing LogSource that allows us to fill the data buffer
// at will. Note there is no call to StartPollTimer().
class LogSourceForTesting : public LogSource {
 public:
  LogSourceForTesting(const std::string& filepath,
                      base::TimeDelta poll_rate,
                      size_t batch_size)
      : LogSource(filepath, poll_rate, batch_size) {}
  LogSourceForTesting(const LogSourceForTesting&) = delete;
  LogSourceForTesting& operator=(const LogSourceForTesting&) = delete;
  ~LogSourceForTesting() override = default;

  void FillDataBufferForTesting() { FillDataBuffer(); }

 protected:
  // Override this and avoid serialization as it greatly complicates testing
  void SerializeDataBuffer(std::vector<std::string>& buffer) override {}
};

// Define test fixture. This fixture will be used for both the
// LogFile and LogSource objects as they are closely linked and
// require similar setup. Each test will create real (temporary)
// files on the filesystem, to be used as the underlying data
// sources.
class ArtemisLogSourceTest : public testing::Test {
 public:
  ArtemisLogSourceTest()
      : test_file_("test_file.log"),
        rotated_file_("test_file.log.1"),
        rotate_log_prefix_("ROTATE: ") {}
  ArtemisLogSourceTest(const ArtemisLogSourceTest&) = delete;
  ArtemisLogSourceTest& operator=(const ArtemisLogSourceTest&) = delete;

  void SetUp() override {
    test_db_ = std::make_unique<PersistentDbForTesting>();
    PersistentDb::InitializeForTesting(test_db_.get());
    AppendNewLines(test_file_, kTestFileNumLines, "");
  }

  void TearDown() override {
    std::filesystem::remove(test_file_);
    if (std::filesystem::exists(rotated_file_)) {
      std::filesystem::remove(rotated_file_);
    }
    test_db_.reset(nullptr);
    PersistentDb::ShutdownForTesting();
  }

  void RotateFile() {
    std::filesystem::rename(test_file_, rotated_file_);
    AppendNewLines(test_file_, kTestFileNumLines, rotate_log_prefix_);
  }

  void AppendNewLines(const std::string& filename,
                      size_t count,
                      const std::string& prefix) {
    std::ofstream test_file;
    test_file.open(filename, std::ios_base::app);
    for (unsigned int i = 0; i < count; i++) {
      test_file << prefix << i << std::endl;
    }
    test_file.close();
  }

  int GetFileSize() {
    std::ifstream tmp_stream(test_file_);
    tmp_stream.seekg(0, tmp_stream.end);
    return (int)tmp_stream.tellg();
  }

 protected:
  const std::string test_file_;
  const std::string rotated_file_;
  const std::string rotate_log_prefix_;

 private:
  std::unique_ptr<PersistentDb> test_db_;
};

// ------- Start LogFile tests -------

TEST_F(ArtemisLogSourceTest, OpenFileAtVariousOffsets) {
  LogFile logfile_bad(test_file_ + "noexist");
  LogFile logfile(test_file_);

  // File doesn't exist
  EXPECT_FALSE(logfile_bad.OpenAtOffset(0));
  logfile_bad.CloseStream();

  // File opens and is accessible
  EXPECT_TRUE(logfile.OpenAtOffset(0));
  logfile.CloseStream();

  // File opens and can seek to a reasonable offset
  EXPECT_TRUE(logfile.OpenAtOffset(8));
  logfile.CloseStream();

  // Negative offsets are not invalid. File will seek
  // to end, minus the offset.
  EXPECT_TRUE(logfile.OpenAtOffset(-1));
  logfile.CloseStream();

  // Too-high offset is not invalid. File will seek to end.
  EXPECT_TRUE(logfile.OpenAtOffset(kLargeOffset));
  logfile.CloseStream();
}

TEST_F(ArtemisLogSourceTest, CheckEOFStateAfterVariousOpens) {
  LogFile logfile(test_file_);
  int file_size = GetFileSize();

  // File opened at beginning
  logfile.OpenAtOffset(0);
  EXPECT_FALSE(logfile.IsAtEOF());

  // File opened at the end yields EOF
  logfile.OpenAtOffset(file_size);
  logfile.RetrieveNextLogs(1);
  EXPECT_TRUE(logfile.IsAtEOF());

  // File opened at high offset yields EOF
  logfile.OpenAtOffset(kLargeOffset);
  logfile.RetrieveNextLogs(1);
  EXPECT_TRUE(logfile.IsAtEOF());

  // File opened just near end yields no EOF
  logfile.OpenAtOffset(file_size - 1);
  EXPECT_FALSE(logfile.IsAtEOF());
}

TEST_F(ArtemisLogSourceTest, RequestVaryingAmountOfLogLines) {
  LogFile logfile(test_file_);
  logfile.OpenAtOffset(0);

  // Read all logs. Note EOF is expected to be false until
  // we try another read, so expect false.
  auto lines = logfile.RetrieveNextLogs(kTestFileNumLines);
  EXPECT_EQ(lines.size(), kTestFileNumLines);
  EXPECT_FALSE(logfile.IsAtEOF());
  logfile.CloseStream();

  // Try to read more than all logs. Verify result is the same,
  // but this time with EOF == true.
  logfile.OpenAtOffset(0);
  lines = logfile.RetrieveNextLogs(kTestFileNumLines + 1);
  EXPECT_EQ(lines.size(), kTestFileNumLines);
  EXPECT_TRUE(logfile.IsAtEOF());
  logfile.CloseStream();

  // Verify partial reads
  size_t num_to_read = 3;
  logfile.OpenAtOffset(0);
  lines = logfile.RetrieveNextLogs(num_to_read);
  EXPECT_EQ(lines.size(), num_to_read);
  lines = logfile.RetrieveNextLogs(kTestFileNumLines - num_to_read);
  EXPECT_EQ(lines.size(), kTestFileNumLines - num_to_read);
  logfile.CloseStream();
}

TEST_F(ArtemisLogSourceTest, VerifyNewLinesAppearAfterRefresh) {
  LogFile logfile(test_file_);
  logfile.OpenAtOffset(0);

  // Note for the below tests: there are two cases we need to
  // consider:
  // 1. The case where we "exhaust" the data source by attempting
  //    to read more than what's available, which triggers an EOF
  //    and requires a subsequent Refresh().
  // 2. The case where we read exactly (or less than) the amount
  //    of data in the log file and do NOT hit an EOF. New lines
  //    will be available immediately after adding them and will
  //    not require a Refresh() to observe them.
  //
  // This is likely a side effect of how EOFs are handled in the
  // underlying C++ filesystem API.

  // Exhaust all the lines in the file, then add more. This is
  // case #1 above.
  logfile.RetrieveNextLogs(kTestFileNumLines + 1);
  EXPECT_TRUE(logfile.IsAtEOF());
  AppendNewLines(test_file_, kTestFileNumLines, "NEW: ");

  // Verify no new lines are reported before Refresh()
  auto new_lines = logfile.RetrieveNextLogs(kTestFileNumLines);
  EXPECT_EQ(new_lines.size(), 0u);
  EXPECT_TRUE(logfile.IsAtEOF());

  // Verify Refresh() triggers new lines to appear
  logfile.Refresh();
  EXPECT_FALSE(logfile.IsAtEOF());
  new_lines = logfile.RetrieveNextLogs(kTestFileNumLines);
  EXPECT_EQ(new_lines.size(), kTestFileNumLines);

  // Verify that the lines are the new lines
  for (auto& line : new_lines) {
    EXPECT_TRUE(line.starts_with("NEW: "));
  }

  // We read the exact log count in the previous operation, so EOF
  // should be false and newly appended lines should be available
  // immediately. Add more lines and test case #2 above.
  EXPECT_FALSE(logfile.IsAtEOF());
  AppendNewLines(test_file_, kTestFileNumLines, "NEW2: ");

  // Verify lines are immediately observable.
  new_lines = logfile.RetrieveNextLogs(kTestFileNumLines);
  EXPECT_EQ(new_lines.size(), kTestFileNumLines);
  EXPECT_FALSE(logfile.IsAtEOF());

  // Verify that the lines are the new lines
  for (auto& line : new_lines) {
    EXPECT_TRUE(line.starts_with("NEW2: "));
  }
}

// ------- Start LogSource tests -------

TEST_F(ArtemisLogSourceTest, TestBatchSizeCorrectlyLimitsOutput) {
  const size_t batch_size = 2;
  const size_t expected_num_reads = kTestFileNumLines / batch_size;

  auto log_source = LogSource(test_file_, kDefaultPollFrequency, batch_size);

  for (size_t i = 0; i < expected_num_reads; ++i) {
    auto data = log_source.GetNextData();
    EXPECT_EQ(data.size(), batch_size);
  }

  auto data = log_source.GetNextData();
  EXPECT_EQ(data.size(), 0u);
}

TEST_F(ArtemisLogSourceTest, VerifyNewLinesAppearAfterRotation) {
  auto log_source =
      LogSource(test_file_, kDefaultPollFrequency, kDefaultBatchSize);

  // Initial setup. Read everything from original file
  auto data = log_source.GetNextData();
  EXPECT_EQ(data.size(), kTestFileNumLines);
  data = log_source.GetNextData();
  EXPECT_EQ(data.size(), 0u);

  // Add more lines to original file
  AppendNewLines(test_file_, kTestFileNumLines, "NEW: ");

  // Rotate file and verify that next fetch returns lines from new file
  RotateFile();
  data = log_source.GetNextData();
  EXPECT_EQ(data.size(), kTestFileNumLines);
  for (auto& line : data) {
    // TODO(b/320996557): we are expecting the new lines (NEW: ...) from
    // the old file to be dropped here. This will be the case until we
    // add full rotation support.
    EXPECT_TRUE(line.starts_with(rotate_log_prefix_));
  }
}

TEST_F(ArtemisLogSourceTest, TestCrashRecovery) {
  base::test::TaskEnvironment task_environment;
  base::RunLoop run_loop;

  size_t batch_size = 2;
  auto log_source = std::make_unique<LogSourceForTesting>(
      test_file_, kDefaultPollFrequency, batch_size);

  // Add <batch_size> lines to internal buffer. Then fetch & drop.
  log_source->FillDataBufferForTesting();
  log_source->Fetch(base::DoNothing());
  run_loop.RunUntilIdle();

  // We haven't flushed yet, so the db should be empty.
  EXPECT_EQ(PersistentDb::Get()->GetSize(), 0u);

  // Flush to report success. Verify inode was cached.
  log_source->Flush();
  run_loop.RunUntilIdle();
  EXPECT_EQ(PersistentDb::Get()->GetSize(), 1u);

  // Tear down and reset the log source. Then add more data.
  log_source.reset(nullptr);
  log_source = std::make_unique<LogSourceForTesting>(
      test_file_, kDefaultPollFrequency, batch_size);
  log_source->FillDataBufferForTesting();

  // Run the next Fetch and expect the data to be continued from the last
  // point. Note that the file we're examining is just filled with integers,
  // 0 to kTestFileNumLines, so we expect to start at integer <batch_size>.
  log_source->Fetch(base::BindOnce(
      [](size_t start, const std::vector<std::string>& results) {
        EXPECT_EQ(results[0], base::NumberToString(start));
        EXPECT_EQ(results[1], base::NumberToString(start + 1));
      },
      batch_size));
  run_loop.RunUntilIdle();

  // Explicitly do not Flush()! Tear down and reset again. Add more data.
  log_source.reset(nullptr);
  log_source = std::make_unique<LogSourceForTesting>(
      test_file_, kDefaultPollFrequency, batch_size);
  log_source->FillDataBufferForTesting();

  // Because Flush() was not called, we assume that the last attempt failed,
  // so make sure we start from the same recovery location.
  log_source->Fetch(base::BindOnce(
      [](size_t start, const std::vector<std::string>& results) {
        ASSERT_EQ(results.size(), 2u);
        EXPECT_EQ(results[0], base::NumberToString(start));
        EXPECT_EQ(results[1], base::NumberToString(start + 1));
      },
      batch_size));
  run_loop.RunUntilIdle();

  // Tear down and reset again.
  log_source.reset(nullptr);
  log_source = std::make_unique<LogSourceForTesting>(
      test_file_, kDefaultPollFrequency, batch_size);

  // This time, before adding data, rotate the file.
  RotateFile();
  log_source->FillDataBufferForTesting();

  // Expect that we are now starting from the beginning of the rotated file.
  log_source->Fetch(base::BindOnce(
      [](size_t start, const std::string& prefix,
         const std::vector<std::string>& results) {
        ASSERT_EQ(results.size(), 2u);
        EXPECT_EQ(results[0], prefix + base::NumberToString(start));
        EXPECT_EQ(results[1], prefix + base::NumberToString(start + 1));
      },
      0u, rotate_log_prefix_));
  log_source->Flush();
  run_loop.RunUntilIdle();

  // Expect that the old inode was deleted and replaced with the new one.
  EXPECT_EQ(PersistentDb::Get()->GetSize(), 1u);
}

}  // namespace
}  // namespace ash::cfm
