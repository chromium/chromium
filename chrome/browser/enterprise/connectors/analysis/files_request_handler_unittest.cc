// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_features.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/fake_files_request_handler.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kDmToken[] = "dm_token";
constexpr char kUserActionId[] = "123";
constexpr char kTabTitle[] = "tab_title";
constexpr char kContentTransferMethod[] = "content_transfer_method";
constexpr char kTestUrl[] = "http://example.com/";
base::TimeDelta kResponseDelay = base::Seconds(0);

constexpr char kBlockingScansForDlpAndMalware[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp", "malware"]
    }
  ],
  "block_until_verdict": 1
})";

constexpr char kBlockingScansForDlp[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp"]
    }
  ],
  "block_until_verdict": 1
})";

constexpr char kBlockingScansForMalware[] = R"(
{
  "service_provider": "google",
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp"]
    }
  ],
  "block_until_verdict": 1
})";

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
constexpr char kLocalServiceProvider[] = R"(
{
  "service_provider": "local_user_agent",
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp"]
    }
  ]
})";
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

constexpr char kNothingEnabled[] = R"({ "service_provider": "google" })";

class ScopedSetDMToken {
 public:
  explicit ScopedSetDMToken(const policy::DMToken& dm_token) {
    SetDMTokenForTesting(dm_token);
  }
  ~ScopedSetDMToken() {
    SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
  }
};

class BaseTest : public testing::Test {
 public:
  BaseTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  [[nodiscard]] std::vector<base::FilePath> CreateFilesForTest(
      const std::vector<base::FilePath::StringType>& file_names,
      const std::string& content = "content") {
    std::vector<base::FilePath> paths;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    for (const auto& file_name : file_names) {
      base::FilePath path = temp_dir_.GetPath().Append(file_name);
      base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      file.WriteAtCurrentPos(base::as_byte_span(content));
      paths.emplace_back(path);
    }
    return paths;
  }

  Profile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<test::FakeFilesRequestHandler> fake_files_request_handler_;
};

MATCHER_P3(MatchesRequestHandlerResult, complies, final_result, tag, "") {
  bool complies_matches = arg.complies == complies;
  bool final_result_matches = arg.final_result == final_result;
  bool tag_matches = arg.tag == tag;

  return complies_matches && final_result_matches && tag_matches;
}

}  // namespace

// Make a RequestHandlerResult show nicely in google tests.
// It's important that PrintTo() is defined in the SAME namespace that defines
// RequestHandlerResult.  C++'s look-up rules rely on that. Additionally, it
// cannot go into the anonymous namespace.
void PrintTo(const RequestHandlerResult& request_handler_result,
             std::ostream* os) {
  *os << "RequestHandlerResult: (";
  *os << "complies: " << (request_handler_result.complies ? "true" : "false")
      << ", ";

  *os << "final_result: "
      << static_cast<int>(request_handler_result.final_result) << "(";
  switch (request_handler_result.final_result) {
    case FinalContentAnalysisResult::FAILURE:
      *os << "FAILURE";
      break;
    case FinalContentAnalysisResult::FAIL_CLOSED:
      *os << "FAIL_CLOSED";
      break;
    case FinalContentAnalysisResult::LARGE_FILES:
      *os << "LARGE_FILES";
      break;
    case FinalContentAnalysisResult::ENCRYPTED_FILES:
      *os << "ENCRYPTED_FILES";
      break;
    case FinalContentAnalysisResult::WARNING:
      *os << "WARNING";
      break;
    case FinalContentAnalysisResult::SUCCESS:
      *os << "SUCCESS";
      break;
  }
  *os << "), tag: \"" << request_handler_result.tag << "\")";
}

class FilesRequestHandlerTest : public BaseTest {
 public:
  FilesRequestHandlerTest() = default;

 protected:
  std::optional<std::vector<RequestHandlerResult>> ScanUpload(
      const std::vector<base::FilePath>& paths) {
    // The settings need to exist until the "scanning" has completed, we can
    // thus not pass it into test::FakeFilesRequestHandler as a rvalue
    // reference.
    std::optional<AnalysisSettings> settings = GetSettings();
    if (!settings.has_value()) {
      return std::nullopt;
    }

    using ResultFuture =
        base::test::TestFuture<std::vector<RequestHandlerResult>>;
    ResultFuture future;

    // The access point is only used for metrics, so its value doesn't affect
    // the tests in this file and can always be the same.
    fake_files_request_handler_ =
        std::make_unique<test::FakeFilesRequestHandler>(
            base::BindRepeating(
                &FilesRequestHandlerTest::FakeFileUploadCallback,
                weak_ptr_factory_.GetWeakPtr(),
                settings->cloud_or_local_settings.is_cloud_analysis()),
            /*upload_service=*/nullptr, profile_, *settings, GURL(kTestUrl), "",
            "", kUserActionId, kTabTitle, kContentTransferMethod,
            safe_browsing::DeepScanAccessPoint::UPLOAD,
            ContentAnalysisRequest::FILE_PICKER_DIALOG, paths,
            future.GetCallback());

    fake_files_request_handler_->UploadData();

    EXPECT_TRUE(future.Wait()) << "Scanning did not finish successfully";

    EXPECT_GE(paths.size(),
              fake_files_request_handler_->request_tokens_to_ack_final_actions()
                  .size());
    for (const auto& token_and_action :
         fake_files_request_handler_->request_tokens_to_ack_final_actions()) {
      EXPECT_FALSE(token_and_action.first.empty());
    }

    return future.Take();
  }

  std::optional<enterprise_connectors::AnalysisSettings> GetSettings() {
    auto* service =
        enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
            profile());
    // If the corresponding Connector policy isn't set, no scans can be
    // performed.
    if (!service) {
      return std::nullopt;
    }
    EXPECT_TRUE(service->IsConnectorEnabled(AnalysisConnector::FILE_ATTACHED));

    // Check that `url` matches the appropriate URL patterns by getting
    // settings. No settings means no matches were found.
    return service->GetAnalysisSettings(GURL(kTestUrl),
                                        AnalysisConnector::FILE_ATTACHED);
  }

  void SetDLPResponse(ContentAnalysisResponse response) {
    dlp_response_ = std::move(response);
  }

  void SetExpectedUserActionRequestsCount(uint64_t requests_count) {
    expected_user_action_requests_count_ = requests_count;
  }

  void PathFailsDeepScan(base::FilePath path,
                         ContentAnalysisResponse response) {
    failures_.insert({std::move(path), std::move(response)});
  }

  void SetScanPolicies(bool dlp, bool malware) {
    include_dlp_ = dlp;
    include_malware_ = malware;

    if (include_dlp_ && include_malware_) {
      enterprise_connectors::test::SetAnalysisConnector(
          profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
          kBlockingScansForDlpAndMalware);
    } else if (include_dlp_) {
      enterprise_connectors::test::SetAnalysisConnector(
          profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
          kBlockingScansForDlp);
    } else if (include_malware_) {
      enterprise_connectors::test::SetAnalysisConnector(
          profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
          kBlockingScansForMalware);
    } else {
      enterprise_connectors::test::SetAnalysisConnector(
          profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
          kNothingEnabled);
    }
  }

  void SetUp() override {
    BaseTest::SetUp();

    enterprise_connectors::test::SetAnalysisConnector(
        profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
        kBlockingScansForDlpAndMalware);
  }

  void FakeFileUploadCallback(
      bool is_cloud_analysis,
      safe_browsing::BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
      test::FakeFilesRequestHandler::FakeFileRequestCallback callback) {
    EXPECT_FALSE(path.empty());
    if (is_cloud_analysis) {
      EXPECT_EQ(request->device_token(), kDmToken);
    } else {
      EXPECT_EQ(request->user_action_requests_count(),
                expected_user_action_requests_count_);
      EXPECT_EQ(request->user_action_id(), kUserActionId);
      EXPECT_EQ(request->tab_title(), kTabTitle);
    }

    upload_performed_ = true;

    // Simulate a response.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), path, result,
                       ConnectorStatusCallback(path)),
        kResponseDelay);
  }

  ContentAnalysisResponse ConnectorStatusCallback(const base::FilePath& path) {
    // The path succeeds if it is not in the `failures_` maps.
    auto it = failures_.find(path);
    ContentAnalysisResponse response =
        it != failures_.end()
            ? it->second
            : test::FakeContentAnalysisDelegate::SuccessfulResponse([this]() {
                std::set<std::string> tags;
                if (include_dlp_ && !dlp_response_.has_value()) {
                  tags.insert("dlp");
                }
                if (include_malware_) {
                  tags.insert("malware");
                }
                return tags;
              }());

    if (include_dlp_ && dlp_response_.has_value()) {
      *response.add_results() = dlp_response_.value().results(0);
    }

    // Set any non empty request token.
    response.set_request_token("request_token");
    return response;
  }

  bool was_upload_performed() { return upload_performed_; }

 private:
  ScopedSetDMToken scoped_dm_token_{
      policy::DMToken::CreateValidToken(kDmToken)};
  bool include_dlp_ = true;
  bool include_malware_ = true;

  // Paths in this map will be considered to have failed deep scan checks.
  // The actual failure response is given for each path.
  std::map<base::FilePath, ContentAnalysisResponse> failures_;

  // DLP response to ovewrite in the callback if present.
  std::optional<ContentAnalysisResponse> dlp_response_ = std::nullopt;

  // To verify user action requests count in local content analysis request is
  // set correctly.
  uint64_t expected_user_action_requests_count_ = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
  bool upload_performed_ = false;

  base::WeakPtrFactory<FilesRequestHandlerTest> weak_ptr_factory_{this};
};

TEST_F(FilesRequestHandlerTest, Empty) {
  GURL url(kTestUrl);
  std::vector<base::FilePath> paths;

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(0u, results->size());
}

TEST_F(FilesRequestHandlerTest, ZeroLengthFileSucceeds) {
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths =
      CreateFilesForTest({FILE_PATH_LITERAL("zerolength.doc")}, "");
  PathFailsDeepScan(
      paths[0],
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::BLOCK));

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());

  EXPECT_EQ(1u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
}

TEST_F(FilesRequestHandlerTest, FileDataPositiveMalwareAndDlpVerdicts) {
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths =
      CreateFilesForTest({FILE_PATH_LITERAL("foo.doc")});

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());

  EXPECT_EQ(1u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
}

TEST_F(FilesRequestHandlerTest, FileDataPositiveMalwareAndDlpVerdicts2) {
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")});

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(2u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
  EXPECT_THAT((*results)[1],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
}

TEST_F(FilesRequestHandlerTest, FileDataPositiveMalwareVerdict) {
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")});

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(2u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
  EXPECT_THAT((*results)[1],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
}

TEST_F(FilesRequestHandlerTest, FileIsEncrypted) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
      R"(
    {
      "service_provider": "google",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ],
      "block_until_verdict": 1,
      "block_password_protected": true
    })");
  GURL url(kTestUrl);
  std::vector<base::FilePath> paths;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");
  paths.emplace_back(test_zip);

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(1u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  false, FinalContentAnalysisResult::ENCRYPTED_FILES, ""));
  EXPECT_FALSE(was_upload_performed());
}

// With a local service provider, a scan should not terminate early due to
// encryption.
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
TEST_F(FilesRequestHandlerTest, FileIsEncrypted_LocalAnalysis) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
      kLocalServiceProvider);
  GURL url(kTestUrl);
  std::vector<base::FilePath> paths;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");
  paths.emplace_back(test_zip);
  SetExpectedUserActionRequestsCount(1);

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(1u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
}
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

TEST_F(FilesRequestHandlerTest, FileIsEncrypted_PolicyAllows) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
      R"(
    {
      "service_provider": "google",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ],
      "block_until_verdict": 1,
      "block_password_protected": false
    })");
  GURL url(kTestUrl);
  std::vector<base::FilePath> paths;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");
  paths.emplace_back(test_zip);

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(1u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
  // When the resumable upload protocol is in use and the policy does not block
  // encrypted files by default, the file's metadata is uploaded for scanning.
  EXPECT_TRUE(was_upload_performed());
}

TEST_F(FilesRequestHandlerTest, FileIsLarge) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
      R"(
    {
      "service_provider": "google",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ],
      "block_until_verdict": 1,
      "block_large_files": true
    })");
  GURL url(kTestUrl);
  std::vector<base::FilePath> paths;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("large.doc");
  std::string contents(
      safe_browsing::BinaryUploadService::kMaxUploadSizeBytes + 1, 'a');
  base::WriteFile(file_path, contents);
  paths.emplace_back(file_path);
  SetExpectedUserActionRequestsCount(1);

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(1u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  false, FinalContentAnalysisResult::LARGE_FILES, ""));
  EXPECT_FALSE(was_upload_performed());
}

// With a local service provider, a scan should not terminate early due to
// size.
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
TEST_F(FilesRequestHandlerTest, FileIsLarge_LocalAnalysis) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
      kLocalServiceProvider);
  GURL url(kTestUrl);
  std::vector<base::FilePath> paths;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("large.doc");
  std::string contents(
      safe_browsing::BinaryUploadService::kMaxUploadSizeBytes + 1, 'a');
  base::WriteFile(file_path, contents);
  paths.emplace_back(file_path);
  SetExpectedUserActionRequestsCount(1);

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(1u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
}
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

TEST_F(FilesRequestHandlerTest, FileIsLarge_PolicyAllows) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
      R"(
    {
      "service_provider": "google",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ],
      "block_until_verdict": 1,
      "block_large_files": false
    })");
  GURL url(kTestUrl);
  std::vector<base::FilePath> paths;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("large.doc");
  std::string contents(
      safe_browsing::BinaryUploadService::kMaxUploadSizeBytes + 1, 'a');
  base::WriteFile(file_path, contents);
  paths.emplace_back(file_path);
  SetExpectedUserActionRequestsCount(1);

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(1u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
  // When the resumable upload protocol is in use and the policy does not block
  // large files by default, the file's metadata is uploaded for scanning.
  EXPECT_TRUE(was_upload_performed());
}

// With a local service provider, multiple file uploads should result in
// multiple analysis requests.
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
TEST_F(FilesRequestHandlerTest, MultipleFilesUpload_LocalAnalysis) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
      kLocalServiceProvider);
  GURL url(kTestUrl);
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")});
  SetExpectedUserActionRequestsCount(2);

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(2u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
  EXPECT_THAT((*results)[1],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
}
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

TEST_F(FilesRequestHandlerTest, FileDataNegativeMalwareVerdict) {
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")});
  PathFailsDeepScan(
      paths[1],
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::BLOCK));

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(2u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
  EXPECT_THAT((*results)[1],
              MatchesRequestHandlerResult(
                  false, FinalContentAnalysisResult::FAILURE, "malware"));
}

TEST_F(FilesRequestHandlerTest, FileDataPositiveDlpVerdict) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")});

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(2u, results->size());

  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));

  EXPECT_THAT((*results)[1],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
}

TEST_F(FilesRequestHandlerTest, FileDataPositiveDlpVerdictDataControls) {
  file_access::MockScopedFileAccessDelegate scoped_files_access_delegate;

  EXPECT_CALL(scoped_files_access_delegate, RequestFilesAccessForSystem)
      .WillOnce(base::test::RunOnceCallback<1>(
          file_access::ScopedFileAccess::Allowed()));

  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")});

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(2u, results->size());

  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));

  EXPECT_THAT((*results)[1],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
}

TEST_F(FilesRequestHandlerTest, FileDataNegativeDlpVerdict) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")});

  PathFailsDeepScan(paths[1], test::FakeContentAnalysisDelegate::DlpResponse(
                                  ContentAnalysisResponse::Result::SUCCESS,
                                  "rule", TriggeredRule::BLOCK));

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(2u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
  EXPECT_THAT((*results)[1],
              MatchesRequestHandlerResult(
                  false, FinalContentAnalysisResult::FAILURE, "dlp"));
}

TEST_F(FilesRequestHandlerTest, FileDataNegativeMalwareAndDlpVerdicts) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/true);
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")});

  PathFailsDeepScan(
      paths[1],
      test::FakeContentAnalysisDelegate::MalwareAndDlpResponse(
          TriggeredRule::BLOCK, ContentAnalysisResponse::Result::SUCCESS,
          "rule", TriggeredRule::BLOCK));

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(2u, results->size());
  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  true, FinalContentAnalysisResult::SUCCESS, ""));
  // In this case, we expect either a "malware" or a "dlp" tag.
  EXPECT_THAT(
      (*results)[1],
      testing::AnyOf(MatchesRequestHandlerResult(
                         false, FinalContentAnalysisResult::FAILURE, "malware"),
                     MatchesRequestHandlerResult(
                         false, FinalContentAnalysisResult::FAILURE, "dlp")));
}

TEST_F(FilesRequestHandlerTest, NoDelay) {
  // Tests that scanning results are independent of block_until_verdict.
  // Note that this behavior is different compared to the
  // ContentAnalysisDelegateUnittest which checks that the Delegate allows
  // access to all data for block_until_verdict==0.
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), AnalysisConnector::FILE_ATTACHED,
      R"(
    {
      "service_provider": "google",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ],
      "block_until_verdict": 0
    })");
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths =
      CreateFilesForTest({FILE_PATH_LITERAL("foo_fail_malware_0.doc"),
                          FILE_PATH_LITERAL("foo_fail_malware_1.doc"),
                          FILE_PATH_LITERAL("foo_fail_malware_2.doc"),
                          FILE_PATH_LITERAL("foo_fail_dlp_status.doc"),
                          FILE_PATH_LITERAL("foo_fail_dlp_rule.doc")});

  // Mark all files and text with failed scans.
  SetDLPResponse(test::FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));
  PathFailsDeepScan(
      paths[0],
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::BLOCK));
  PathFailsDeepScan(
      paths[1],
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::WARN));
  PathFailsDeepScan(
      paths[2],
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::BLOCK));
  PathFailsDeepScan(paths[3], test::FakeContentAnalysisDelegate::DlpResponse(
                                  ContentAnalysisResponse::Result::FAILURE, "",
                                  TriggeredRule::REPORT_ONLY));
  PathFailsDeepScan(paths[4], test::FakeContentAnalysisDelegate::DlpResponse(
                                  ContentAnalysisResponse::Result::SUCCESS,
                                  "dlp", TriggeredRule::BLOCK));

  auto results = ScanUpload(paths);
  ASSERT_TRUE(results.has_value());
  EXPECT_EQ(5u, results->size());

  EXPECT_THAT((*results)[0],
              MatchesRequestHandlerResult(
                  false, FinalContentAnalysisResult::FAILURE, "malware"));
  // Dlp response (block) should overrule malware response (warning).
  EXPECT_THAT((*results)[1],
              MatchesRequestHandlerResult(
                  false, FinalContentAnalysisResult::FAILURE, "dlp"));
  EXPECT_THAT((*results)[2],
              MatchesRequestHandlerResult(
                  false, FinalContentAnalysisResult::FAILURE, "malware"));
  EXPECT_THAT((*results)[3],
              MatchesRequestHandlerResult(
                  false, FinalContentAnalysisResult::FAILURE, "dlp"));
  EXPECT_THAT((*results)[4],
              MatchesRequestHandlerResult(
                  false, FinalContentAnalysisResult::FAILURE, "dlp"));
}

}  // namespace enterprise_connectors
