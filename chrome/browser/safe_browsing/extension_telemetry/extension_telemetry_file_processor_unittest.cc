// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_file_processor.h"

#include <string_view>

#include "base/files/file_path.h"
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
  return base::HexEncode(crypto::SHA256HashString(content));
}

void WriteExtensionFile(const base::FilePath& path,
                        const std::string& file_name,
                        const std::string& content) {
  ASSERT_TRUE(base::WriteFile(path.AppendASCII(file_name), content));
}

void WriteEmptyFile(const base::FilePath& path, const std::string& file_name) {
  base::FilePath file_path = path.AppendASCII(file_name);
  base::WriteFile(file_path, std::string_view());

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

    extension_root_dir_ = temp_dir_.GetPath().AppendASCII(kExtensionId);
    ASSERT_TRUE(base::CreateDirectory(extension_root_dir_));

    processor_ = base::SequenceBound<ExtensionTelemetryFileProcessor>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
    task_environment_.RunUntilIdle();
  }

  void SetUpExtensionFiles() {
    // Set up dir structure for extension:
    // |- folder
    //     |- html_file_1.html
    //     |- html_file_2.html
    //     |- css_file_1.css
    //     |- css_file_2.css
    // |- manifest.json
    // |- js_file_1.js
    // |- js_file_2.js

    WriteExtensionFile(extension_root_dir_, kManifestFile, kManifestFile);
    WriteExtensionFile(extension_root_dir_, kJavaScriptFile1, kJavaScriptFile1);
    WriteExtensionFile(extension_root_dir_, kJavaScriptFile2, kJavaScriptFile2);

    extension_sub_dir_ = extension_root_dir_.AppendASCII(kExtensionSubDir);
    ASSERT_TRUE(base::CreateDirectory(extension_sub_dir_));
    WriteExtensionFile(extension_sub_dir_, kHTMLFile1, kHTMLFile1);
    WriteExtensionFile(extension_sub_dir_, kHTMLFile2, kHTMLFile2);
    WriteExtensionFile(extension_sub_dir_, kCSSFile1, kCSSFile1);
    WriteExtensionFile(extension_sub_dir_, kCSSFile2, kCSSFile2);
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
  base::FilePath extension_root_dir_;
  base::FilePath extension_sub_dir_;

  base::SequenceBound<safe_browsing::ExtensionTelemetryFileProcessor>
      processor_;
  content::BrowserTaskEnvironment task_environment_;
  base::Value::Dict extensions_data_;
  base::WeakPtrFactory<ExtensionTelemetryFileProcessorTest> weak_factory_{this};
};

TEST_F(ExtensionTelemetryFileProcessorTest, ProcessesExtension) {
  SetUpExtensionFiles();
  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());

  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(extension_root_dir_)
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
       IgnoresExtensionWithInvalidRootDirectory) {
  // Empty root path
  base::FilePath empty_root;

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(empty_root)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  base::Value::Dict expected_dict;
  EXPECT_EQ(extensions_data_, expected_dict);
}

TEST_F(ExtensionTelemetryFileProcessorTest,
       IgnoresExtensionWithMissingManifestFile) {
  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(extension_root_dir_)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  base::Value::Dict expected_dict;
  EXPECT_EQ(extensions_data_, expected_dict);
}

TEST_F(ExtensionTelemetryFileProcessorTest,
       IgnoresExtensionWithEmptyManifestFile) {
  WriteEmptyFile(extension_root_dir_, "manifest.json");

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(extension_root_dir_)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  base::Value::Dict expected_dict;
  EXPECT_EQ(extensions_data_, expected_dict);
}

TEST_F(ExtensionTelemetryFileProcessorTest,
       ProcessesSameFilenamesButDifferentPaths) {
  SetUpExtensionFiles();
  // Add extension_root_dir/html_file_1.html file
  WriteExtensionFile(extension_root_dir_, kHTMLFile1, kHTMLFile1);

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(extension_root_dir_)
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

TEST_F(ExtensionTelemetryFileProcessorTest, IgnoresEmptyFiles) {
  SetUpExtensionFiles();
  WriteEmptyFile(extension_root_dir_, "empty_file_1.js");
  WriteEmptyFile(extension_root_dir_, "empty_file_2.js");

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(extension_root_dir_)
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

TEST_F(ExtensionTelemetryFileProcessorTest, IgnoresUnapplicableFiles) {
  SetUpExtensionFiles();
  WriteExtensionFile(extension_root_dir_, "file.txt", "file.txt");
  WriteExtensionFile(extension_root_dir_, "file.json", "file.json");
  WriteExtensionFile(extension_root_dir_, "file", "file");

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(extension_root_dir_)
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
  SetUpExtensionFiles();
  // Set max_file_read limit to 3
  processor_
      .AsyncCall(&ExtensionTelemetryFileProcessor::SetMaxFilesToReadForTest)
      .WithArgs(3);

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(extension_root_dir_)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  // Only 3 files are read.
  base::Value::Dict expected_dict;
  expected_dict.Set(kManifestFile, kManifestFile);
  expected_dict.Set(kJavaScriptFile1, HashContent(kJavaScriptFile1));
  expected_dict.Set(kJavaScriptFile2, HashContent(kJavaScriptFile2));

  EXPECT_EQ(extensions_data_, expected_dict);
}

TEST_F(ExtensionTelemetryFileProcessorTest, EnforcesMaxNumFilesLimit) {
  SetUpExtensionFiles();
  // Set max_files_to_process to 4.
  processor_
      .AsyncCall(&ExtensionTelemetryFileProcessor::SetMaxFilesToProcessForTest)
      .WithArgs(4);

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(extension_root_dir_)
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
  SetUpExtensionFiles();
  // Add in file over size limit.
  WriteExtensionFile(
      extension_root_dir_, "over_sized_file.js",
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");

  // Set max_file_size to 50 bytes.
  int64_t max_file_size = 50;
  processor_
      .AsyncCall(&ExtensionTelemetryFileProcessor::SetMaxFilesToProcessForTest)
      .WithArgs(50);
  processor_
      .AsyncCall(&ExtensionTelemetryFileProcessor::SetMaxFileSizeBytesForTest)
      .WithArgs(max_file_size);

  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());
  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(extension_root_dir_)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  int64_t file_size;
  EXPECT_TRUE(base::GetFileSize(
      extension_root_dir_.AppendASCII("over_sized_file.js"), &file_size));
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

TEST_F(ExtensionTelemetryFileProcessorTest,
       ProcessesUpperCaseFileExtensionsCorrectly) {
  WriteExtensionFile(extension_root_dir_, kManifestFile, kManifestFile);
  WriteExtensionFile(extension_root_dir_, "file_1.Js", kJavaScriptFile1);
  WriteExtensionFile(extension_root_dir_, "file_2.cSS", kCSSFile2);
  auto callback =
      base::BindOnce(&ExtensionTelemetryFileProcessorTest::CallbackHelper,
                     weak_factory_.GetWeakPtr());

  processor_.AsyncCall(&ExtensionTelemetryFileProcessor::ProcessExtension)
      .WithArgs(extension_root_dir_)
      .Then(std::move(callback));
  task_environment_.RunUntilIdle();

  base::Value::Dict expected_dict;
  expected_dict.Set(kManifestFile, kManifestFile);
  expected_dict.Set("file_1.Js", HashContent(kJavaScriptFile1));
  expected_dict.Set("file_2.cSS", HashContent(kCSSFile2));

  EXPECT_EQ(extensions_data_, expected_dict);
}

}  // namespace safe_browsing
