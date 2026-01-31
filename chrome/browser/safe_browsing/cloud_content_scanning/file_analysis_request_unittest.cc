// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/common/chrome_paths.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using ::enterprise_connectors::BinaryUploadRequest;
using ::enterprise_connectors::BinaryUploadService;

// Helper to cast base::DoNothing.
BinaryUploadRequest::ContentAnalysisCallback DoNothingConnector() {
  return base::DoNothing();
}

// The mime type detected for each file can vary based on the platform/builder,
// so helper functions are used to validate that at least the returned type is
// one of multiple values.
bool IsZipMimeType(const std::string& mime_type) {
  static std::set<std::string> set = {"application/zip",
                                      "application/x-zip-compressed"};
  return set.count(mime_type);
}

}  // namespace

class FileAnalysisRequestTest : public testing::Test {
 public:
  FileAnalysisRequestTest() = default;

  std::unique_ptr<FileAnalysisRequest> MakeRequest(base::FilePath path,
                                                   base::FilePath file_name,
                                                   bool delay_opening_file,
                                                   std::string mime_type = "",
                                                   bool is_obfuscated = false) {
    enterprise_connectors::AnalysisSettings settings;
    return std::make_unique<FileAnalysisRequest>(
        settings, path, file_name, mime_type, delay_opening_file,
        DoNothingConnector(), base::DoNothing(), is_obfuscated);
  }
};

TEST_F(FileAnalysisRequestTest, SuccessWithCorrectPassword) {
  content::BrowserTaskEnvironment browser_task_environment;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip =
      test_zip.AppendASCII("safe_browsing/download_protection/encrypted.zip");

  auto request =
      MakeRequest(test_zip, test_zip.BaseName(), /*delay_opening_file*/ false);
  request->set_password("12345");

  base::test::TestFuture<enterprise_connectors::ScanRequestUploadResult,
                         BinaryUploadRequest::Data>
      future;
  request->GetRequestData(future.GetCallback());

  auto [result, data] = future.Take();

  EXPECT_EQ(result, enterprise_connectors::ScanRequestUploadResult::kSuccess);
}

TEST_F(FileAnalysisRequestTest, FileEncryptedWithIncorrectPassword) {
  content::BrowserTaskEnvironment browser_task_environment;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip =
      test_zip.AppendASCII("safe_browsing/download_protection/encrypted.zip");

  auto request =
      MakeRequest(test_zip, test_zip.BaseName(), /*delay_opening_file*/ false);
  request->set_password("67890");

  base::test::TestFuture<enterprise_connectors::ScanRequestUploadResult,
                         BinaryUploadRequest::Data>
      future;
  request->GetRequestData(future.GetCallback());

  auto [result, data] = future.Take();

  EXPECT_EQ(result,
            enterprise_connectors::ScanRequestUploadResult::kFileEncrypted);
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

  auto request =
      MakeRequest(test_zip, test_zip.BaseName(), /*delay_opening_file*/ false);

  base::test::TestFuture<enterprise_connectors::ScanRequestUploadResult,
                         BinaryUploadRequest::Data>
      future;
  request->GetRequestData(future.GetCallback());

  auto [result, data] = future.Take();

  // encrypted_zip_no_extension is a copy of encrypted.zip, so the same
  // assertions hold and the same commands can be used to get its size/hash.
  EXPECT_EQ(result,
            enterprise_connectors::ScanRequestUploadResult::kFileEncrypted);
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

TEST_F(FileAnalysisRequestTest, ObfuscatedEncryptedZipFile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {enterprise_obfuscation::kEnterpriseFileObfuscation,
       enterprise_obfuscation::kEnterpriseFileObfuscationArchiveAnalyzer},
      {});

  content::BrowserTaskEnvironment browser_task_environment;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");

  std::string original_contents;
  ASSERT_TRUE(base::ReadFileToString(test_zip, &original_contents));

  // Obfuscate the file contents and write to file.
  enterprise_obfuscation::DownloadObfuscator obfuscator;
  auto obfuscation_result =
      obfuscator.ObfuscateChunk(base::as_byte_span(original_contents), true);

  ASSERT_TRUE(obfuscation_result.has_value());
  base::FilePath obfuscated_path =
      temp_dir.GetPath().AppendASCII("obfuscated.zip");
  ASSERT_TRUE(base::WriteFile(obfuscated_path, obfuscation_result.value()));

  auto obfuscated_request =
      MakeRequest(obfuscated_path, obfuscated_path.BaseName(),
                  /*delay_opening_file=*/false,
                  /*mime_type=*/"application/zip",
                  /*is_obfuscated=*/true);
  obfuscated_request->set_password("67890");  // Incorrect password

  base::test::TestFuture<enterprise_connectors::ScanRequestUploadResult,
                         BinaryUploadRequest::Data>
      future;
  obfuscated_request->GetRequestData(future.GetCallback());
  auto [result, data] = future.Take();

  // Should detect encryption and fail because of incorrect password.
  EXPECT_EQ(result,
            enterprise_connectors::ScanRequestUploadResult::kFileEncrypted);
  // Check if size has been updated to use the calculated unobfuscated content
  // size.
  EXPECT_EQ(data.size, original_contents.size());
}

class FileAnalysisRequestRarTest : public FileAnalysisRequestTest,
                                   public testing::WithParamInterface<bool> {
 public:
  bool is_obfuscated() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(, FileAnalysisRequestRarTest, testing::Bool());

TEST_P(FileAnalysisRequestRarTest, EncryptedRarFile) {
  base::test::ScopedFeatureList scoped_feature_list;
  if (is_obfuscated()) {
    scoped_feature_list.InitWithFeatures(
        {enterprise_obfuscation::kEnterpriseFileObfuscation,
         enterprise_obfuscation::kEnterpriseFileObfuscationArchiveAnalyzer},
        {});
  }

  content::BrowserTaskEnvironment browser_task_environment;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath test_rar;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_rar));
  test_rar = test_rar.AppendASCII("safe_browsing")
                 .AppendASCII("rar")
                 .AppendASCII("passwd1234.rar");

  std::string original_contents;
  ASSERT_TRUE(base::ReadFileToString(test_rar, &original_contents));

  base::FilePath file_path = test_rar;
  if (is_obfuscated()) {
    // Obfuscate the file contents and write to file.
    enterprise_obfuscation::DownloadObfuscator obfuscator;
    auto obfuscation_result =
        obfuscator.ObfuscateChunk(base::as_byte_span(original_contents), true);

    ASSERT_TRUE(obfuscation_result.has_value());
    file_path = temp_dir.GetPath().AppendASCII("obfuscated.rar");
    ASSERT_TRUE(base::WriteFile(file_path, obfuscation_result.value()));
  }

  auto request = MakeRequest(file_path, file_path.BaseName(),
                             /*delay_opening_file=*/false,
                             /*mime_type=*/"application/x-rar-compressed",
                             /*is_obfuscated=*/is_obfuscated());
  request->set_password("67890");  // Incorrect password

  base::test::TestFuture<enterprise_connectors::ScanRequestUploadResult,
                         BinaryUploadRequest::Data>
      future;
  request->GetRequestData(future.GetCallback());
  auto [result, data] = future.Take();

  EXPECT_EQ(result,
            enterprise_connectors::ScanRequestUploadResult::kFileEncrypted);
  EXPECT_EQ(data.size, original_contents.size());
}

}  // namespace safe_browsing
