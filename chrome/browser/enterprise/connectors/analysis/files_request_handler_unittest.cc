// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_files_request_handler.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kDmToken[] = "dm_token";
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

constexpr char kLocalServiceProvider[] = R"(
{
  "service_provider": "local_test",
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp"]
    }
  ]
})";

constexpr char kNothingEnabled[] = R"({ "service_provider": "google" })";

class ScopedSetDMToken {
 public:
  explicit ScopedSetDMToken(const policy::DMToken& dm_token) {
    SetDMTokenForTesting(dm_token);
  }
  ~ScopedSetDMToken() {
    SetDMTokenForTesting(policy::DMToken::CreateEmptyTokenForTesting());
  }
};

class BaseTest : public testing::Test {
 public:
  BaseTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void EnableFeatures() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
  }

  void DisableFeatures() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({}, {kEnterpriseConnectorsEnabled});
  }

  [[nodiscard]] std::vector<base::FilePath> CreateFilesForTest(
      const std::vector<base::FilePath::StringType>& file_names,
      const std::string& content = "content") {
    std::vector<base::FilePath> paths;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    for (const auto& file_name : file_names) {
      base::FilePath path = temp_dir_.GetPath().Append(file_name);
      base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      file.WriteAtCurrentPos(content.data(), content.size());
      paths.emplace_back(path);
    }
    return paths;
  }

  Profile* profile() { return profile_; }

  void RunUntilDone() { run_loop_.Run(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FakeFilesRequestHandler> fake_files_request_handler_;
  base::RunLoop run_loop_;
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
  void ScanUpload(const std::vector<base::FilePath>& paths,
                  FilesRequestHandler::CompletionCallback callback) {
    // The settings need to exist until the "scanning" has completed, we can
    // thus not pass it into FakeFilesRequestHandler as a rvalue reference.
    AnalysisSettings settings = GetSettings();

    // The access point is only used for metrics, so its value doesn't affect
    // the tests in this file and can always be the same.
    fake_files_request_handler_ = std::make_unique<FakeFilesRequestHandler>(
        base::BindRepeating(
            &FilesRequestHandlerTest::FakeFileUploadCallback,
            weak_ptr_factory_.GetWeakPtr(),
            settings.cloud_or_local_settings.is_cloud_analysis()),
        /*upload_service=*/nullptr, profile_, settings, GURL(kTestUrl),
        safe_browsing::DeepScanAccessPoint::UPLOAD, paths,
        base::BindOnce(
            [](base::RepeatingClosure runloop_closure,
               FilesRequestHandler::CompletionCallback callback,
               std::vector<RequestHandlerResult> result) {
              std::move(callback).Run(std::move(result));
              runloop_closure.Run();
            },
            run_loop_.QuitClosure(), std::move(callback)));

    fake_files_request_handler_->UploadData();
    RunUntilDone();
  }

  enterprise_connectors::AnalysisSettings GetSettings() {
    auto* service =
        enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
            profile());
    // If the corresponding Connector policy isn't set, no scans can be
    // performed.
    EXPECT_TRUE(service);
    EXPECT_TRUE(service->IsConnectorEnabled(AnalysisConnector::FILE_ATTACHED));

    // Check that `url` matches the appropriate URL patterns by getting
    // settings. No settings means no matches were found.
    auto settings = service->GetAnalysisSettings(
        GURL(kTestUrl), AnalysisConnector::FILE_ATTACHED);
    EXPECT_TRUE(settings.has_value());
    return std::move(settings.value());
  }

  void SetDLPResponse(ContentAnalysisResponse response) {
    dlp_response_ = std::move(response);
  }

  void PathFailsDeepScan(base::FilePath path,
                         ContentAnalysisResponse response) {
    failures_.insert({std::move(path), std::move(response)});
  }

  void SetScanPolicies(bool dlp, bool malware) {
    include_dlp_ = dlp;
    include_malware_ = malware;

    if (include_dlp_ && include_malware_) {
      safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                          AnalysisConnector::FILE_ATTACHED,
                                          kBlockingScansForDlpAndMalware);
    } else if (include_dlp_) {
      safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                          AnalysisConnector::FILE_ATTACHED,
                                          kBlockingScansForDlp);
    } else if (include_malware_) {
      safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                          AnalysisConnector::FILE_ATTACHED,
                                          kBlockingScansForMalware);
    } else {
      safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                          AnalysisConnector::FILE_ATTACHED,
                                          kNothingEnabled);
    }
  }

  void SetUp() override {
    BaseTest::SetUp();

    EnableFeatures();
    safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                        AnalysisConnector::FILE_ATTACHED,
                                        kBlockingScansForDlpAndMalware);
  }

  void FakeFileUploadCallback(
      bool is_cloud_analysis,
      safe_browsing::BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request) {
    EXPECT_FALSE(path.empty());
    if (is_cloud_analysis)
      EXPECT_EQ(request->device_token(), kDmToken);

    // Simulate a response.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FilesRequestHandler::FileRequestCallbackForTesting,
                       fake_files_request_handler_->GetWeakPtr(), path,
                       safe_browsing::BinaryUploadService::Result::SUCCESS,
                       ConnectorStatusCallback(path)),
        kResponseDelay);
  }

  ContentAnalysisResponse ConnectorStatusCallback(const base::FilePath& path) {
    // The path succeeds if it is not in the `failures_` maps.
    auto it = failures_.find(path);
    ContentAnalysisResponse response =
        it != failures_.end()
            ? it->second
            : FakeContentAnalysisDelegate::SuccessfulResponse([this]() {
                std::set<std::string> tags;
                if (include_dlp_ && !dlp_response_.has_value())
                  tags.insert("dlp");
                if (include_malware_)
                  tags.insert("malware");
                return tags;
              }());

    if (include_dlp_ && dlp_response_.has_value()) {
      *response.add_results() = dlp_response_.value().results(0);
    }

    return response;
  }

 private:
  ScopedSetDMToken scoped_dm_token_{
      policy::DMToken::CreateValidTokenForTesting(kDmToken)};
  bool include_dlp_ = true;
  bool include_malware_ = true;

  // Paths in this map will be considered to have failed deep scan checks.
  // The actual failure response is given for each path.
  std::map<base::FilePath, ContentAnalysisResponse> failures_;

  // DLP response to ovewrite in the callback if present.
  absl::optional<ContentAnalysisResponse> dlp_response_ = absl::nullopt;

  base::WeakPtrFactory<FilesRequestHandlerTest> weak_ptr_factory_{this};
};

TEST_F(FilesRequestHandlerTest, Empty) {
  GURL url(kTestUrl);
  std::vector<base::FilePath> paths;

  bool called = false;
  ScanUpload(paths,
             base::BindOnce(
                 [](bool* called, std::vector<RequestHandlerResult> results) {
                   EXPECT_EQ(0u, results.size());
                   *called = true;
                 },
                 &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, FileDataPositiveMalwareAndDlpVerdicts) {
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths =
      CreateFilesForTest({FILE_PATH_LITERAL("foo.doc")});

  bool called = false;
  ScanUpload(paths,
             base::BindOnce(
                 [](bool* called, std::vector<RequestHandlerResult> results) {
                   EXPECT_EQ(1u, results.size());
                   EXPECT_THAT(
                       results[0],
                       MatchesRequestHandlerResult(
                           true, FinalContentAnalysisResult::SUCCESS, ""));

                   *called = true;
                 },
                 &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, FileDataPositiveMalwareAndDlpVerdicts2) {
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")});

  bool called = false;
  ScanUpload(
      paths,
      base::BindOnce(
          [](bool* called, std::vector<RequestHandlerResult> results) {
            EXPECT_EQ(2u, results.size());
            EXPECT_THAT(results[0],
                        MatchesRequestHandlerResult(
                            true, FinalContentAnalysisResult::SUCCESS, ""));
            EXPECT_THAT(results[1],
                        MatchesRequestHandlerResult(
                            true, FinalContentAnalysisResult::SUCCESS, ""));
            *called = true;
          },
          &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, FileDataPositiveMalwareVerdict) {
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")});

  bool called = false;
  ScanUpload(
      paths,
      base::BindOnce(
          [](bool* called, std::vector<RequestHandlerResult> results) {
            EXPECT_EQ(2u, results.size());
            EXPECT_THAT(results[0],
                        MatchesRequestHandlerResult(
                            true, FinalContentAnalysisResult::SUCCESS, ""));
            EXPECT_THAT(results[1],
                        MatchesRequestHandlerResult(
                            true, FinalContentAnalysisResult::SUCCESS, ""));
            *called = true;
          },
          &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, FileIsEncrypted) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                      AnalysisConnector::FILE_ATTACHED, R"(
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

  bool called = false;
  ScanUpload(
      paths,
      base::BindOnce(
          [](bool* called, std::vector<RequestHandlerResult> results) {
            EXPECT_EQ(1u, results.size());
            EXPECT_THAT(
                results[0],
                MatchesRequestHandlerResult(
                    false, FinalContentAnalysisResult::ENCRYPTED_FILES, ""));
            *called = true;
          },
          &called));

  EXPECT_TRUE(called);
}

// With a local service provider, a scan should not terminate early due to
// encryption.
TEST_F(FilesRequestHandlerTest, FileIsEncrypted_LocalAnalysis) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                      AnalysisConnector::FILE_ATTACHED,
                                      kLocalServiceProvider);
  GURL url(kTestUrl);
  std::vector<base::FilePath> paths;

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");
  paths.emplace_back(test_zip);

  bool called = false;
  ScanUpload(paths,
             base::BindOnce(
                 [](bool* called, std::vector<RequestHandlerResult> results) {
                   EXPECT_EQ(1u, results.size());
                   EXPECT_THAT(
                       results[0],
                       MatchesRequestHandlerResult(
                           true, FinalContentAnalysisResult::SUCCESS, ""));
                   *called = true;
                 },
                 &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, FileIsEncrypted_PolicyAllows) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                      AnalysisConnector::FILE_ATTACHED, R"(
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

  bool called = false;
  ScanUpload(paths,
             base::BindOnce(
                 [](bool* called, std::vector<RequestHandlerResult> results) {
                   EXPECT_EQ(1u, results.size());
                   EXPECT_THAT(
                       results[0],
                       MatchesRequestHandlerResult(
                           true, FinalContentAnalysisResult::SUCCESS, ""));
                   *called = true;
                 },
                 &called));

  EXPECT_TRUE(called);
}

// With a local service provider, a scan should not terminate early due to
// size.
TEST_F(FilesRequestHandlerTest, FileIsLarge_LocalAnalysis) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                      AnalysisConnector::FILE_ATTACHED,
                                      kLocalServiceProvider);
  GURL url(kTestUrl);
  std::vector<base::FilePath> paths;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path = temp_dir.GetPath().AppendASCII("large.doc");
  std::string contents(
      safe_browsing::BinaryUploadService::kMaxUploadSizeBytes + 1, 'a');
  base::WriteFile(file_path, contents.data(), contents.size());
  paths.emplace_back(file_path);

  bool called = false;
  ScanUpload(paths,
             base::BindOnce(
                 [](bool* called, std::vector<RequestHandlerResult> results) {
                   EXPECT_EQ(1u, results.size());
                   EXPECT_THAT(
                       results[0],
                       MatchesRequestHandlerResult(
                           true, FinalContentAnalysisResult::SUCCESS, ""));
                   *called = true;
                 },
                 &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, FileDataNegativeMalwareVerdict) {
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")});
  PathFailsDeepScan(paths[1], FakeContentAnalysisDelegate::MalwareResponse(
                                  TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(
      paths,
      base::BindOnce(
          [](bool* called, std::vector<RequestHandlerResult> results) {
            EXPECT_EQ(2u, results.size());
            EXPECT_THAT(results[0],
                        MatchesRequestHandlerResult(
                            true, FinalContentAnalysisResult::SUCCESS, ""));
            EXPECT_THAT(
                results[1],
                MatchesRequestHandlerResult(
                    false, FinalContentAnalysisResult::FAILURE, "malware"));
            *called = true;
          },
          &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, FileDataPositiveDlpVerdict) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")});

  bool called = false;
  ScanUpload(
      paths,
      base::BindOnce(
          [](bool* called, std::vector<RequestHandlerResult> results) {
            EXPECT_EQ(2u, results.size());

            EXPECT_THAT(results[0],
                        MatchesRequestHandlerResult(
                            true, FinalContentAnalysisResult::SUCCESS, ""));

            EXPECT_THAT(results[1],
                        MatchesRequestHandlerResult(
                            true, FinalContentAnalysisResult::SUCCESS, ""));

            *called = true;
          },
          &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, FileDataNegativeDlpVerdict) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")});

  PathFailsDeepScan(paths[1], FakeContentAnalysisDelegate::DlpResponse(
                                  ContentAnalysisResponse::Result::SUCCESS,
                                  "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(
      paths,
      base::BindOnce(
          [](bool* called, std::vector<RequestHandlerResult> results) {
            EXPECT_EQ(2u, results.size());
            EXPECT_THAT(results[0],
                        MatchesRequestHandlerResult(
                            true, FinalContentAnalysisResult::SUCCESS, ""));
            EXPECT_THAT(results[1],
                        MatchesRequestHandlerResult(
                            false, FinalContentAnalysisResult::FAILURE, "dlp"));
            *called = true;
          },
          &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, FileDataNegativeMalwareAndDlpVerdicts) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/true);
  GURL url(kTestUrl);

  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")});

  PathFailsDeepScan(
      paths[1],
      FakeContentAnalysisDelegate::MalwareAndDlpResponse(
          TriggeredRule::BLOCK, ContentAnalysisResponse::Result::SUCCESS,
          "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(
      paths,
      base::BindOnce(
          [](bool* called, std::vector<RequestHandlerResult> results) {
            EXPECT_EQ(2u, results.size());
            EXPECT_THAT(results[0],
                        MatchesRequestHandlerResult(
                            true, FinalContentAnalysisResult::SUCCESS, ""));
            // In this case, we expect either a "malware" or a "dlp" tag.
            EXPECT_THAT(
                results[1],
                testing::AnyOf(
                    MatchesRequestHandlerResult(
                        false, FinalContentAnalysisResult::FAILURE, "malware"),
                    MatchesRequestHandlerResult(
                        false, FinalContentAnalysisResult::FAILURE, "dlp")));
            *called = true;
          },
          &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, NoDelay) {
  // Tests that scanning results are independent of block_until_verdict.
  // Note that this behavior is different compared to the
  // ContentAnalysisDelegateUnittest which checks that the Delegate allows
  // access to all data for block_until_verdict==0.
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                      AnalysisConnector::FILE_ATTACHED, R"(
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
  SetDLPResponse(FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));
  PathFailsDeepScan(paths[0], FakeContentAnalysisDelegate::MalwareResponse(
                                  TriggeredRule::BLOCK));
  PathFailsDeepScan(paths[1], FakeContentAnalysisDelegate::MalwareResponse(
                                  TriggeredRule::WARN));
  PathFailsDeepScan(paths[2], FakeContentAnalysisDelegate::MalwareResponse(
                                  TriggeredRule::BLOCK));
  PathFailsDeepScan(paths[3], FakeContentAnalysisDelegate::DlpResponse(
                                  ContentAnalysisResponse::Result::FAILURE, "",
                                  TriggeredRule::REPORT_ONLY));
  PathFailsDeepScan(paths[4], FakeContentAnalysisDelegate::DlpResponse(
                                  ContentAnalysisResponse::Result::SUCCESS,
                                  "dlp", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(
      paths,
      base::BindOnce(
          [](bool* called, std::vector<RequestHandlerResult> results) {
            EXPECT_EQ(5u, results.size());

            EXPECT_THAT(
                results[0],
                MatchesRequestHandlerResult(
                    false, FinalContentAnalysisResult::FAILURE, "malware"));
            // Dlp response (block) should overrule malware response (warning).
            EXPECT_THAT(results[1],
                        MatchesRequestHandlerResult(
                            false, FinalContentAnalysisResult::FAILURE, "dlp"));
            EXPECT_THAT(
                results[2],
                MatchesRequestHandlerResult(
                    false, FinalContentAnalysisResult::FAILURE, "malware"));
            EXPECT_THAT(results[3],
                        MatchesRequestHandlerResult(
                            false, FinalContentAnalysisResult::FAILURE, "dlp"));
            EXPECT_THAT(results[4],
                        MatchesRequestHandlerResult(
                            false, FinalContentAnalysisResult::FAILURE, "dlp"));

            *called = true;
          },
          &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, SupportedTypes) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  GURL url(kTestUrl);

  std::vector<base::FilePath::StringType> file_names;
  for (const base::FilePath::StringType& supported_type :
       {FILE_PATH_LITERAL(".7z"),   FILE_PATH_LITERAL(".bz2"),
        FILE_PATH_LITERAL(".bzip"), FILE_PATH_LITERAL(".cab"),
        FILE_PATH_LITERAL(".csv"),  FILE_PATH_LITERAL(".doc"),
        FILE_PATH_LITERAL(".docx"), FILE_PATH_LITERAL(".eps"),
        FILE_PATH_LITERAL(".gz"),   FILE_PATH_LITERAL(".gzip"),
        FILE_PATH_LITERAL(".htm"),  FILE_PATH_LITERAL(".html"),
        FILE_PATH_LITERAL(".odt"),  FILE_PATH_LITERAL(".pdf"),
        FILE_PATH_LITERAL(".ppt"),  FILE_PATH_LITERAL(".pptx"),
        FILE_PATH_LITERAL(".ps"),   FILE_PATH_LITERAL(".rar"),
        FILE_PATH_LITERAL(".rtf"),  FILE_PATH_LITERAL(".tar"),
        FILE_PATH_LITERAL(".txt"),  FILE_PATH_LITERAL(".wpd"),
        FILE_PATH_LITERAL(".xls"),  FILE_PATH_LITERAL(".xlsx"),
        FILE_PATH_LITERAL(".xps"),  FILE_PATH_LITERAL(".zip")}) {
    file_names.push_back(base::FilePath::StringType(FILE_PATH_LITERAL("foo")) +
                         supported_type);
  }
  std::vector<base::FilePath> paths = CreateFilesForTest(file_names);

  // Mark all files with failed scans.
  for (const auto& path : paths) {
    PathFailsDeepScan(path, FakeContentAnalysisDelegate::MalwareResponse(
                                TriggeredRule::BLOCK));
  }

  bool called = false;
  ScanUpload(paths,
             base::BindOnce(
                 [](bool* called, std::vector<RequestHandlerResult> results) {
                   EXPECT_EQ(26u, results.size());

                   for (const auto& result : results) {
                     EXPECT_THAT(result,
                                 MatchesRequestHandlerResult(
                                     false, FinalContentAnalysisResult::FAILURE,
                                     "malware"));
                   }

                   *called = true;
                 },
                 &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, UnsupportedTypesDefaultPolicy) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);

  // The file content bytes correspond to an unsupported type (png) so that
  // sniffing doesn't indicate the file is supported.
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.these"), FILE_PATH_LITERAL("foo.file"),
       FILE_PATH_LITERAL("foo.types"), FILE_PATH_LITERAL("foo.are"),
       FILE_PATH_LITERAL("foo.not"), FILE_PATH_LITERAL("foo.supported")},
      /*content*/ "\x89PNG\x0D\x0A\x1A\x0A");

  // Mark all files with failed scans.
  for (const auto& path : paths) {
    PathFailsDeepScan(path, FakeContentAnalysisDelegate::DlpResponse(
                                ContentAnalysisResponse::Result::SUCCESS,
                                "rule", TriggeredRule::WARN));
  }

  bool called = false;
  ScanUpload(paths,
             base::BindOnce(
                 [](bool* called, std::vector<RequestHandlerResult> results) {
                   EXPECT_EQ(6u, results.size());

                   // The unsupported types should be marked as compliant since
                   // the default policy behavior is to allow them through.
                   for (const auto& result : results)
                     EXPECT_THAT(
                         result,
                         MatchesRequestHandlerResult(
                             true, FinalContentAnalysisResult::SUCCESS, ""));
                   *called = true;
                 },
                 &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, UnsupportedTypesBlockPolicy) {
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(),
                                      AnalysisConnector::FILE_ATTACHED, R"(
    {
      "service_provider": "google",
      "enable": [
        {
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }
      ],
      "block_until_verdict": 1,
      "block_unsupported_file_types": true
    })");
  GURL url(kTestUrl);

  // The file content bytes correspond to an unsupported type (png) so that
  // sniffing doesn't indicate the file is supported.
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.these"), FILE_PATH_LITERAL("foo.file"),
       FILE_PATH_LITERAL("foo.types"), FILE_PATH_LITERAL("foo.are"),
       FILE_PATH_LITERAL("foo.not"), FILE_PATH_LITERAL("foo.supported")},
      /*content*/ "\x89PNG\x0D\x0A\x1A\x0A");

  // Mark all files with failed scans.
  for (const auto& path : paths) {
    PathFailsDeepScan(path, FakeContentAnalysisDelegate::DlpResponse(
                                ContentAnalysisResponse::Result::SUCCESS,
                                "rule", TriggeredRule::WARN));
  }

  bool called = false;
  ScanUpload(paths,
             base::BindOnce(
                 [](bool* called, std::vector<RequestHandlerResult> results) {
                   ASSERT_EQ(6u, results.size());

                   // The unsupported types should be marked as non-compliant
                   // since the block policy behavior is to not allow them
                   // through. There also shouldn't be a tag, as the file type
                   // is unsupported.
                   for (const auto& result : results)
                     EXPECT_THAT(
                         result,
                         MatchesRequestHandlerResult(
                             false, FinalContentAnalysisResult::FAILURE, ""));
                   *called = true;
                 },
                 &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, SupportedAndUnsupportedTypes) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);

  // Only 3 of these file types are supported (bzip, cab and doc). They are
  // mixed in the list so as to show that insertion order does not matter. The
  // file content bytes correspond to an unsupported type (png) so that sniffing
  // doesn't indicate the file is supported.
  std::vector<base::FilePath> paths = CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.bzip"), FILE_PATH_LITERAL("foo.these"),
       FILE_PATH_LITERAL("foo.file"), FILE_PATH_LITERAL("foo.types"),
       FILE_PATH_LITERAL("foo.cab"), FILE_PATH_LITERAL("foo.are"),
       FILE_PATH_LITERAL("foo.not"), FILE_PATH_LITERAL("foo.supported"),
       FILE_PATH_LITERAL("foo_no_extension"), FILE_PATH_LITERAL("foo.doc")},
      /*content*/ "\x89PNG\x0D\x0A\x1A\x0A");

  // Mark all files with failed scans.
  for (const auto& path : paths) {
    PathFailsDeepScan(path, FakeContentAnalysisDelegate::DlpResponse(
                                ContentAnalysisResponse::Result::SUCCESS,
                                "rule", TriggeredRule::BLOCK));
  }

  bool called = false;
  ScanUpload(
      paths,
      base::BindOnce(
          [](bool* called, std::vector<RequestHandlerResult> results) {
            ASSERT_EQ(10u, results.size());

            // The unsupported types should be marked as compliant, and the
            // valid types as non-compliant since they are marked as failed
            // scans.
            std::set<size_t> supported_indices = {0, 4, 9};
            for (size_t index = 0; index < results.size(); ++index) {
              if (base::Contains(supported_indices, index)) {
                EXPECT_THAT(
                    results[index],
                    MatchesRequestHandlerResult(
                        false, FinalContentAnalysisResult::FAILURE, "dlp"));
              } else {
                EXPECT_THAT(results[index],
                            MatchesRequestHandlerResult(
                                true, FinalContentAnalysisResult::SUCCESS, ""));
              }
            }
            *called = true;
          },
          &called));

  EXPECT_TRUE(called);
}

TEST_F(FilesRequestHandlerTest, UnsupportedTypeAndDLPFailure) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);

  // The file content bytes correspond to an unsupported type (png) so that
  // sniffing doesn't indicate the file is supported.
  std::vector<base::FilePath> paths =
      CreateFilesForTest({FILE_PATH_LITERAL("foo.unsupported_extension"),
                          FILE_PATH_LITERAL("dlp_fail.doc")},
                         /*content*/ "\x89PNG\x0D\x0A\x1A\x0A");

  // Mark DLP as failure.
  SetDLPResponse(FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(
      paths,
      base::BindOnce(
          [](bool* called, std::vector<RequestHandlerResult> results) {
            EXPECT_EQ(2u, results.size());

            // The unsupported type file should be marked as compliant, and
            // the valid type file as non-compliant.
            EXPECT_THAT(results[0],
                        MatchesRequestHandlerResult(
                            true, FinalContentAnalysisResult::SUCCESS, ""));
            EXPECT_THAT(results[1],
                        MatchesRequestHandlerResult(
                            false, FinalContentAnalysisResult::FAILURE, "dlp"));
            *called = true;
          },
          &called));

  EXPECT_TRUE(called);
}

}  // namespace enterprise_connectors
