// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <optional>

#include "base/test/file_path_reparse_point_win.h"
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
namespace {

// Creates a directory symlink on POSIX or an NTFS junction on Windows, keeping
// the Windows reparse point handle alive for the scope.
class ScopedDirectoryLink {
 public:
  ScopedDirectoryLink() = default;
  ~ScopedDirectoryLink() = default;

  [[nodiscard]] bool Create(const base::FilePath& link_path,
                            const base::FilePath& target_dir) {
#if BUILDFLAG(IS_POSIX)
    return base::CreateSymbolicLink(target_dir, link_path);
#elif BUILDFLAG(IS_WIN)
    if (!base::CreateDirectory(link_path)) {
      return false;
    }
    reparse_point_ =
        base::test::FilePathReparsePoint::Create(link_path, target_dir);
    return reparse_point_.has_value();
#endif
  }

 private:
#if BUILDFLAG(IS_WIN)
  std::optional<base::test::FilePathReparsePoint> reparse_point_;
#endif
};

}  // namespace
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)

const char kTestTime[] = "time";
const char kTestReportId[] = "report-id";
const char kTestLocalId[] = "local-id";

class WebRtcLogUploaderTest : public testing::Test {
 public:
  WebRtcLogUploaderTest() = default;

 protected:
  bool VerifyNumberOfLines(int expected_lines) {
    std::vector<std::string> lines = GetLinesFromListFile();
    EXPECT_EQ(expected_lines, static_cast<int>(lines.size()));
    return expected_lines == static_cast<int>(lines.size());
  }

  bool VerifyLastLineHasAllInfo() {
    std::string last_line = GetLastLineFromListFile();
    if (last_line.empty())
      return false;
    std::vector<std::string> line_parts = base::SplitString(
        last_line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_EQ(4u, line_parts.size());
    if (4u != line_parts.size())
      return false;
    // The times (indices 0 and 3) is the time when the info was written to the
    // file which we don't know, so just verify that it's not empty.
    EXPECT_FALSE(line_parts[0].empty());
    EXPECT_STREQ(kTestReportId, line_parts[1].c_str());
    EXPECT_STREQ(kTestLocalId, line_parts[2].c_str());
    EXPECT_FALSE(line_parts[3].empty());
    return true;
  }

  // Verify that the last line contains the correct info for a local storage.
  bool VerifyLastLineHasLocalStorageInfoOnly() {
    std::string last_line = GetLastLineFromListFile();
    if (last_line.empty())
      return false;
    std::vector<std::string> line_parts = base::SplitString(
        last_line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_EQ(4u, line_parts.size());
    if (4u != line_parts.size())
      return false;
    EXPECT_TRUE(line_parts[0].empty());
    EXPECT_TRUE(line_parts[1].empty());
    EXPECT_STREQ(kTestLocalId, line_parts[2].c_str());
    EXPECT_FALSE(line_parts[3].empty());
    return true;
  }

  // Verify that the last line contains the correct info for an upload.
  bool VerifyLastLineHasUploadInfoOnly() {
    std::string last_line = GetLastLineFromListFile();
    if (last_line.empty())
      return false;
    std::vector<std::string> line_parts = base::SplitString(
        last_line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_EQ(4u, line_parts.size());
    if (4u != line_parts.size())
      return false;
    EXPECT_FALSE(line_parts[0].empty());
    EXPECT_STREQ(kTestReportId, line_parts[1].c_str());
    EXPECT_TRUE(line_parts[2].empty());
    EXPECT_FALSE(line_parts[3].empty());
    return true;
  }

  bool AddLinesToTestFile(int number_of_lines) {
    base::File test_list_file(test_list_path_,
                              base::File::FLAG_OPEN | base::File::FLAG_APPEND);
    EXPECT_TRUE(test_list_file.IsValid());
    if (!test_list_file.IsValid())
      return false;

    for (int i = 0; i < number_of_lines; ++i) {
      EXPECT_TRUE(test_list_file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_cstring(kTestTime)));
      EXPECT_TRUE(test_list_file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_cstring(",")));
      EXPECT_TRUE(test_list_file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_cstring(kTestReportId)));
      EXPECT_TRUE(test_list_file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_cstring(",")));
      EXPECT_TRUE(test_list_file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_cstring(kTestLocalId)));
      EXPECT_TRUE(test_list_file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_cstring(",")));
      EXPECT_TRUE(test_list_file.WriteAtCurrentPos(
          base::byte_span_from_cstring(kTestTime)));
      EXPECT_TRUE(test_list_file.WriteAtCurrentPosAndCheck(
          base::byte_span_from_cstring("\n")));
    }
    return true;
  }

  std::vector<std::string> GetLinesFromListFile() {
    std::string contents;
    int read = base::ReadFileToString(test_list_path_, &contents);
    EXPECT_GT(read, 0);
    if (read == 0)
      return std::vector<std::string>();
    // Since every line should end with '\n', the last line should be empty. So
    // we expect at least two lines including the final empty. Remove the empty
    // line before returning.
    std::vector<std::string> lines = base::SplitString(
        contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    EXPECT_GT(lines.size(), 1u);
    if (lines.size() < 2)
      return std::vector<std::string>();
    EXPECT_TRUE(lines.back().empty());
    if (!lines.back().empty())
      return std::vector<std::string>();
    lines.pop_back();
    return lines;
  }

  std::string GetLastLineFromListFile() {
    std::vector<std::string> lines = GetLinesFromListFile();
    EXPECT_GT(lines.size(), 0u);
    if (lines.empty())
      return std::string();
    return lines[lines.size() - 1];
  }

  void VerifyRtpDumpInMultipart(const std::string& post_data,
                                const std::string& dump_name,
                                const std::string& dump_content) {
    std::vector<std::string> lines = base::SplitStringUsingSubstr(
        post_data, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    std::string name_line = "Content-Disposition: form-data; name=\"";
    name_line.append(dump_name);
    name_line.append("\"");
    name_line.append("; filename=\"");
    name_line.append(dump_name);
    name_line.append(".gz\"");

    size_t i = 0;
    for (; i < lines.size(); ++i) {
      if (lines[i] == name_line)
        break;
    }

    // The RTP dump takes 4 lines: content-disposition, content-type, empty
    // line, dump content.
    EXPECT_LT(i, lines.size() - 3);

    EXPECT_EQ("Content-Type: application/gzip", lines[i + 1]);
    EXPECT_EQ("", lines[i + 2]);
    EXPECT_EQ(dump_content, lines[i + 3]);
  }

  std::string GetValueFromMultipart(const std::string& post_data,
                                    const std::string& value_name) {
    std::vector<std::string> lines = base::SplitStringUsingSubstr(
        post_data, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    std::string name_line = "Content-Disposition: form-data; name=\"";
    name_line.append(value_name);
    name_line.append("\"");

    size_t i = 0;
    for (; i < lines.size(); ++i) {
      if (lines[i] == name_line) {
        break;
      }
    }

    EXPECT_LT(i, lines.size() - 2);

    return lines[i + 2];
  }

  static std::string GetMultipartLineStartingWithValue(
      const std::string& post_data,
      const std::string& value_name) {
    std::vector<std::string> lines = base::SplitStringUsingSubstr(
        post_data, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    std::string name_line = "Content-Disposition: form-data; name=\"";
    name_line.append(value_name);
    name_line.append("\"");

    for (const auto& line : lines) {
      if (line.starts_with(name_line)) {
        return line;
      }
    }
    return std::string();
  }

  static void AddLocallyStoredLogInfoToUploadListFile(
      WebRtcLogUploader* log_uploader,
      const base::FilePath& upload_list_path,
      const std::string& local_log_id) {
    base::RunLoop run_loop;
    log_uploader->background_task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &WebRtcLogUploader::AddLocallyStoredLogInfoToUploadListFile,
            base::Unretained(log_uploader), upload_list_path, local_log_id),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  static void AddUploadedLogInfoToUploadListFile(
      const base::FilePath& upload_list_path,
      const std::string& local_log_id,
      const std::string& report_id) {
    WebRtcLogUploader::AddUploadedLogInfoToUploadListFile(
        upload_list_path, local_log_id, report_id);
  }

  void FlushRunLoop() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void UploadPostData(WebRtcLogUploadSite site, std::string& post_data) {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
    webrtc_log_uploader->OverrideUploadWithBufferForTesting(&post_data);

    WebRtcLogUploader::UploadDoneData upload_done_data;
    upload_done_data.paths.directory = temp_dir.GetPath().AppendASCII("log");

    std::unique_ptr<WebRtcLogBuffer> log = std::make_unique<WebRtcLogBuffer>();
    log->SetComplete();

    base::RunLoop run_loop;
    webrtc_log_uploader->background_task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&WebRtcLogUploader::OnLoggingStopped,
                       base::Unretained(webrtc_log_uploader.get()), site,
                       std::move(log), std::make_unique<WebRtcLogMetaDataMap>(),
                       std::move(upload_done_data),
                       /*is_text_log_upload_allowed=*/true),
        run_loop.QuitClosure());
    run_loop.Run();

    webrtc_log_uploader->Shutdown();
    FlushRunLoop();
  }
  base::test::TaskEnvironment task_environment_;
  base::FilePath test_list_path_;
};

TEST_F(WebRtcLogUploaderTest, AddLocallyStoredLogInfoToUploadListFile) {
  // Get a temporary filename. We don't want the file to exist to begin with
  // since that's the normal use case, hence the delete.
  ASSERT_TRUE(base::CreateTemporaryFile(&test_list_path_));
  EXPECT_TRUE(base::DeleteFile(test_list_path_));
  std::unique_ptr<WebRtcLogUploader> webrtc_log_uploader(
      new WebRtcLogUploader());

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(2));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  const int expected_line_limit = 50;
  ASSERT_TRUE(AddLinesToTestFile(expected_line_limit - 2));
  ASSERT_TRUE(VerifyNumberOfLines(expected_line_limit));
  ASSERT_TRUE(VerifyLastLineHasAllInfo());

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(expected_line_limit));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  ASSERT_TRUE(AddLinesToTestFile(10));
  ASSERT_TRUE(VerifyNumberOfLines(60));
  ASSERT_TRUE(VerifyLastLineHasAllInfo());

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(expected_line_limit));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, AddUploadedLogInfoToUploadListFile) {
  // Get a temporary filename. We don't want the file to exist to begin with
  // since that's the normal use case, hence the delete.
  ASSERT_TRUE(base::CreateTemporaryFile(&test_list_path_));
  EXPECT_TRUE(base::DeleteFile(test_list_path_));
  std::unique_ptr<WebRtcLogUploader> webrtc_log_uploader(
      new WebRtcLogUploader());

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  webrtc_log_uploader->AddUploadedLogInfoToUploadListFile(
      test_list_path_, kTestLocalId, kTestReportId);
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasAllInfo());

  // Use a local ID that should not be found in the list.
  webrtc_log_uploader->AddUploadedLogInfoToUploadListFile(
      test_list_path_, "dummy id", kTestReportId);
  ASSERT_TRUE(VerifyNumberOfLines(2));
  ASSERT_TRUE(VerifyLastLineHasUploadInfoOnly());

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, AddRtpDumpsToPostedData) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::string post_data;
  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
  webrtc_log_uploader->OverrideUploadWithBufferForTesting(&post_data);

  // Create the fake dump files.
  const base::FilePath incoming_dump = temp_dir.GetPath().AppendASCII("recv");
  const base::FilePath outgoing_dump = temp_dir.GetPath().AppendASCII("send");
  const std::string incoming_dump_content = "dummy incoming";
  const std::string outgoing_dump_content = "dummy outgoing";

  base::WriteFile(incoming_dump, incoming_dump_content);
  base::WriteFile(outgoing_dump, outgoing_dump_content);

  WebRtcLogUploader::UploadDoneData upload_done_data;
  upload_done_data.paths.directory = temp_dir.GetPath().AppendASCII("log");

  upload_done_data.paths.incoming_rtp_dump = incoming_dump;
  upload_done_data.paths.outgoing_rtp_dump = outgoing_dump;

  std::unique_ptr<WebRtcLogBuffer> log = std::make_unique<WebRtcLogBuffer>();
  log->SetComplete();

  base::RunLoop run_loop;
  webrtc_log_uploader->background_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUploader::OnLoggingStopped,
                     base::Unretained(webrtc_log_uploader.get()),
                     WebRtcLogUploadSite::kSameSite, std::move(log),
                     std::make_unique<WebRtcLogMetaDataMap>(),
                     std::move(upload_done_data),
                     /*is_text_log_upload_allowed=*/true),
      run_loop.QuitClosure());
  run_loop.Run();

  VerifyRtpDumpInMultipart(post_data, "rtpdump_recv", incoming_dump_content);
  VerifyRtpDumpInMultipart(post_data, "rtpdump_send", outgoing_dump_content);

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, DisableUploadOfMultipartData) {
  base::test::TestFuture<bool, const std::string&, const std::string&> future;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::string post_data;
  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
  webrtc_log_uploader->OverrideUploadWithBufferForTesting(&post_data);

  // Create the fake dump files.
  const base::FilePath incoming_dump = temp_dir.GetPath().AppendASCII("recv");
  const base::FilePath outgoing_dump = temp_dir.GetPath().AppendASCII("send");
  const std::string incoming_dump_content = "dummy incoming";
  const std::string outgoing_dump_content = "dummy outgoing";

  base::WriteFile(incoming_dump, incoming_dump_content);
  base::WriteFile(outgoing_dump, outgoing_dump_content);

  WebRtcLogUploader::UploadDoneData upload_done_data;

  upload_done_data.paths.directory = temp_dir.GetPath().AppendASCII("log");
  upload_done_data.paths.incoming_rtp_dump = incoming_dump;
  upload_done_data.paths.outgoing_rtp_dump = outgoing_dump;
  upload_done_data.callback = future.GetCallback();

  std::unique_ptr<WebRtcLogBuffer> log = std::make_unique<WebRtcLogBuffer>();
  log->SetComplete();

  base::RunLoop run_loop;
  webrtc_log_uploader->background_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUploader::OnLoggingStopped,
                     base::Unretained(webrtc_log_uploader.get()),
                     WebRtcLogUploadSite::kSameSite, std::move(log),
                     std::make_unique<WebRtcLogMetaDataMap>(),
                     std::move(upload_done_data),
                     /*is_text_log_upload_allowed=*/false),
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(future.Get<0>());
  EXPECT_EQ("", future.Get<1>());
  EXPECT_EQ(WebRtcLogUploader::kLogUploadDisabledMsg, future.Get<2>());

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, ProductHasNoSuffixWithoutFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kWebRTCLogUploadSuffix);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::string post_data;
  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
  webrtc_log_uploader->OverrideUploadWithBufferForTesting(&post_data);

  WebRtcLogUploader::UploadDoneData upload_done_data;
  upload_done_data.paths.directory = temp_dir.GetPath().AppendASCII("log");

  std::unique_ptr<WebRtcLogBuffer> log = std::make_unique<WebRtcLogBuffer>();
  log->SetComplete();

  base::RunLoop run_loop;
  webrtc_log_uploader->background_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUploader::OnLoggingStopped,
                     base::Unretained(webrtc_log_uploader.get()),
                     WebRtcLogUploadSite::kSameSite, std::move(log),
                     std::make_unique<WebRtcLogMetaDataMap>(),
                     std::move(upload_done_data),
                     /*is_text_log_upload_allowed=*/true),
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_THAT(GetValueFromMultipart(post_data, "prod"),
              testing::Not(testing::HasSubstr("_webrtc")));

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, ProductHasSuffixWithFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kWebRTCLogUploadSuffix);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  std::string post_data;
  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
  webrtc_log_uploader->OverrideUploadWithBufferForTesting(&post_data);

  WebRtcLogUploader::UploadDoneData upload_done_data;
  upload_done_data.paths.directory = temp_dir.GetPath().AppendASCII("log");

  std::unique_ptr<WebRtcLogBuffer> log = std::make_unique<WebRtcLogBuffer>();
  log->SetComplete();

  base::RunLoop run_loop;
  webrtc_log_uploader->background_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUploader::OnLoggingStopped,
                     base::Unretained(webrtc_log_uploader.get()),
                     WebRtcLogUploadSite::kSameSite, std::move(log),
                     std::make_unique<WebRtcLogMetaDataMap>(),
                     std::move(upload_done_data),
                     /*is_text_log_upload_allowed=*/true),
      run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_THAT(GetValueFromMultipart(post_data, "prod"),
              testing::HasSubstr("_webrtc"));

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, SameSiteContentNameIsCorrect) {
  std::string post_data;
  UploadPostData(WebRtcLogUploadSite::kSameSite, post_data);

  EXPECT_THAT(
      GetMultipartLineStartingWithValue(
          post_data, WebRtcLogUploader::kSameSiteContentName),
      testing::HasSubstr(base::StrCat(
          {"filename=\"", WebRtcLogUploader::kSameSiteContentName, ".gz\""})));
}

TEST_F(WebRtcLogUploaderTest, CrossSiteContentNameIsCorrect) {
  std::string post_data;
  UploadPostData(WebRtcLogUploadSite::kCrossSite, post_data);
  EXPECT_THAT(
      GetMultipartLineStartingWithValue(
          post_data, WebRtcLogUploader::kCrossSiteContentName),
      testing::HasSubstr(base::StrCat(
          {"filename=\"", WebRtcLogUploader::kCrossSiteContentName, ".gz\""})));
}

TEST_F(WebRtcLogUploaderTest, GetLogUploadProductValues) {
  std::string same_site_product =
      GetLogUploadProduct(WebRtcLogUploadSite::kSameSite);
  std::string cross_site_product =
      GetLogUploadProduct(WebRtcLogUploadSite::kCrossSite);

  EXPECT_THAT(same_site_product, testing::HasSubstr("_webrtc"));
  EXPECT_THAT(same_site_product,
              testing::Not(testing::HasSubstr("_cross_site_webrtc")));

  EXPECT_THAT(cross_site_product, testing::HasSubstr("_cross_site_webrtc"));
}

TEST_F(WebRtcLogUploaderTest, GuidExcludedWithFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kWebRtcLogUploaderExcludesGuid);

  std::string post_data;
  UploadPostData(WebRtcLogUploadSite::kSameSite, post_data);

  EXPECT_TRUE(GetMultipartLineStartingWithValue(post_data, "guid").empty());
}

TEST_F(WebRtcLogUploaderTest, GuidIncludedWithoutFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kWebRtcLogUploaderExcludesGuid);

  std::string post_data;
  UploadPostData(WebRtcLogUploadSite::kSameSite, post_data);

  EXPECT_EQ("0", GetValueFromMultipart(post_data, "guid"));
}

TEST_F(WebRtcLogUploaderTest, RejectsInvalidReportIds) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  test_list_path_ = temp_dir.GetPath().AppendASCII("Log List");
  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId, "");
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     "\nssh-ed25519 AAAA attacker\n");
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     "report\rid");
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     "report\tid");
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     "report id");
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     "report,injected");
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     "report\x01id");
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     "report\x7fid");
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     "report\x80id");
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     std::string(257, 'a'));

  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  const std::string max_len_report_id(256, 'a');
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     max_len_report_id);
  ASSERT_TRUE(VerifyNumberOfLines(1));
  std::vector<std::string> line_parts =
      base::SplitString(GetLastLineFromListFile(), ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  ASSERT_EQ(4u, line_parts.size());
  EXPECT_EQ(max_len_report_id, line_parts[1]);

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, LeavesOversizedUploadListFileUntouched) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  test_list_path_ = temp_dir.GetPath().AppendASCII("Log List");
  const std::string oversized_contents(
      webrtc_logging::kMaxLogListFileSizeBytes + 1, 'a');
  ASSERT_TRUE(base::WriteFile(test_list_path_, oversized_contents));

  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     kTestReportId);

  std::string actual_contents;
  ASSERT_TRUE(base::ReadFileToString(test_list_path_, &actual_contents));
  EXPECT_EQ(oversized_contents.size(), actual_contents.size());
  EXPECT_EQ(oversized_contents, actual_contents);

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

#if BUILDFLAG(IS_POSIX)
TEST_F(WebRtcLogUploaderTest, LeavesUnreadableUploadListFileUntouched) {
  if (geteuid() == 0) {
    GTEST_SKIP() << "chmod 000 does not block reads when running as root.";
  }

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  test_list_path_ = temp_dir.GetPath().AppendASCII("Log List");
  const std::string initial_contents = "100.1,,existing_local,101.1\n";
  ASSERT_TRUE(base::WriteFile(test_list_path_, initial_contents));
  ASSERT_EQ(0, chmod(test_list_path_.value().c_str(), 0000));

  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  AddUploadedLogInfoToUploadListFile(test_list_path_, "existing_local",
                                     kTestReportId);

  ASSERT_EQ(0, chmod(test_list_path_.value().c_str(), 0600));
  std::string actual_contents;
  ASSERT_TRUE(base::ReadFileToString(test_list_path_, &actual_contents));
  EXPECT_EQ(initial_contents, actual_contents);

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, ReplacesFifoAndIgnoresDirectoryAtUploadListPath) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  test_list_path_ = temp_dir.GetPath().AppendASCII("Log List");

  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();

  // A FIFO at Log List must not hang open() and should be atomically replaced.
  ASSERT_EQ(0, mkfifo(test_list_path_.value().c_str(), 0600));
  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  struct stat st;
  ASSERT_EQ(0, lstat(test_list_path_.value().c_str(), &st));
  EXPECT_TRUE(S_ISREG(st.st_mode));
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  ASSERT_TRUE(base::DeleteFile(test_list_path_));
  ASSERT_EQ(0, mkfifo(test_list_path_.value().c_str(), 0600));
  // Hold a writer open so a non-blocking read would fail with EAGAIN without
  // the S_ISFIFO check.
  base::ScopedFD writer(
      HANDLE_EINTR(open(test_list_path_.value().c_str(), O_RDWR | O_NONBLOCK)));
  ASSERT_TRUE(writer.is_valid());
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     kTestReportId);
  ASSERT_EQ(0, lstat(test_list_path_.value().c_str(), &st));
  EXPECT_TRUE(S_ISREG(st.st_mode));
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasUploadInfoOnly());

  // A directory at Log List must be left untouched without crashing.
  ASSERT_TRUE(base::DeleteFile(test_list_path_));
  ASSERT_TRUE(base::CreateDirectory(test_list_path_));
  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     kTestReportId);
  EXPECT_TRUE(base::DirectoryExists(test_list_path_));

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, DoesNotFollowSymlinkForUploadListFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  test_list_path_ = temp_dir.GetPath().AppendASCII("Log List");
  const base::FilePath victim_path = temp_dir.GetPath().AppendASCII("victim");
  const std::string victim_content = "100.1,victim_report,victim_local,101.1\n";
  ASSERT_TRUE(base::WriteFile(victim_path, victim_content));
  ASSERT_TRUE(base::CreateSymbolicLink(victim_path, test_list_path_));
  ASSERT_TRUE(base::IsLink(test_list_path_));

  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  EXPECT_FALSE(base::IsLink(test_list_path_));
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  std::string actual_victim_content;
  ASSERT_TRUE(base::ReadFileToString(victim_path, &actual_victim_content));
  EXPECT_EQ(victim_content, actual_victim_content);

  // Re-plant the symlink and verify AddUploadedLogInfoToUploadListFile also
  // replaces the symlink without reading or modifying the victim file.
  ASSERT_TRUE(base::DeleteFile(test_list_path_));
  ASSERT_TRUE(base::CreateSymbolicLink(victim_path, test_list_path_));
  ASSERT_TRUE(base::IsLink(test_list_path_));

  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     kTestReportId);
  EXPECT_FALSE(base::IsLink(test_list_path_));
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasUploadInfoOnly());

  ASSERT_TRUE(base::ReadFileToString(victim_path, &actual_victim_content));
  EXPECT_EQ(victim_content, actual_victim_content);

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, DoesNotFollowDanglingSymlinkForUploadListFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  test_list_path_ = temp_dir.GetPath().AppendASCII("Log List");
  const base::FilePath victim_path =
      temp_dir.GetPath().AppendASCII("nonexistent_victim");
  ASSERT_TRUE(base::CreateSymbolicLink(victim_path, test_list_path_));
  ASSERT_TRUE(base::IsLink(test_list_path_));

  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();

  AddLocallyStoredLogInfoToUploadListFile(webrtc_log_uploader.get(),
                                          test_list_path_, kTestLocalId);
  EXPECT_FALSE(base::IsLink(test_list_path_));
  EXPECT_FALSE(base::PathExists(victim_path));
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasLocalStorageInfoOnly());

  ASSERT_TRUE(base::DeleteFile(test_list_path_));
  ASSERT_TRUE(base::CreateSymbolicLink(victim_path, test_list_path_));
  ASSERT_TRUE(base::IsLink(test_list_path_));

  AddUploadedLogInfoToUploadListFile(test_list_path_, kTestLocalId,
                                     kTestReportId);
  EXPECT_FALSE(base::IsLink(test_list_path_));
  EXPECT_FALSE(base::PathExists(victim_path));
  ASSERT_TRUE(VerifyNumberOfLines(1));
  ASSERT_TRUE(VerifyLastLineHasUploadInfoOnly());

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest, DoesNotFollowSymlinkInLoggingStoppedDoStore) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath log_dir = temp_dir.GetPath().AppendASCII("log");
  ASSERT_TRUE(base::CreateDirectory(log_dir));

  const base::FilePath victim_gz = temp_dir.GetPath().AppendASCII("victim_gz");
  const base::FilePath victim_meta =
      temp_dir.GetPath().AppendASCII("victim_meta");
  const std::string initial_victim_content = "untouched victim content\n";
  ASSERT_TRUE(base::WriteFile(victim_gz, initial_victim_content));
  ASSERT_TRUE(base::WriteFile(victim_meta, initial_victim_content));

  const base::FilePath gz_link =
      log_dir.AppendASCII(kTestLocalId).AddExtension(FILE_PATH_LITERAL(".gz"));
  const base::FilePath meta_link =
      log_dir.AppendASCII(kTestLocalId)
          .AddExtension(FILE_PATH_LITERAL(".meta"));
  ASSERT_TRUE(base::CreateSymbolicLink(victim_gz, gz_link));
  ASSERT_TRUE(base::CreateSymbolicLink(victim_meta, meta_link));

  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
  ASSERT_TRUE(webrtc_log_uploader->ApplyForStartLogging());

  WebRtcLogPaths paths;
  paths.directory = log_dir;
  auto log = std::make_unique<WebRtcLogBuffer>();
  log->SetComplete();
  auto meta_data = std::make_unique<WebRtcLogMetaDataMap>();
  (*meta_data)["key"] = "\nssh-ed25519 AAAA attacker\n";

  base::test::TestFuture<bool, const std::string&> store_future;
  // base::Unretained is safe because |store_future| waits for completion before
  // |webrtc_log_uploader| goes out of scope.
  webrtc_log_uploader->background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUploader::LoggingStoppedDoStore,
                     base::Unretained(webrtc_log_uploader.get()), paths,
                     kTestLocalId, std::move(log), std::move(meta_data),
                     store_future.GetCallback()));
  EXPECT_TRUE(store_future.Get<0>());

  EXPECT_FALSE(base::IsLink(gz_link));
  EXPECT_FALSE(base::IsLink(meta_link));

  std::string actual_gz_victim;
  std::string actual_meta_victim;
  ASSERT_TRUE(base::ReadFileToString(victim_gz, &actual_gz_victim));
  ASSERT_TRUE(base::ReadFileToString(victim_meta, &actual_meta_victim));
  EXPECT_EQ(initial_victim_content, actual_gz_victim);
  EXPECT_EQ(initial_victim_content, actual_meta_victim);

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest,
       DoesNotFollowDanglingSymlinkInLoggingStoppedDoStore) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath log_dir = temp_dir.GetPath().AppendASCII("log");
  ASSERT_TRUE(base::CreateDirectory(log_dir));

  const base::FilePath victim_gz = temp_dir.GetPath().AppendASCII("victim_gz");
  const base::FilePath victim_meta =
      temp_dir.GetPath().AppendASCII("authorized_keys");
  ASSERT_FALSE(base::PathExists(victim_gz));
  ASSERT_FALSE(base::PathExists(victim_meta));

  const base::FilePath gz_link =
      log_dir.AppendASCII(kTestLocalId).AddExtension(FILE_PATH_LITERAL(".gz"));
  const base::FilePath meta_link =
      log_dir.AppendASCII(kTestLocalId)
          .AddExtension(FILE_PATH_LITERAL(".meta"));
  ASSERT_TRUE(base::CreateSymbolicLink(victim_gz, gz_link));
  ASSERT_TRUE(base::CreateSymbolicLink(victim_meta, meta_link));

  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
  ASSERT_TRUE(webrtc_log_uploader->ApplyForStartLogging());

  WebRtcLogPaths paths;
  paths.directory = log_dir;
  auto log = std::make_unique<WebRtcLogBuffer>();
  log->SetComplete();
  auto meta_data = std::make_unique<WebRtcLogMetaDataMap>();
  (*meta_data)["key"] = "\nssh-ed25519 AAAA attacker\n";

  base::test::TestFuture<bool, const std::string&> store_future;
  // base::Unretained is safe because |store_future| waits for completion before
  // |webrtc_log_uploader| goes out of scope.
  webrtc_log_uploader->background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUploader::LoggingStoppedDoStore,
                     base::Unretained(webrtc_log_uploader.get()), paths,
                     kTestLocalId, std::move(log), std::move(meta_data),
                     store_future.GetCallback()));
  EXPECT_TRUE(store_future.Get<0>());

  EXPECT_FALSE(base::IsLink(gz_link));
  EXPECT_FALSE(base::IsLink(meta_link));
  EXPECT_TRUE(base::PathExists(gz_link));
  EXPECT_TRUE(base::PathExists(meta_link));
  EXPECT_FALSE(base::PathExists(victim_gz));
  EXPECT_FALSE(base::PathExists(victim_meta));

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
TEST_F(WebRtcLogUploaderTest, DoesNotFollowDirectorySymlinkInOnLoggingStopped) {
  network::TestURLLoaderFactory test_url_loader_factory;
  TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
      test_url_loader_factory.GetSafeWeakWrapper());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath victim_dir =
      temp_dir.GetPath().AppendASCII("victim_dir");
  ASSERT_TRUE(base::CreateDirectory(victim_dir));
  const base::FilePath victim_list_path =
      webrtc_logging::TextLogList::GetWebRtcLogListFileForDirectory(victim_dir);
  const std::string initial_victim_list = "100.1,,unrelated_local,101.1\n";
  ASSERT_TRUE(base::WriteFile(victim_list_path, initial_victim_list));

  const base::FilePath symlink_log_dir =
      temp_dir.GetPath().AppendASCII("symlink_log_dir");
  ScopedDirectoryLink dir_link;
  ASSERT_TRUE(dir_link.Create(symlink_log_dir, victim_dir));
  ASSERT_TRUE(base::IsLink(symlink_log_dir));

  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
  ASSERT_TRUE(webrtc_log_uploader->ApplyForStartLogging());

  base::test::TestFuture<bool, const std::string&, const std::string&>
      upload_future;
  WebRtcLogUploader::UploadDoneData upload_done_data;
  upload_done_data.paths.directory = symlink_log_dir;
  upload_done_data.callback = upload_future.GetCallback();
  upload_done_data.web_app_id = 0;

  auto log = std::make_unique<WebRtcLogBuffer>();
  log->SetComplete();

  base::RunLoop bg_loop;
  webrtc_log_uploader->background_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUploader::OnLoggingStopped,
                     base::Unretained(webrtc_log_uploader.get()),
                     WebRtcLogUploadSite::kSameSite, std::move(log),
                     std::make_unique<WebRtcLogMetaDataMap>(),
                     std::move(upload_done_data),
                     /*is_text_log_upload_allowed=*/true),
      bg_loop.QuitClosure());
  bg_loop.Run();
  FlushRunLoop();

  ASSERT_EQ(1, test_url_loader_factory.NumPending());
  test_url_loader_factory.SimulateResponseForPendingRequest(
      webrtc_log_uploader->upload_url().spec(), kTestReportId);

  EXPECT_TRUE(upload_future.Get<0>());
  EXPECT_EQ(kTestReportId, upload_future.Get<1>());

  base::RunLoop flush_bg;
  webrtc_log_uploader->background_task_runner()->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), flush_bg.QuitClosure());
  flush_bg.Run();

  // Verify the victim directory's Log List was not modified and no .gz was
  // written into victim_dir.
  std::string actual_victim_list;
  ASSERT_TRUE(base::ReadFileToString(victim_list_path, &actual_victim_list));
  EXPECT_EQ(initial_victim_list, actual_victim_list);
  base::FileEnumerator gz_files(victim_dir, /*recursive=*/false,
                                base::FileEnumerator::FILES,
                                FILE_PATH_LITERAL("*.gz"));
  EXPECT_TRUE(gz_files.Next().empty());

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest,
       RejectsDirectorySwappedForSymlinkBeforeUploadCompletes) {
  network::TestURLLoaderFactory test_url_loader_factory;
  TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
      test_url_loader_factory.GetSafeWeakWrapper());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath log_dir = temp_dir.GetPath().AppendASCII("log_dir");
  ASSERT_TRUE(base::CreateDirectory(log_dir));

  const base::FilePath victim_dir =
      temp_dir.GetPath().AppendASCII("victim_dir");
  ASSERT_TRUE(base::CreateDirectory(victim_dir));
  const base::FilePath victim_list_path =
      webrtc_logging::TextLogList::GetWebRtcLogListFileForDirectory(victim_dir);
  const std::string initial_victim_list = "100.1,,unrelated_local,101.1\n";
  ASSERT_TRUE(base::WriteFile(victim_list_path, initial_victim_list));

  // Declared after |temp_dir| so the Windows reparse point handle is closed
  // before |temp_dir| is deleted.
  ScopedDirectoryLink dir_link;

  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
  ASSERT_TRUE(webrtc_log_uploader->ApplyForStartLogging());

  base::test::TestFuture<bool, const std::string&, const std::string&>
      upload_future;
  WebRtcLogUploader::UploadDoneData upload_done_data;
  upload_done_data.paths.directory = log_dir;
  upload_done_data.callback = upload_future.GetCallback();
  upload_done_data.web_app_id = 0;

  auto log = std::make_unique<WebRtcLogBuffer>();
  log->SetComplete();

  base::RunLoop bg_loop;
  webrtc_log_uploader->background_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUploader::OnLoggingStopped,
                     base::Unretained(webrtc_log_uploader.get()),
                     WebRtcLogUploadSite::kSameSite, std::move(log),
                     std::make_unique<WebRtcLogMetaDataMap>(),
                     std::move(upload_done_data),
                     /*is_text_log_upload_allowed=*/true),
      bg_loop.QuitClosure());
  bg_loop.Run();
  FlushRunLoop();

  // Swap log_dir for a link to victim_dir while the upload is in flight.
  ASSERT_TRUE(base::DeletePathRecursively(log_dir));
  ASSERT_TRUE(dir_link.Create(log_dir, victim_dir));
  ASSERT_TRUE(base::IsLink(log_dir));

  ASSERT_EQ(1, test_url_loader_factory.NumPending());
  test_url_loader_factory.SimulateResponseForPendingRequest(
      webrtc_log_uploader->upload_url().spec(), kTestReportId);

  EXPECT_TRUE(upload_future.Get<0>());

  base::RunLoop flush_bg;
  webrtc_log_uploader->background_task_runner()->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), flush_bg.QuitClosure());
  flush_bg.Run();

  std::string actual_victim_list;
  ASSERT_TRUE(base::ReadFileToString(victim_list_path, &actual_victim_list));
  EXPECT_EQ(initial_victim_list, actual_victim_list);

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}

TEST_F(WebRtcLogUploaderTest,
       RejectsSymlinkedLogDirectoryInLoggingStoppedDoStore) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath victim_dir =
      temp_dir.GetPath().AppendASCII("victim_dir");
  ASSERT_TRUE(base::CreateDirectory(victim_dir));
  const base::FilePath symlink_log_dir =
      temp_dir.GetPath().AppendASCII("symlink_log_dir");
  ScopedDirectoryLink dir_link;
  ASSERT_TRUE(dir_link.Create(symlink_log_dir, victim_dir));
  ASSERT_TRUE(base::IsLink(symlink_log_dir));

  auto webrtc_log_uploader = std::make_unique<WebRtcLogUploader>();
  // Exhaust all 20 log slots (kLogCountLimit).
  for (int i = 0; i < 20; ++i) {
    ASSERT_TRUE(webrtc_log_uploader->ApplyForStartLogging());
  }
  EXPECT_FALSE(webrtc_log_uploader->ApplyForStartLogging());

  const base::FilePath victim_incoming_rtp =
      victim_dir.AppendASCII("incoming_rtp_temp");
  ASSERT_TRUE(base::WriteFile(victim_incoming_rtp, "rtp in"));
  const base::FilePath outgoing_rtp =
      temp_dir.GetPath().AppendASCII("outgoing_rtp_temp");
  ASSERT_TRUE(base::WriteFile(outgoing_rtp, "rtp out"));

  WebRtcLogPaths symlink_paths;
  symlink_paths.directory = symlink_log_dir;
  symlink_paths.incoming_rtp_dump =
      symlink_log_dir.AppendASCII("incoming_rtp_temp");
  symlink_paths.outgoing_rtp_dump = outgoing_rtp;
  auto symlink_log = std::make_unique<WebRtcLogBuffer>();
  symlink_log->SetComplete();
  base::test::TestFuture<bool, const std::string&> symlink_store_future;
  // base::Unretained is safe because |symlink_store_future| waits for
  // completion before |webrtc_log_uploader| goes out of scope.
  webrtc_log_uploader->background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcLogUploader::LoggingStoppedDoStore,
                     base::Unretained(webrtc_log_uploader.get()), symlink_paths,
                     kTestLocalId, std::move(symlink_log),
                     std::make_unique<WebRtcLogMetaDataMap>(),
                     symlink_store_future.GetCallback()));
  EXPECT_FALSE(symlink_store_future.Get<0>());
  EXPECT_EQ("Invalid WebRTC log directory.", symlink_store_future.Get<1>());
  // The dump under the symlinked parent is preserved and nothing else is
  // written into |victim_dir|, while the non-symlinked dump is deleted.
  EXPECT_TRUE(base::PathExists(victim_incoming_rtp));
  ASSERT_TRUE(base::DeleteFile(victim_incoming_rtp));
  EXPECT_TRUE(base::IsDirectoryEmpty(victim_dir));
  EXPECT_FALSE(base::PathExists(outgoing_rtp));

  // DecreaseLogCount() is posted before done_callback, so a slot is already
  // freed when symlink_store_future resolves.
  EXPECT_TRUE(webrtc_log_uploader->ApplyForStartLogging());
  for (int i = 0; i < 20; ++i) {  // kLogCountLimit
    webrtc_log_uploader->LoggingStoppedDontUpload();
  }

  webrtc_log_uploader->Shutdown();
  FlushRunLoop();
}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
