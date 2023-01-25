// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_file_processor.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

constexpr uint32_t kMaxFilesToProcess = 50;
// Max file size: 100 MB
constexpr int64_t kMaxFileSizeBytes = 100 * 1024;

constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionSubDir[] = "folder";
constexpr char kManifestFile[] = "manifest.json";
constexpr char kJavaScriptFile1[] = "js_file_1.js";
constexpr char kJavaScriptFile2[] = "js_file_2.js";
constexpr char kHTMLFile1[] = "html_file_1.html";
constexpr char kHTMLFile2[] = "html_file_2.html";
constexpr char kCSSFile1[] = "css_file_1.css";
constexpr char kCSSFile2[] = "css_file_2.css";
constexpr char kExtensionSubDirHTMLFile1[] = "folder/html_file_1.html";
constexpr char kExtensionSubDirHTMLFile2[] = "folder/html_file_2.html";
constexpr char kExtensionSubDirCSSFile1[] = "folder/css_file_1.css";
constexpr char kExtensionSubDirCSSFile2[] = "folder/css_file_2.css";

std::string HashContent(const std::string& content) {
  std::string hash = crypto::SHA256HashString(content);
  return base::HexEncode(hash.c_str(), hash.size());
}

void WriteExtensionFile(const base::FilePath& path,
                        const std::string& file_name,
                        const std::string& content) {
  ASSERT_TRUE(base::WriteFile(path.AppendASCII(file_name), content.data(),
                              static_cast<int>(content.size())));
}

void WriteEmptyFile(const base::FilePath& path, const std::string& file_name) {
  base::FilePath file_path = path.AppendASCII(file_name);
  base::WriteFile(file_path, nullptr, 0);

  int64_t file_size;
  EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
  ASSERT_EQ(file_size, 0);
}

class ExtensionTelemetryFileProcessorTest : public ::testing::Test {
 public:
  ExtensionTelemetryFileProcessorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    // Set up temp directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    LOG(INFO) << "Setting up tmp extension directory.";

    // Set up dir structure for extension:
    // |- folder
    //     |- html_file_1.html
    //     |- html_file_2.html
    //     |- css_file_1.css
    //     |- css_file_2.css
    // |- manifest.json
    // |- js_file_1.js
    // |- js_file_2.js

    ext_root_dir_ = temp_dir_.GetPath().AppendASCII(kExtensionId);

    ASSERT_TRUE(base::CreateDirectory(ext_root_dir_));
    WriteExtensionFile(ext_root_dir_, kManifestFile, kManifestFile);
    WriteExtensionFile(ext_root_dir_, kJavaScriptFile1, kJavaScriptFile1);
    WriteExtensionFile(ext_root_dir_, kJavaScriptFile2, kJavaScriptFile2);

    ext_sub_dir_ = ext_root_dir_.AppendASCII(kExtensionSubDir);
    ASSERT_TRUE(base::CreateDirectory(ext_sub_dir_));
    WriteExtensionFile(ext_sub_dir_, kHTMLFile1, kHTMLFile1);
    WriteExtensionFile(ext_sub_dir_, kHTMLFile2, kHTMLFile2);
    WriteExtensionFile(ext_sub_dir_, kCSSFile1, kCSSFile1);
    WriteExtensionFile(ext_sub_dir_, kCSSFile2, kCSSFile2);

    processor_ = base::SequenceBound<ExtensionTelemetryFileProcessor>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
        kMaxFilesToProcess, kMaxFileSizeBytes, ext_root_dir_);
    task_environment_.RunUntilIdle();
  }

  void InitializeProcessor(size_t max_files_to_process, int64_t max_file_size) {
    processor_ = base::SequenceBound<ExtensionTelemetryFileProcessor>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
        max_files_to_process, max_file_size, ext_root_dir_);
    task_environment_.RunUntilIdle();
  }

  void CallbackHelper(base::Value::Dict data) {
    extensions_data_ = std::move(data);
  }

  void TearDown() override {
    processor_.SynchronouslyResetForTest();
    testing::Test::TearDown();
    EXPECT_TRUE(temp_dir_.Delete());
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath ext_root_dir_;
  base::FilePath ext_sub_dir_;

  base::SequenceBound<safe_browsing::ExtensionTelemetryFileProcessor>
      processor_;
  content::BrowserTaskEnvironment task_environment_;
  base::Value::Dict extensions_data_;
  base::WeakPtrFactory<ExtensionTelemetryFileProcessorTest> weak_factory_{this};
};

TEST_F(ExtensionTelemetryFileProcessorTest, ProcessesExtension) {
  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());

  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  base::Value::Dict expected_dict;
  expected_dict.Set(kManifestFile, kManifestFile);
  expected_dict.Set(kJavaScriptFile1, HashContent(kJavaScriptFile1));
  expected_dict.Set(kJavaScriptFile2, HashContent(kJavaScriptFile2));
  expected_dict.Set(kExtensionSubDirHTMLFile1, HashContent(kHTMLFile1));
  expected_dict.Set(kExtensionSubDirHTMLFile2, HashContent(kHTMLFile2));
  expected_dict.Set(kExtensionSubDirCSSFile1, HashContent(kCSSFile1));
  expected_dict.Set(kExtensionSubDirCSSFile2, HashContent(kCSSFile2));

  EXPECT_EQ(extensions_data_, expected_dict);
}

TEST_F(ExtensionTelemetryFileProcessorTest,
       ProcessesSameFilenamesButDifferentPaths) {
  // Add ext_root_dir/html_file_1.html file
  WriteExtensionFile(ext_root_dir_, kHTMLFile1, kHTMLFile1);

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  base::Value::Dict expected_dict;
  expected_dict.Set(kManifestFile, kManifestFile);
  expected_dict.Set(kJavaScriptFile1, HashContent(kJavaScriptFile1));
  expected_dict.Set(kJavaScriptFile2, HashContent(kJavaScriptFile2));
  expected_dict.Set(kHTMLFile1, HashContent(kHTMLFile1));
  expected_dict.Set(kExtensionSubDirHTMLFile1, HashContent(kHTMLFile1));
  expected_dict.Set(kExtensionSubDirHTMLFile2, HashContent(kHTMLFile2));
  expected_dict.Set(kExtensionSubDirCSSFile1, HashContent(kCSSFile1));
  expected_dict.Set(kExtensionSubDirCSSFile2, HashContent(kCSSFile2));

  EXPECT_EQ(extensions_data_, expected_dict);
}

TEST_F(ExtensionTelemetryFileProcessorTest, EnforcesMaxNumFilesLimit) {
  // Set max_files_to_process to 4.
  InitializeProcessor(/*max_files_to_process=*/4, kMaxFileSizeBytes);

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  // JS/HTML type prioritized.
  base::Value::Dict expected_dict;
  expected_dict.Set(kManifestFile, kManifestFile);
  expected_dict.Set(kJavaScriptFile1, HashContent(kJavaScriptFile1));
  expected_dict.Set(kJavaScriptFile2, HashContent(kJavaScriptFile2));
  expected_dict.Set(kExtensionSubDirHTMLFile1, HashContent(kHTMLFile1));
  expected_dict.Set(kExtensionSubDirHTMLFile2, HashContent(kHTMLFile2));

  EXPECT_EQ(extensions_data_, expected_dict);
}

TEST_F(ExtensionTelemetryFileProcessorTest, EnforcesMaxFileSizeLimit) {
  // Add in file over size limit.
  WriteExtensionFile(
      ext_root_dir_, "over_sized_file.js",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

  // Set max_file_size to 50 bytes.
  int64_t max_file_size = 50;
  InitializeProcessor(kMaxFilesToProcess, /*max_file_size=*/50);

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  int64_t file_size;
  EXPECT_TRUE(base::GetFileSize(ext_root_dir_.AppendASCII("over_sized_file.js"),
                                &file_size));
  ASSERT_GT(file_size, max_file_size);

  base::Value::Dict expected_dict;
  expected_dict.Set(kManifestFile, kManifestFile);
  expected_dict.Set(kJavaScriptFile1, HashContent(kJavaScriptFile1));
  expected_dict.Set(kJavaScriptFile2, HashContent(kJavaScriptFile2));
  expected_dict.Set(kExtensionSubDirHTMLFile1, HashContent(kHTMLFile1));
  expected_dict.Set(kExtensionSubDirHTMLFile2, HashContent(kHTMLFile2));
  expected_dict.Set(kExtensionSubDirCSSFile1, HashContent(kCSSFile1));
  expected_dict.Set(kExtensionSubDirCSSFile2, HashContent(kCSSFile2));

  EXPECT_EQ(extensions_data_, expected_dict);
}

TEST_F(ExtensionTelemetryFileProcessorTest, IgnoresEmptyFiles) {
  WriteEmptyFile(ext_root_dir_, "empty_file_1.js");
  WriteEmptyFile(ext_root_dir_, "empty_file_2.js");

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  base::Value::Dict expected_dict;
  expected_dict.Set(kManifestFile, kManifestFile);
  expected_dict.Set(kJavaScriptFile1, HashContent(kJavaScriptFile1));
  expected_dict.Set(kJavaScriptFile2, HashContent(kJavaScriptFile2));
  expected_dict.Set(kExtensionSubDirHTMLFile1, HashContent(kHTMLFile1));
  expected_dict.Set(kExtensionSubDirHTMLFile2, HashContent(kHTMLFile2));
  expected_dict.Set(kExtensionSubDirCSSFile1, HashContent(kCSSFile1));
  expected_dict.Set(kExtensionSubDirCSSFile2, HashContent(kCSSFile2));

  EXPECT_EQ(extensions_data_, expected_dict);
}

TEST_F(ExtensionTelemetryFileProcessorTest, IgnoresOtherFileTypes) {
  WriteExtensionFile(ext_root_dir_, "file.txt", "file.txt");
  WriteExtensionFile(ext_root_dir_, "file.json", "file.json");

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  base::Value::Dict expected_dict;
  expected_dict.Set(kManifestFile, kManifestFile);
  expected_dict.Set(kJavaScriptFile1, HashContent(kJavaScriptFile1));
  expected_dict.Set(kJavaScriptFile2, HashContent(kJavaScriptFile2));
  expected_dict.Set(kExtensionSubDirHTMLFile1, HashContent(kHTMLFile1));
  expected_dict.Set(kExtensionSubDirHTMLFile2, HashContent(kHTMLFile2));
  expected_dict.Set(kExtensionSubDirCSSFile1, HashContent(kCSSFile1));
  expected_dict.Set(kExtensionSubDirCSSFile2, HashContent(kCSSFile2));

  EXPECT_EQ(extensions_data_, expected_dict);
}

TEST_F(ExtensionTelemetryFileProcessorTest, EnforcesMaxFilesToReadLimit) {
  // Set max_file_read limit to 3
  processor_
      .AsyncCall(&ExtensionTelemetryFileProcessor::SetMaxFilesToReadForTest)
      .WithArgs(3);

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  // Only 3 files are read.
  base::Value::Dict expected_dict;
  expected_dict.Set(kManifestFile, kManifestFile);
  expected_dict.Set(kJavaScriptFile1, HashContent(kJavaScriptFile1));
  expected_dict.Set(kJavaScriptFile2, HashContent(kJavaScriptFile2));

  EXPECT_EQ(extensions_data_, expected_dict);
}

}  // namespace safe_browsing
