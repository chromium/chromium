// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list/crash_upload_list_chromeos.h"

#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using FatalCrashType = CrashUploadListChromeOS::CrashUploadInfo::FatalCrashType;

// Input to the fatal_crash_type field and the expected parsed FatalCrashType. A
// missing input implies that the fatal_crash_type field is missing.
struct InputExpectedPair {
  std::optional<std::string> input;
  FatalCrashType expected;
};

static const InputExpectedPair kKernelPair = {
    .input = "kernel",
    .expected = FatalCrashType::Kernel};

static const InputExpectedPair kEcPair = {
    .input = "ec",
    .expected = FatalCrashType::EmbeddedController};

static const InputExpectedPair kMissingPair = {
    .input = std::nullopt,
    .expected = FatalCrashType::Unknown};

// Unknown pair with a specified input.
const InputExpectedPair UnknownPair(std::string input) {
  return {.input = input, .expected = FatalCrashType::Unknown};
}

static const InputExpectedPair kEmptyPair = UnknownPair("");

class CrashUploadListChromeOSTest
    : public testing::TestWithParam<std::vector<InputExpectedPair>> {
 public:
  CrashUploadListChromeOSTest(const CrashUploadListChromeOSTest&) = delete;
  CrashUploadListChromeOSTest& operator=(const CrashUploadListChromeOSTest&) =
      delete;

 protected:
  CrashUploadListChromeOSTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void WriteUploadLog(const std::string& log_data) {
    ASSERT_TRUE(base::WriteFile(log_path(), log_data));
  }

  base::FilePath log_path() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("uploads.log"));
  }

  const std::vector<InputExpectedPair>& input_expected_pairs() const {
    return GetParam();
  }

 private:
  base::ScopedTempDir temp_dir_;

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_P(CrashUploadListChromeOSTest, ParseWithFatalCrashType_JSON) {
  const size_t num_items = input_expected_pairs().size();
  std::stringstream stream;
  for (size_t i = 0; i < num_items; ++i) {
    stream << "{";
    // uploads_id and local_id are parsed by TextLogUploadList. Keep them here
    // to track the order of entries and to ensure fields parsed by
    // TextLogUploadList are still correct.
    stream << "\"upload_id\":\"" << i << "\",";
    stream << "\"local_id\":\"" << num_items - i << "\"";
    const auto& fatal_crash_type = input_expected_pairs()[i].input;
    if (fatal_crash_type.has_value()) {
      stream << ",\"fatal_crash_type\":\"" << fatal_crash_type.value() << "\"";
    }
    stream << "}" << std::endl;
  }

  WriteUploadLog(stream.str());
  auto upload_list = MakeRefCounted<CrashUploadListChromeOS>(log_path());
  base::RunLoop run_loop;
  upload_list->Load(run_loop.QuitClosure());
  run_loop.Run();

  const std::vector<const UploadList::UploadInfo*> uploads =
      upload_list->GetUploads(999);
  EXPECT_EQ(num_items, uploads.size());

  // Indicate whether a particular JSON entry has been parsed.
  std::vector<bool> parsed(num_items, false);
  for (const UploadList::UploadInfo* upload : uploads) {
    uint64_t upload_id;
    ASSERT_TRUE(base::StringToUint64(upload->upload_id, &upload_id));
    // Ensures all elements in parsed to be eventually true after the loop: If
    // any element is assigned twice, it means that there's another element
    // being false.
    EXPECT_FALSE(parsed.at(upload_id))
        << "Entry with upload ID \"" << upload_id << "\" appears twice.";
    parsed.at(upload_id) = true;
    EXPECT_EQ(base::NumberToString(num_items - upload_id), upload->local_id);
    EXPECT_EQ(
        input_expected_pairs()[upload_id].expected,
        static_cast<const CrashUploadListChromeOS::CrashUploadInfo*>(upload)
            ->fatal_crash_type);
  }
}

INSTANTIATE_TEST_SUITE_P(
    CrashUploadListChromeOSInstantiation,
    CrashUploadListChromeOSTest,
    testing::Values(
        // Every JSON entry contains a valid fatal_crash_type field
        std::vector<InputExpectedPair>{kEcPair, kKernelPair, kEcPair},
        // No fatal_crash_type field is present
        std::vector<InputExpectedPair>{kMissingPair, kMissingPair},
        // Some JSON entries contain unknown fatal_crash_type field
        std::vector<InputExpectedPair>{UnknownPair("abc"), kKernelPair,
                                       UnknownPair("123")},
        // Some JSON entries contain empty fatal_crash_type field
        std::vector<InputExpectedPair>{kEmptyPair, kEcPair, kEmptyPair},
        // Hybrid scenario
        std::vector<InputExpectedPair>{kEmptyPair, kEcPair,
                                       UnknownPair("my-chrome"), kKernelPair,
                                       kMissingPair}));

}  // namespace
