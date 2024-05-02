// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/log_source.h"

#include <filesystem>
#include <fstream>

#include "base/time/time.h"
#include "chrome/browser/ash/chromebox_for_meetings/hotlog2/log_file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::cfm {
namespace {

constexpr std::string kTestFileName = "test_file.log";
constexpr std::string kRotatedTestFileName = "test_file.log.1";

constexpr size_t kTestFileNumLines = 10;
constexpr int kLargeOffset = 100000;

// We aren't actually polling, so this value doesn't matter.
constexpr base::TimeDelta kDefaultPollFrequency = base::Seconds(0);

// Default to reading all lines in test file.
constexpr size_t kDefaultBatchSize = kTestFileNumLines;

// Define test fixture. This fixture will be used for both the
// LogFile and LogSource objects as they are closely linked and
// require similar setup. Each test will create real (temporary)
// files on the filesystem, to be used as the underlying data
// sources.
class HotlogLogSourceTest : public testing::Test {
 public:
  HotlogLogSourceTest() {}
  HotlogLogSourceTest(const HotlogLogSourceTest&) = delete;
  HotlogLogSourceTest& operator=(const HotlogLogSourceTest&) = delete;

  void SetUp() override {
    AppendNewLines(kTestFileName, kTestFileNumLines, "");
  }

  void TearDown() override {
    std::filesystem::remove(kTestFileName);
    if (std::filesystem::exists(kRotatedTestFileName)) {
      std::filesystem::remove(kRotatedTestFileName);
    }
  }

  void RotateFile() {
    std::filesystem::rename(kTestFileName, kRotatedTestFileName);
    AppendNewLines(kTestFileName, kTestFileNumLines, "ROTATED: ");
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
    std::ifstream tmp_stream(kTestFileName);
    tmp_stream.seekg(0, tmp_stream.end);
    return (int)tmp_stream.tellg();
  }
};

// ------- Start LogFile tests -------

TEST_F(HotlogLogSourceTest, OpenFileAtVariousOffsets) {
  LogFile logfile_bad(kTestFileName + "noexist");
  LogFile logfile(kTestFileName);

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

TEST_F(HotlogLogSourceTest, CheckEOFStateAfterVariousOpens) {
  LogFile logfile(kTestFileName);
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

TEST_F(HotlogLogSourceTest, RequestVaryingAmountOfLogLines) {
  LogFile logfile(kTestFileName);
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

TEST_F(HotlogLogSourceTest, VerifyNewLinesAppearAfterRefresh) {
  LogFile logfile(kTestFileName);
  logfile.OpenAtOffset(0);

  // Consume all the lines in the file, then add more
  logfile.RetrieveNextLogs(kTestFileNumLines);
  AppendNewLines(kTestFileName, kTestFileNumLines, "NEW: ");

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
}

// ------- Start LogSource tests -------

TEST_F(HotlogLogSourceTest, TestBatchSizeCorrectlyLimitsOutput) {
  const size_t batch_size = 2;
  const size_t expected_num_reads = kTestFileNumLines / batch_size;

  auto log_source = LogSource(kTestFileName, kDefaultPollFrequency, batch_size);

  for (size_t i = 0; i < expected_num_reads; ++i) {
    auto data = log_source.GetNextData();
    EXPECT_EQ(data.size(), batch_size);
  }

  auto data = log_source.GetNextData();
  EXPECT_EQ(data.size(), 0u);
}

TEST_F(HotlogLogSourceTest, VerifyNewLinesAppearAfterRotation) {
  auto log_source =
      LogSource(kTestFileName, kDefaultPollFrequency, kDefaultBatchSize);

  // Initial setup. Read everything from original file
  auto data = log_source.GetNextData();
  EXPECT_EQ(data.size(), kTestFileNumLines);
  data = log_source.GetNextData();
  EXPECT_EQ(data.size(), 0u);

  // Add more lines to original file
  AppendNewLines(kTestFileName, kTestFileNumLines, "NEW: ");

  // Rotate file and verify that next fetch returns lines from new file
  RotateFile();
  data = log_source.GetNextData();
  EXPECT_EQ(data.size(), kTestFileNumLines);
  for (auto& line : data) {
    // TODO(b/320996557): we are expecting the new lines (NEW: ...) from
    // the old file to be dropped here. This will be the case until we
    // add full rotation support.
    EXPECT_TRUE(line.starts_with("ROTATED: "));
  }
}

}  // namespace
}  // namespace ash::cfm
