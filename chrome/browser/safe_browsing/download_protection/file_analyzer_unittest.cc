// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/file_analyzer.h"

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/file_type_policies_test_util.h"
#include "chrome/common/safe_browsing/mock_binary_feature_extractor.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace safe_browsing {

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::DoAll;

class FileAnalyzerTest : public testing::Test {
 public:
  FileAnalyzerTest() {}
  void DoneCallback(base::OnceCallback<void()> quit_callback,
                    FileAnalyzer::Results result) {
    result_ = result;
    has_result_ = true;
    std::move(quit_callback).Run();
  }

 protected:
  void SetUp() override { has_result_ = false; }

  void TearDown() override {}

 protected:
  bool has_result_;
  FileAnalyzer::Results result_;

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;
};

TEST_F(FileAnalyzerTest, TypeWinExecutable) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.exe"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  EXPECT_CALL(*extractor, CheckSignature(tmp_path, _)).WillOnce(Return());
  EXPECT_CALL(*extractor, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.type, ClientDownloadRequest::WIN_EXECUTABLE);
}

TEST_F(FileAnalyzerTest, TypeChromeExtension) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.crx"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  EXPECT_CALL(*extractor, CheckSignature(tmp_path, _)).WillOnce(Return());
  EXPECT_CALL(*extractor, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.type, ClientDownloadRequest::CHROME_EXTENSION);
}

TEST_F(FileAnalyzerTest, TypeAndroidApk) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.apk"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  EXPECT_CALL(*extractor, CheckSignature(tmp_path, _)).WillOnce(Return());
  EXPECT_CALL(*extractor, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.type, ClientDownloadRequest::ANDROID_APK);
}

TEST_F(FileAnalyzerTest, TypeZippedExecutable) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.exe")),
                file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path,
                       /* include_hidden_files= */ false));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.type, ClientDownloadRequest::ZIPPED_EXECUTABLE);
}

TEST_F(FileAnalyzerTest, TypeMacExecutable) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.pkg"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  EXPECT_CALL(*extractor, CheckSignature(tmp_path, _)).WillOnce(Return());
  EXPECT_CALL(*extractor, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.type, ClientDownloadRequest::MAC_EXECUTABLE);
}

TEST_F(FileAnalyzerTest, TypeZippedArchive) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.zip")),
                file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path,
                       /* include_hidden_files= */ false));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.type, ClientDownloadRequest::ZIPPED_ARCHIVE);
}

TEST_F(FileAnalyzerTest, TypeInvalidZip) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  std::string file_contents = "invalid contents";
  ASSERT_EQ(
      static_cast<int>(file_contents.size()),
      base::WriteFile(tmp_path, file_contents.data(), file_contents.size()));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.type, ClientDownloadRequest::INVALID_ZIP);
}

// Since we only inspect contents of DMGs on OS X, we only get
// MAC_ARCHIVE_FAILED_PARSING on OS X.
#if defined(OS_MACOSX)
TEST_F(FileAnalyzerTest, TypeInvalidDmg) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.dmg"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  std::string file_contents = "invalid contents";
  ASSERT_EQ(
      static_cast<int>(file_contents.size()),
      base::WriteFile(tmp_path, file_contents.data(), file_contents.size()));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.type, ClientDownloadRequest::MAC_ARCHIVE_FAILED_PARSING);
}
#endif

// TODO(drubery): Add tests verifying Rar inspection

TEST_F(FileAnalyzerTest, ArchiveIsValidUnsetForNonArchive) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.exe"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  EXPECT_CALL(*extractor, CheckSignature(tmp_path, _)).WillOnce(Return());
  EXPECT_CALL(*extractor, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.archive_is_valid, FileAnalyzer::ArchiveValid::UNSET);
}

TEST_F(FileAnalyzerTest, ArchiveIsValidSetForValidArchive) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.exe")),
                file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path,
                       /* include_hidden_files= */ false));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.archive_is_valid, FileAnalyzer::ArchiveValid::VALID);
}

TEST_F(FileAnalyzerTest, ArchiveIsValidSetForInvalidArchive) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  std::string file_contents = "invalid zip";
  ASSERT_EQ(
      static_cast<int>(file_contents.size()),
      base::WriteFile(tmp_path, file_contents.data(), file_contents.size()));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.archive_is_valid, FileAnalyzer::ArchiveValid::INVALID);
}

TEST_F(FileAnalyzerTest, ArchivedExecutableSetForZipWithExecutable) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.exe")),
                file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path,
                       /* include_hidden_files= */ false));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_TRUE(result_.archived_executable);
}

TEST_F(FileAnalyzerTest, ArchivedExecutableFalseForZipNoExecutable) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.txt")),
                file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path,
                       /* include_hidden_files= */ false));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_FALSE(result_.archived_executable);
}

TEST_F(FileAnalyzerTest, ArchivedArchiveSetForZipWithArchive) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.zip")),
                file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path,
                       /* include_hidden_files= */ false));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_TRUE(result_.archived_archive);
}

TEST_F(FileAnalyzerTest, ArchivedArchiveSetForZipNoArchive) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.txt")),
                file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path,
                       /* include_hidden_files= */ false));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_FALSE(result_.archived_archive);
}

TEST_F(FileAnalyzerTest, ArchivedBinariesHasArchiveAndExecutable) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.exe")),
                file_contents.data(), file_contents.size()));
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.rar")),
                file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path,
                       /* include_hidden_files= */ false));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_THAT(result_.archived_binaries, SizeIs(2));
}

TEST_F(FileAnalyzerTest, ArchivedBinariesSkipsSafeFiles) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.txt")),
                file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path,
                       /* include_hidden_files= */ false));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_THAT(result_.archived_binaries, IsEmpty());
}

TEST_F(FileAnalyzerTest, ArchivedBinariesRespectsPolicyMaximum) {
  // Set the policy maximum to 1
  FileTypePoliciesTestOverlay policies;
  std::unique_ptr<DownloadFileTypeConfig> config = policies.DuplicateConfig();
  config->set_max_archived_binaries_to_report(1);
  policies.SwapConfig(config);

  // Analyze an archive with 2 binaries
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.zip"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.crdownload"));

  base::ScopedTempDir zip_source_dir;
  ASSERT_TRUE(zip_source_dir.CreateUniqueTempDir());
  std::string file_contents = "dummy file";
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.exe")),
                file_contents.data(), file_contents.size()));
  ASSERT_EQ(static_cast<int>(file_contents.size()),
            base::WriteFile(
                zip_source_dir.GetPath().Append(FILE_PATH_LITERAL("file.rar")),
                file_contents.data(), file_contents.size()));
  ASSERT_TRUE(zip::Zip(zip_source_dir.GetPath(), tmp_path,
                       /* include_hidden_files= */ false));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_THAT(result_.archived_binaries, SizeIs(1));
}

TEST_F(FileAnalyzerTest, ExtractsFileSignatureForExe) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.exe"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.exe"));

  ClientDownloadRequest::SignatureInfo signature;
  *signature.add_signed_data() = "signature";

  EXPECT_CALL(*extractor, CheckSignature(tmp_path, _))
      .WillOnce(SetArgPointee<1>(signature));
  EXPECT_CALL(*extractor, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(Return(true));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_THAT(result_.signature_info.signed_data(), SizeIs(1));
  EXPECT_THAT(result_.signature_info.signed_data(0), StrEq("signature"));
}

TEST_F(FileAnalyzerTest, ExtractsImageHeadersForExe) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.exe"));
  base::FilePath tmp_path(FILE_PATH_LITERAL("tmp.exe"));

  ClientDownloadRequest::ImageHeaders image_headers;
  image_headers.mutable_pe_headers()->set_file_header("image header");

  EXPECT_CALL(*extractor, CheckSignature(tmp_path, _)).WillOnce(Return());
  EXPECT_CALL(*extractor, ExtractImageFeatures(tmp_path, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(image_headers), Return(true)));

  analyzer.Start(
      target_path, tmp_path,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_TRUE(result_.image_headers.has_pe_headers());
  EXPECT_EQ(result_.image_headers.pe_headers().file_header(), "image header");
}

#if defined(OS_MACOSX)

TEST_F(FileAnalyzerTest, ExtractsSignatureForDmg) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.dmg"));
  base::FilePath signed_dmg;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &signed_dmg));
  signed_dmg = signed_dmg.AppendASCII("safe_browsing")
                   .AppendASCII("mach_o")
                   .AppendASCII("signed-archive.dmg");

  analyzer.Start(
      target_path, signed_dmg,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(2215u, result_.disk_image_signature.size());

  base::FilePath signed_dmg_signature;
  EXPECT_TRUE(
      base::PathService::Get(chrome::DIR_TEST_DATA, &signed_dmg_signature));
  signed_dmg_signature = signed_dmg_signature.AppendASCII("safe_browsing")
                             .AppendASCII("mach_o")
                             .AppendASCII("signed-archive-signature.data");

  std::string signature;
  base::ReadFileToString(signed_dmg_signature, &signature);
  EXPECT_EQ(2215u, signature.length());
  std::vector<uint8_t> signature_vector(signature.begin(), signature.end());
  EXPECT_EQ(signature_vector, result_.disk_image_signature);
}

TEST_F(FileAnalyzerTest, TypeSniffsDmgWithoutExtension) {
  scoped_refptr<MockBinaryFeatureExtractor> extractor =
      new testing::StrictMock<MockBinaryFeatureExtractor>();
  FileAnalyzer analyzer(extractor);
  base::RunLoop run_loop;

  base::FilePath target_path(FILE_PATH_LITERAL("target.dmg"));
  base::FilePath dmg_no_extension;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &dmg_no_extension));
  dmg_no_extension = dmg_no_extension.AppendASCII("safe_browsing")
                         .AppendASCII("dmg")
                         .AppendASCII("data")
                         .AppendASCII("mach_o_in_dmg.txt");

  analyzer.Start(
      target_path, dmg_no_extension,
      base::BindOnce(&FileAnalyzerTest::DoneCallback, base::Unretained(this),
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(has_result_);
  EXPECT_EQ(result_.type, ClientDownloadRequest::MAC_EXECUTABLE);
  EXPECT_EQ(result_.archive_is_valid, FileAnalyzer::ArchiveValid::VALID);
}

#endif

}  // namespace safe_browsing
