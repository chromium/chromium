// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

enterprise_connectors::AnalysisSettings settings(bool block_unsupported_types) {
  enterprise_connectors::AnalysisSettings settings;
  settings.block_unsupported_file_types = block_unsupported_types;
  return settings;
}

// Helper to cast base::DoNothing.
BinaryUploadService::ContentAnalysisCallback DoNothingConnector() {
  return base::DoNothing();
}

// The mime type detected for each file can vary based on the platform/builder,
// so helper functions are used to validate that at least the returned type is
// one of multiple values.
bool IsDocMimeType(const std::string& mime_type) {
  static std::set<std::string> set = {
      "application/msword", "text/plain",
      // Large files can result in no mimetype being found.
      ""};
  return set.count(mime_type);
}

bool IsZipMimeType(const std::string& mime_type) {
  static std::set<std::string> set = {"application/zip",
                                      "application/x-zip-compressed"};
  return set.count(mime_type);
}

}  // namespace

class FileAnalysisRequestTest : public testing::Test {
 public:
  FileAnalysisRequestTest() = default;

  std::unique_ptr<FileAnalysisRequest> MakeRequest(bool block_unsupported_types,
                                                   base::FilePath path,
                                                   base::FilePath file_name,
                                                   bool delay_opening_file,
                                                   std::string mime_type = "") {
    return std::make_unique<FileAnalysisRequest>(
        settings(block_unsupported_types), path, file_name, mime_type,
        delay_opening_file, DoNothingConnector());
  }

  void GetResultsForFileContents(const std::string& file_contents,
                                 BinaryUploadService::Result* out_result,
                                 BinaryUploadService::Request::Data* out_data) {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
    base::WriteFile(file_path, file_contents);

    auto request =
        MakeRequest(/*block_unsupported_types=*/false, file_path,
                    file_path.BaseName(), /*delay_opening_file*/ false);

    bool called = false;
    base::RunLoop run_loop;
    request->GetRequestData(base::BindLambdaForTesting(
        [&run_loop, &called, &out_result, &out_data](
            BinaryUploadService::Result result,
            BinaryUploadService::Request::Data data) {
          called = true;
          run_loop.Quit();
          *out_result = result;
          *out_data = std::move(data);
        }));
    run_loop.Run();

    EXPECT_TRUE(called);
    EXPECT_EQ(file_path, out_data->path);
    EXPECT_TRUE(out_data->contents.empty());
  }

 private:
};

TEST_F(FileAnalysisRequestTest, InvalidFiles) {
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  {
    // Non-existent files should return UNKNOWN and have no information set.
    base::FilePath path = temp_dir.GetPath().AppendASCII("not_a_real.doc");
    auto request = MakeRequest(/*block_unsupported_types=*/false, path,
                               path.BaseName(), /*delay_opening_file*/ false);

    bool called = false;
    base::RunLoop run_loop;
    request->GetRequestData(base::BindLambdaForTesting(
        [&run_loop, &called](BinaryUploadService::Result result,
                             BinaryUploadService::Request::Data data) {
          called = true;
          run_loop.Quit();

          EXPECT_EQ(result, BinaryUploadService::Result::UNKNOWN);
          EXPECT_EQ(data.size, 0u);
          EXPECT_TRUE(data.contents.empty());
          EXPECT_TRUE(data.hash.empty());
          EXPECT_TRUE(data.mime_type.empty());
        }));
    run_loop.Run();

    EXPECT_TRUE(called);
  }

  {
    // Directories should not be used as paths passed to GetFileSHA256Blocking,
    // so they should return UNKNOWN and have no information set.
    base::FilePath path = temp_dir.GetPath();
    auto request = MakeRequest(/*block_unsupported_types=*/false, path,
                               path.BaseName(), /*delay_opening_file*/ false);

    bool called = false;
    base::RunLoop run_loop;
    request->GetRequestData(base::BindLambdaForTesting(
        [&run_loop, &called](BinaryUploadService::Result result,
                             BinaryUploadService::Request::Data data) {
          called = true;
          run_loop.Quit();

          EXPECT_EQ(result, BinaryUploadService::Result::UNKNOWN);
          EXPECT_EQ(data.size, 0u);
          EXPECT_TRUE(data.contents.empty());
          EXPECT_TRUE(data.hash.empty());
          EXPECT_TRUE(data.mime_type.empty());
        }));
    run_loop.Run();

    EXPECT_TRUE(called);
  }

  {
    // Empty files should return SUCCESS as they have no content to scan.
    base::FilePath path = temp_dir.GetPath().AppendASCII("empty.doc");
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    auto request = MakeRequest(/*block_unsupported_types=*/false, path,
                               path.BaseName(), /*delay_opening_file*/ false);

    bool called = false;
    base::RunLoop run_loop;
    request->GetRequestData(base::BindLambdaForTesting(
        [&run_loop, &called](BinaryUploadService::Result result,
                             BinaryUploadService::Request::Data data) {
          called = true;
          run_loop.Quit();

          EXPECT_EQ(result, BinaryUploadService::Result::SUCCESS);
          EXPECT_EQ(data.size, 0u);
          EXPECT_TRUE(data.contents.empty());
          EXPECT_TRUE(data.hash.empty());
          EXPECT_TRUE(data.mime_type.empty());
        }));
    run_loop.Run();

    EXPECT_TRUE(called);
  }
}

TEST_F(FileAnalysisRequestTest, NormalFiles) {
  base::test::TaskEnvironment task_environment;

  BinaryUploadService::Result result;
  BinaryUploadService::Request::Data data;

  std::string normal_contents = "Normal file contents";
  GetResultsForFileContents(normal_contents, &result, &data);
  EXPECT_EQ(result, BinaryUploadService::Result::SUCCESS);
  EXPECT_EQ(data.size, normal_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // printf "Normal file contents" | sha256sum |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
  EXPECT_TRUE(IsDocMimeType(data.mime_type))
      << data.mime_type << " is not an expected mimetype";

  std::string long_contents =
      std::string(BinaryUploadService::kMaxUploadSizeBytes, 'a');
  GetResultsForFileContents(long_contents, &result, &data);
  EXPECT_EQ(result, BinaryUploadService::Result::SUCCESS);
  EXPECT_EQ(data.size, long_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // printf "Normal file contents" | sha256sum |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "4F0E9C6A1A9A90F35B884D0F0E7343459C21060EEFEC6C0F2FA9DC1118DBE5BE");
  EXPECT_TRUE(IsDocMimeType(data.mime_type))
      << data.mime_type << " is not an expected mimetype";
}

// Disabled due to flakiness on Mac https://crbug.com/1229051
#if BUILDFLAG(IS_MAC)
#define MAYBE_LargeFiles DISABLED_LargeFiles
#else
#define MAYBE_LargeFiles LargeFiles
#endif

TEST_F(FileAnalysisRequestTest, MAYBE_LargeFiles) {
  base::test::TaskEnvironment task_environment;

  BinaryUploadService::Result result;
  BinaryUploadService::Request::Data data;

  std::string large_file_contents(BinaryUploadService::kMaxUploadSizeBytes + 1,
                                  'a');
  GetResultsForFileContents(large_file_contents, &result, &data);
  EXPECT_EQ(result, BinaryUploadService::Result::FILE_TOO_LARGE);
  EXPECT_EQ(data.size, large_file_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // python3 -c "print('a' * (50 * 1024 * 1024 + 1), end='')" | sha256sum | tr
  // '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "9EB56DB30C49E131459FE735BA6B9D38327376224EC8D5A1233F43A5B4A25942");
  EXPECT_TRUE(IsDocMimeType(data.mime_type))
      << data.mime_type << " is not an expected mimetype";

  std::string very_large_file_contents(
      2 * BinaryUploadService::kMaxUploadSizeBytes, 'a');
  GetResultsForFileContents(very_large_file_contents, &result, &data);
  EXPECT_EQ(result, BinaryUploadService::Result::FILE_TOO_LARGE);
  EXPECT_EQ(data.size, very_large_file_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // python3 -c "print('a' * (100 * 1024 * 1024), end='')" | sha256sum | tr
  // '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "CEE41E98D0A6AD65CC0EC77A2BA50BF26D64DC9007F7F1C7D7DF68B8B71291A6");
  EXPECT_TRUE(IsDocMimeType(data.mime_type))
      << data.mime_type << " is not an expected mimetype";
}

TEST_F(FileAnalysisRequestTest, PopulatesDigest) {
  base::test::TaskEnvironment task_environment;
  std::string file_contents = "Normal file contents";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());

  auto request =
      MakeRequest(/*block_unsupported_types=*/false, file_path,
                  file_path.BaseName(), /*delay_opening_file*/ false);

  base::RunLoop run_loop;
  request->GetRequestData(base::BindLambdaForTesting(
      [&run_loop](BinaryUploadService::Result result,
                  BinaryUploadService::Request::Data data) {
        run_loop.Quit();
      }));
  run_loop.Run();

  // printf "Normal file contents" | sha256sum |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(request->digest(),
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
}

TEST_F(FileAnalysisRequestTest, PopulatesFilename) {
  base::test::TaskEnvironment task_environment;
  std::string file_contents = "contents";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());

  auto request =
      MakeRequest(/*block_unsupported_types=*/false, file_path,
                  file_path.BaseName(), /*delay_opening_file*/ false);

  base::RunLoop run_loop;
  request->GetRequestData(base::BindLambdaForTesting(
      [&run_loop](BinaryUploadService::Result result,
                  BinaryUploadService::Request::Data data) {
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(request->filename(), file_path.AsUTF8Unsafe());
}

TEST_F(FileAnalysisRequestTest, CachesResults) {
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::string normal_contents = "Normal file contents";
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
  base::WriteFile(file_path, normal_contents);

  BinaryUploadService::Result async_result;
  BinaryUploadService::Request::Data async_data;

  auto request =
      MakeRequest(/*block_unsupported_types=*/false, file_path,
                  file_path.BaseName(), /*delay_opening_file*/ false);

  bool called = false;
  base::RunLoop run_loop;
  request->GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &called, &async_result, &async_data](
          BinaryUploadService::Result result,
          BinaryUploadService::Request::Data data) {
        called = true;
        run_loop.Quit();
        async_result = result;
        async_data = std::move(data);
      }));
  run_loop.Run();

  ASSERT_TRUE(called);

  BinaryUploadService::Result sync_result;
  BinaryUploadService::Request::Data sync_data;
  request->GetRequestData(
      base::BindLambdaForTesting([&run_loop, &called, &sync_result, &sync_data](
                                     BinaryUploadService::Result result,
                                     BinaryUploadService::Request::Data data) {
        called = true;
        run_loop.Quit();
        sync_result = result;
        sync_data = std::move(data);
      }));

  EXPECT_EQ(sync_result, async_result);
  EXPECT_EQ(sync_data.contents, async_data.contents);
  EXPECT_EQ(sync_data.size, async_data.size);
  EXPECT_EQ(sync_data.hash, async_data.hash);
  EXPECT_EQ(sync_data.mime_type, async_data.mime_type);
}

TEST_F(FileAnalysisRequestTest, CachesResultsWithKnownMimetype) {
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::string normal_contents = "Normal file contents";
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("normal.doc");
  base::WriteFile(file_path, normal_contents);

  BinaryUploadService::Result result;
  BinaryUploadService::Request::Data data;

  auto request = MakeRequest(/*block_unsupported_types=*/false, file_path,
                             file_path.BaseName(), /*delay_opening_file*/ false,
                             "fake/mimetype");

  bool called = false;
  base::RunLoop run_loop;
  request->GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &called, &result, &data](
          BinaryUploadService::Result tmp_result,
          BinaryUploadService::Request::Data tmp_data) {
        called = true;
        run_loop.Quit();
        result = tmp_result;
        data = std::move(tmp_data);
      }));
  run_loop.Run();

  ASSERT_TRUE(called);

  EXPECT_EQ(result, BinaryUploadService::Result::SUCCESS);
  EXPECT_EQ(data.size, normal_contents.size());
  EXPECT_TRUE(data.contents.empty());
  // printf "Normal file contents" | sha256sum | tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
  EXPECT_EQ(request->digest(), data.hash);
  EXPECT_EQ(request->content_type(), "fake/mimetype");
}

TEST_F(FileAnalysisRequestTest, DelayedFileOpening) {
  content::BrowserTaskEnvironment browser_task_environment;

  // base::test::TaskEnvironment task_environment;
  std::string file_contents = "Normal file contents";
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("foo.doc");

  // Create the file.
  base::File file(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());

  auto request = MakeRequest(/*block_unsupported_types=*/false, file_path,
                             file_path.BaseName(), /*delay_opening_file*/ true);

  base::RunLoop run_loop;
  request->GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &file_contents](BinaryUploadService::Result result,
                                  BinaryUploadService::Request::Data data) {
        run_loop.Quit();

        EXPECT_EQ(result, BinaryUploadService::Result::SUCCESS);
        EXPECT_EQ(data.size, file_contents.size());
        EXPECT_TRUE(data.contents.empty());
        // printf "Normal file contents" | sha256sum |\
        // tr '[:lower:]' '[:upper:]'
        EXPECT_EQ(
            data.hash,
            "29644C10BD036866FCFD2BDACFF340DB5DE47A90002D6AB0C42DE6A22C26158B");
        EXPECT_TRUE(IsDocMimeType(data.mime_type))
            << data.mime_type << " is not an expected mimetype";
      }));

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  request->OpenFile();
  run_loop.Run();

  EXPECT_TRUE(run_loop.AnyQuitCalled());
}

// Class used to validate that an archive file is correctly detected and checked
// for encryption, even without a .zip/.rar extension.
class FileAnalysisRequestZipTest
    : public FileAnalysisRequestTest,
      public testing::WithParamInterface<const char*> {
 public:
  const char* file_name() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(,
                         FileAnalysisRequestZipTest,
                         testing::Values("encrypted.zip",
                                         "encrypted_zip_no_extension"));

TEST_P(FileAnalysisRequestZipTest, Encrypted) {
  content::BrowserTaskEnvironment browser_task_environment;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII(file_name());

  auto request = MakeRequest(/*block_unsupported_types=*/false, test_zip,
                             test_zip.BaseName(), /*delay_opening_file*/ false);

  bool called = false;
  base::RunLoop run_loop;
  BinaryUploadService::Result result;
  BinaryUploadService::Request::Data data;
  request->GetRequestData(base::BindLambdaForTesting(
      [&run_loop, &called, &result, &data](
          BinaryUploadService::Result tmp_result,
          BinaryUploadService::Request::Data tmp_data) {
        called = true;
        run_loop.Quit();
        result = tmp_result;
        data = std::move(tmp_data);
      }));
  run_loop.Run();

  ASSERT_TRUE(called);

  // encrypted_zip_no_extension is a copy of encrypted.zip, so the same
  // assertions hold and the same commands can be used to get its size/hash.
  EXPECT_EQ(result, BinaryUploadService::Result::FILE_ENCRYPTED);
  // du chrome/test/data/safe_browsing/download_protection/<file> -b
  EXPECT_EQ(data.size, 20015u);
  // sha256sum < chrome/test/data/safe_browsing/download_protection/<file> \
  // |  tr '[:lower:]' '[:upper:]'
  EXPECT_EQ(data.hash,
            "701FCEA8B2112FFAB257A8A8DFD3382ABCF047689AB028D42903E3B3AA488D9A");
  EXPECT_EQ(request->digest(), data.hash);
  EXPECT_TRUE(data.contents.empty());
  EXPECT_EQ(test_zip, data.path);
  EXPECT_TRUE(IsZipMimeType(data.mime_type));
}

}  // namespace safe_browsing
