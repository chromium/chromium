// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_manager.h"  // nogncheck
#endif

namespace enterprise_connectors {

namespace {

constexpr char kDmToken[] = "dm_token";
constexpr char kTestUrl[] = "http://example.com/";

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

constexpr char kNothingEnabled[] = R"({ "service_provider": "google" })";

// Helpers to get text with sizes relative to the minimum required size of 100
// bytes for scans to trigger.
std::string large_text() {
  return std::string(100, 'a');
}

std::string small_text() {
  return "random small text";
}

base::ReadOnlySharedMemoryRegion create_page(size_t size) {
  base::MappedReadOnlyRegion page =
      base::ReadOnlySharedMemoryRegion::Create(size);
  std::ranges::fill(base::span(page.mapping), 'a');
  return std::move(page.region);
}

base::ReadOnlySharedMemoryRegion normal_page() {
  return create_page(1024);
}

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
    ContentAnalysisDelegate::DisableUIForTesting();
  }

  void ScanUpload(content::WebContents* web_contents,
                  ContentAnalysisDelegate::Data data,
                  ContentAnalysisDelegate::CompletionCallback callback) {
    // The access point is only used for metrics and choosing the dialog text if
    // one is shown, so its value doesn't affect the tests in this file and can
    // always be the same.
    ContentAnalysisDelegate::CreateForWebContents(
        web_contents, std::move(data), std::move(callback),
        safe_browsing::DeepScanAccessPoint::UPLOAD);
  }

  void CreateFilesForTest(
      const std::vector<base::FilePath::StringType>& file_names,
      ContentAnalysisDelegate::Data* data,
      const std::string& content = "content") {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    for (const auto& file_name : file_names) {
      base::FilePath path = temp_dir_.GetPath().Append(file_name);
      base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      file.WriteAtCurrentPos(base::as_byte_span(content));
      data->paths.emplace_back(path);
    }
  }

  Profile* profile() { return profile_; }

  content::WebContents* contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile());
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
  }

  void RunUntilDone() { run_loop_.Run(); }

  void ValidateIsEnabled(const std::string& url,
                         bool expect_dlp,
                         bool expect_malware) {
    ContentAnalysisDelegate::Data data;
    EXPECT_EQ(expect_dlp || expect_malware,
              ContentAnalysisDelegate::IsEnabled(profile(), GURL(url), &data,
                                                 FILE_ATTACHED));
    const auto& tags = data.settings.tags;
    EXPECT_EQ(expect_dlp, tags.find("dlp") != tags.end());
    EXPECT_EQ(expect_malware, tags.find("malware") != tags.end());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::RunLoop run_loop_;
};

}  // namespace

using ContentAnalysisDelegateIsEnabledTest = BaseTest;

TEST_F(ContentAnalysisDelegateIsEnabledTest, NoDMTokenNoPref) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateInvalidToken());

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, NoDMToken) {
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_ATTACHED, kBlockingScansForDlpAndMalware);
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateInvalidToken());

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpNoPref) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpNoPref2) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_ATTACHED, kNothingEnabled);

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpNoPref3) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_DOWNLOADED, kBlockingScansForDlpAndMalware);

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpEnabled) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_ATTACHED, kBlockingScansForDlp);

  ContentAnalysisDelegate::Data data;
  EXPECT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                 FILE_ATTACHED));
  EXPECT_TRUE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpEnabled2) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_ATTACHED, kBlockingScansForDlp);
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_DOWNLOADED, kBlockingScansForDlp);

  ContentAnalysisDelegate::Data data;
  EXPECT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                 FILE_ATTACHED));
  EXPECT_TRUE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpEnabledWithUrl) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_ATTACHED, kBlockingScansForDlp);
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_DOWNLOADED, kBlockingScansForDlp);
  GURL url(kTestUrl);

  ContentAnalysisDelegate::Data data;
  EXPECT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));
  EXPECT_TRUE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
  EXPECT_EQ(kTestUrl, data.url);
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpDisabledByList) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(profile_->GetPrefs(),
                                                    FILE_ATTACHED,
                                                    R"(
        {
          "service_provider": "google",
          "enable": [
            {
              "url_list": ["*"],
              "tags": ["dlp"]
            }
          ],
          "disable": [
            {
              "url_list": ["http://example.com/"],
              "tags": ["dlp"]
            }
          ],
          "block_until_verdict": 1
        })");

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(kTestUrl),
                                                  &data, FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpDisabledByListWithPatterns) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(profile_->GetPrefs(),
                                                    FILE_ATTACHED,
                                                    R"(
        {
          "service_provider": "google",
          "enable": [
            {
              "url_list": ["*"],
              "tags": ["dlp"]
            }
          ],
          "disable": [
            {
              "url_list": [
                "http://example.com/",
                "https://*",
                "devtools://*",
                "*/a/specific/path/",
                "*:1234",
                "*?q=5678"
              ],
              "tags": ["dlp"]
            }
          ],
          "block_until_verdict": 1
        })");

  ValidateIsEnabled("http://example.com", /*dlp*/ false, /*malware*/ false);
  ValidateIsEnabled("http://google.com", /*dlp*/ true, /*malware*/ false);
  ValidateIsEnabled("https://google.com", /*dlp*/ false, /*malware*/ false);
  ValidateIsEnabled("custom://google.com", /*dlp*/ true, /*malware*/ false);
  ValidateIsEnabled("custom://version", /*dlp*/ true, /*malware*/ false);
  ValidateIsEnabled("devtools://devtools/bundled/inspector.html", /*dlp*/ false,
                    /*malware*/ false);
  ValidateIsEnabled("custom://devtools/bundled/inspector.html", /*dlp*/ true,
                    /*malware*/ false);
  ValidateIsEnabled("http://google.com/a/specific/path/", /*dlp*/ false,
                    /*malware*/ false);
  ValidateIsEnabled("http://google.com/not/a/specific/path/", /*dlp*/ true,
                    /*malware*/ false);
  ValidateIsEnabled("http://google.com:1234", /*dlp*/ false, /*malware*/ false);
  ValidateIsEnabled("http://google.com:4321", /*dlp*/ true, /*malware*/ false);
  ValidateIsEnabled("http://google.com?q=5678", /*dlp*/ false,
                    /*malware*/ false);
  ValidateIsEnabled("http://google.com?q=8765", /*dlp*/ true,
                    /*malware*/ false);
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, MalwareNoPref) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, MalwareNoPref2) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_ATTACHED, kNothingEnabled);

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, MalwareNoPref3) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_DOWNLOADED, kBlockingScansForDlpAndMalware);

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, MalwareEnabled) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(profile_->GetPrefs(),
                                                    FILE_ATTACHED,
                                                    R"(
        {
          "service_provider": "google",
          "enable": [
            {
              "url_list": ["http://example.com/"],
              "tags": ["malware"]
            }
          ],
          "block_until_verdict": 1
        })");

  ContentAnalysisDelegate::Data data;
  EXPECT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(kTestUrl),
                                                 &data, FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_TRUE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, NoScanInIncognito) {
  GURL url(kTestUrl);
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(
      profile_->GetPrefs(), FILE_ATTACHED, kBlockingScansForDlpAndMalware);

  ContentAnalysisDelegate::Data data;
  EXPECT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  // The same URL should not trigger a scan in incognito.
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true), url, &data,
      FILE_ATTACHED));

  // The same URL should not trigger a scan in non-primary OTR profiles
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(
      profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true),
      url, &data, FILE_ATTACHED));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, MalwareEnabledWithPatterns) {
  ScopedSetDMToken scoped_dm_token(policy::DMToken::CreateValidToken(kDmToken));
  enterprise_connectors::test::SetAnalysisConnector(profile_->GetPrefs(),
                                                    FILE_ATTACHED,
                                                    R"(
        {
          "service_provider": "google",
          "enable": [
            {
              "url_list": [
                "http://example.com/",
                "https://*",
                "devtools://*",
                "*/a/specific/path/",
                "*:1234",
                "*?q=5678"
              ],
              "tags": ["malware"]
            }
          ],
          "block_until_verdict": 1
        })");

  ContentAnalysisDelegate::Data data;

  ValidateIsEnabled("http://example.com", /*dlp*/ false, /*malware*/ true);
  ValidateIsEnabled("http://google.com", /*dlp*/ false, /*malware*/ false);
  ValidateIsEnabled("https://google.com", /*dlp*/ false, /*malware*/ true);
  ValidateIsEnabled("custom://google.com", /*dlp*/ false, /*malware*/ false);
  ValidateIsEnabled("custom://version", /*dlp*/ false, /*malware*/ false);
  ValidateIsEnabled("devtools://devtools/bundled/inspector.html", /*dlp*/ false,
                    /*malware*/ true);
  ValidateIsEnabled("custom://devtools/bundled/inspector.html", /*dlp*/ false,
                    /*malware*/ false);
  ValidateIsEnabled("http://google.com/a/specific/path/", /*dlp*/ false,
                    /*malware*/ true);
  ValidateIsEnabled("http://google.com/not/a/specific/path/", /*dlp*/ false,
                    /*malware*/ false);
  ValidateIsEnabled("http://google.com:1234", /*dlp*/ false, /*malware*/ true);
  ValidateIsEnabled("http://google.com:4321", /*dlp*/ false, /*malware*/ false);
  ValidateIsEnabled("http://google.com?q=5678", /*dlp*/ false,
                    /*malware*/ true);
  ValidateIsEnabled("http://google.com?q=8765", /*dlp*/ false,
                    /*malware*/ false);
}

class ContentAnalysisDelegateAuditOnlyTest : public BaseTest {
 public:
  ContentAnalysisDelegateAuditOnlyTest() = default;

 protected:
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

    for (auto connector : {FILE_ATTACHED, BULK_DATA_ENTRY, PRINT}) {
      if (include_dlp_ && include_malware_) {
        enterprise_connectors::test::SetAnalysisConnector(
            profile_->GetPrefs(), connector, kBlockingScansForDlpAndMalware);
      } else if (include_dlp_) {
        enterprise_connectors::test::SetAnalysisConnector(
            profile_->GetPrefs(), connector, kBlockingScansForDlp);
      } else if (include_malware_) {
        enterprise_connectors::test::SetAnalysisConnector(
            profile_->GetPrefs(), connector, kBlockingScansForMalware);
      } else {
        enterprise_connectors::test::SetAnalysisConnector(
            profile_->GetPrefs(), connector, kNothingEnabled);
      }
    }
  }

  void SetUp() override {
    BaseTest::SetUp();

    enterprise_connectors::test::SetAnalysisConnector(
        profile_->GetPrefs(), FILE_ATTACHED, kBlockingScansForDlpAndMalware);
    enterprise_connectors::test::SetAnalysisConnector(
        profile_->GetPrefs(), BULK_DATA_ENTRY, kBlockingScansForDlpAndMalware);
    enterprise_connectors::test::SetAnalysisConnector(
        profile_->GetPrefs(), PRINT, kBlockingScansForDlpAndMalware);

    ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
        &test::FakeContentAnalysisDelegate::Create, run_loop_.QuitClosure(),
        base::BindRepeating(
            &ContentAnalysisDelegateAuditOnlyTest::ConnectorStatusCallback,
            base::Unretained(this)),
        kDmToken));
    test::FakeContentAnalysisDelegate::
        ResetStaticDialogFlagsAndTotalRequestsCount();
  }

  ContentAnalysisResponse ConnectorStatusCallback(const std::string& contents,
                                                  const base::FilePath& path) {
    // The path succeeds if it is not in the |failures_| maps.
    auto it = failures_.find(path);
    ContentAnalysisResponse response =
        it != failures_.end()
            ? it->second
            : test::FakeContentAnalysisDelegate::SuccessfulResponse([this]() {
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
      policy::DMToken::CreateValidToken(kDmToken)};
  bool include_dlp_ = true;
  bool include_malware_ = true;

  // Paths in this map will be consider to have failed deep scan checks.
  // The actual failure response is given for each path.
  std::map<base::FilePath, ContentAnalysisResponse> failures_;

  // DLP response to ovewrite in the callback if present.
  std::optional<ContentAnalysisResponse> dlp_response_ = std::nullopt;
};

TEST_F(ContentAnalysisDelegateAuditOnlyTest, Empty) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  // Keep |data| empty by not setting any text or paths.

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(0,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringData) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(large_text());

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(1u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(1,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringData2) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(large_text());
  data.text.emplace_back(large_text());

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(2u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(2u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   EXPECT_TRUE(result.text_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(1,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringData3) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  // Because the strings are small, they are exempt from scanning and will be
  // allowed even when a negative verdict is mocked.
  data.text.emplace_back(small_text());
  data.text.emplace_back(small_text());

  SetDLPResponse(test::FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(2u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(2u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   EXPECT_TRUE(result.text_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  // Text too small, no analysis request is created.
  EXPECT_EQ(0,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, PagePrintAllowed) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data, PRINT));

  data.page = normal_page();
  ASSERT_TRUE(data.page.IsValid());

  bool called = false;
  ContentAnalysisDelegate::CreateForWebContents(
      contents(), std::move(data),
      base::BindOnce(
          [](bool* called, const ContentAnalysisDelegate::Data& data,
             ContentAnalysisDelegate::Result& result) {
            EXPECT_EQ(0u, data.text.size());
            EXPECT_EQ(0u, data.paths.size());
            // The page data should no longer be valid since it's moved
            // to be uploaded in a request.
            EXPECT_FALSE(data.page.IsValid());
            ASSERT_EQ(0u, result.text_results.size());
            EXPECT_EQ(0u, result.paths_results.size());
            EXPECT_TRUE(result.page_result);
            *called = true;
          },
          &called),
      safe_browsing::DeepScanAccessPoint::PRINT);
  RunUntilDone();
  EXPECT_EQ(1,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, PagePrintBlocked) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data, PRINT));

  data.page = normal_page();
  ASSERT_TRUE(data.page.IsValid());
  SetDLPResponse(test::FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));

  bool called = false;
  ContentAnalysisDelegate::CreateForWebContents(
      contents(), std::move(data),
      base::BindOnce(
          [](bool* called, const ContentAnalysisDelegate::Data& data,
             ContentAnalysisDelegate::Result& result) {
            EXPECT_EQ(0u, data.text.size());
            EXPECT_EQ(0u, data.paths.size());
            // The page data should no longer be valid since it's moved
            // to be uploaded in a request.
            EXPECT_FALSE(data.page.IsValid());
            ASSERT_EQ(0u, result.text_results.size());
            EXPECT_EQ(0u, result.paths_results.size());
            EXPECT_FALSE(result.page_result);
            *called = true;
          },
          &called),
      safe_browsing::DeepScanAccessPoint::PRINT);
  RunUntilDone();
  EXPECT_EQ(1,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest,
       FileDataPositiveMalwareAndDlpVerdicts) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest({FILE_PATH_LITERAL("foo.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(1u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   ASSERT_EQ(1u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(1,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest,
       FileDataPositiveMalwareAndDlpVerdicts2) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   ASSERT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(2,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, FileDataPositiveMalwareVerdict) {
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(2,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, FileIsEncrypted) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  enterprise_connectors::test::SetAnalysisConnector(profile_->GetPrefs(),
                                                    FILE_ATTACHED, R"(
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
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");
  data.paths.emplace_back(test_zip);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(1u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(1u, result.paths_results.size());
                   EXPECT_FALSE(result.paths_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  // "FILE_ATTACHED" is exempt from scanning.
  EXPECT_EQ(0,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, FileIsEncrypted_PolicyAllows) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  enterprise_connectors::test::SetAnalysisConnector(profile_->GetPrefs(),
                                                    FILE_ATTACHED, R"(
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
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");
  data.paths.emplace_back(test_zip);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(1u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(1u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  // When resumable upload is in use and the policy does not block encrypted
  // files by default, the file's metadata is uploaded for scanning.
  EXPECT_EQ(1,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, FileDataNegativeMalwareVerdict) {
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")}, &data);
  PathFailsDeepScan(
      data.paths[1],
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_FALSE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(2,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, FileDataPositiveDlpVerdict) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(2,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, FileDataNegativeDlpVerdict) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")}, &data);

  PathFailsDeepScan(data.paths[1],
                    test::FakeContentAnalysisDelegate::DlpResponse(
                        ContentAnalysisResponse::Result::SUCCESS, "rule",
                        TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_FALSE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(2,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest,
       FileDataNegativeMalwareAndDlpVerdicts) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/true);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")}, &data);

  PathFailsDeepScan(
      data.paths[1],
      test::FakeContentAnalysisDelegate::MalwareAndDlpResponse(
          TriggeredRule::BLOCK, ContentAnalysisResponse::Result::SUCCESS,
          "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_FALSE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(2,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringFileData) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(large_text());
  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   ASSERT_EQ(1u, result.text_results.size());
                   ASSERT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(3,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringFileDataNoDLP) {
  // Enable malware scan so deep scanning still occurs.
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(large_text());
  data.text.emplace_back(large_text());
  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(2u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   ASSERT_EQ(2u, result.text_results.size());
                   ASSERT_EQ(2u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   EXPECT_TRUE(result.text_results[1]);
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(3,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, ImageData) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.image = large_text();

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_TRUE(result.image_result);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(1,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, TextAndImageData) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));
  data.text.emplace_back(large_text());
  data.image = large_text();

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(1u, result.text_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   EXPECT_TRUE(result.image_result);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(2,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringFileDataFailedDLP) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(large_text());
  data.text.emplace_back(large_text());

  SetDLPResponse(test::FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(2u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(2u, result.text_results.size());
                   ASSERT_EQ(0u, result.paths_results.size());
                   EXPECT_FALSE(result.text_results[0]);
                   EXPECT_FALSE(result.text_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(1,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringFileDataPartialSuccess) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(large_text());
  CreateFilesForTest({FILE_PATH_LITERAL("foo.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_1.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_2.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_status.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_rule.doc")},
                     &data);

  // Mark some files with failed scans.
  PathFailsDeepScan(
      data.paths[1],
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::WARN));
  PathFailsDeepScan(
      data.paths[2],
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::BLOCK));
  PathFailsDeepScan(data.paths[3],
                    test::FakeContentAnalysisDelegate::DlpResponse(
                        ContentAnalysisResponse::Result::FAILURE, "",
                        TriggeredRule::REPORT_ONLY));
  PathFailsDeepScan(data.paths[4],
                    test::FakeContentAnalysisDelegate::DlpResponse(
                        ContentAnalysisResponse::Result::SUCCESS, "rule",
                        TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(5u, data.paths.size());
                   ASSERT_EQ(1u, result.text_results.size());
                   ASSERT_EQ(5u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_FALSE(result.paths_results[1]);
                   EXPECT_FALSE(result.paths_results[2]);
                   EXPECT_TRUE(result.paths_results[3]);
                   EXPECT_FALSE(result.paths_results[4]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(6,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, NoDelay) {
  enterprise_connectors::test::SetAnalysisConnector(profile_->GetPrefs(),
                                                    FILE_ATTACHED, R"(
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
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  data.text.emplace_back("dlp_text");
  CreateFilesForTest({FILE_PATH_LITERAL("foo_fail_malware_0.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_1.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_2.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_status.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_rule.doc")},
                     &data);

  // Mark all files and text with failed scans.
  SetDLPResponse(test::FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));
  PathFailsDeepScan(
      data.paths[0],
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::BLOCK));
  PathFailsDeepScan(
      data.paths[1],
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::WARN));
  PathFailsDeepScan(
      data.paths[2],
      test::FakeContentAnalysisDelegate::MalwareResponse(TriggeredRule::BLOCK));
  PathFailsDeepScan(data.paths[3],
                    test::FakeContentAnalysisDelegate::DlpResponse(
                        ContentAnalysisResponse::Result::FAILURE, "",
                        TriggeredRule::REPORT_ONLY));
  PathFailsDeepScan(data.paths[4],
                    test::FakeContentAnalysisDelegate::DlpResponse(
                        ContentAnalysisResponse::Result::SUCCESS, "rule",
                        TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(5u, data.paths.size());
                   EXPECT_EQ(1u, result.text_results.size());
                   EXPECT_EQ(5u, result.paths_results.size());

                   // All results are set to true since we are not blocking the
                   // user.
                   EXPECT_TRUE(result.text_results[0]);
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_TRUE(result.paths_results[1]);
                   EXPECT_TRUE(result.paths_results[2]);
                   EXPECT_TRUE(result.paths_results[3]);
                   EXPECT_TRUE(result.paths_results[4]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  // Text too small, only file analysis requests are created.
  EXPECT_EQ(5,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, EmptyWait) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(0u, result.text_results.size());
                   ASSERT_EQ(0u, result.paths_results.size());
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_EQ(0,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

// test params:
// 0: upload result from binary upload service.
// 1: whether an cloud analysis is done.
// 2: whether to fail open.
class ContentAnalysisDelegateResultHandlingTest
    : public BaseTest,
      public testing::WithParamInterface<
          std::tuple<safe_browsing::BinaryUploadService::Result, bool, bool>> {
 public:
  ContentAnalysisDelegateResultHandlingTest() = default;

  void SetUp() override {
    BaseTest::SetUp();
    std::string pref = base::StringPrintf(R"(
    {
      "service_provider": "%s",
      "enable": [{"url_list": ["*"], "tags": ["dlp", "malware"]}],
      "block_until_verdict": 1,
      "default_action": "%s"
    })",
                                          service_provider_setting(),
                                          default_action_setting());
    enterprise_connectors::test::SetAnalysisConnector(profile_->GetPrefs(),
                                                      FILE_ATTACHED, pref);

    ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
        &test::FakeContentAnalysisDelegate::Create, run_loop_.QuitClosure(),
        base::BindRepeating(
            &ContentAnalysisDelegateResultHandlingTest::ConnectorStatusCallback,
            base::Unretained(this)),
        kDmToken));
    test::FakeContentAnalysisDelegate::
        ResetStaticDialogFlagsAndTotalRequestsCount();
  }

  safe_browsing::BinaryUploadService::Result result() const {
    return std::get<0>(GetParam());
  }

  bool is_cloud() const { return std::get<1>(GetParam()); }

  const char* service_provider_setting() const {
    return is_cloud() ? "google" : "local_system_agent";
  }

  bool should_fail_closed() const { return std::get<2>(GetParam()); }

  const char* default_action_setting() const {
    return should_fail_closed() ? "block" : "allow";
  }

  ContentAnalysisResponse ConnectorStatusCallback(const std::string& contents,
                                                  const base::FilePath& path) {
    return test::FakeContentAnalysisDelegate::SuccessfulResponse(
        {"dlp", "malware"});
  }

 protected:
  ScopedSetDMToken scoped_dm_token_{
      policy::DMToken::CreateValidToken(kDmToken)};

  bool ResultIsFailClosed(safe_browsing::BinaryUploadService::Result result) {
    return result ==
               safe_browsing::BinaryUploadService::Result::UPLOAD_FAILURE ||
           result == safe_browsing::BinaryUploadService::Result::TIMEOUT ||
           result == safe_browsing::BinaryUploadService::Result::
                         FAILED_TO_GET_TOKEN ||
           result ==
               safe_browsing::BinaryUploadService::Result::TOO_MANY_REQUESTS ||
           result == safe_browsing::BinaryUploadService::Result::UNKNOWN ||
           result ==
               safe_browsing::BinaryUploadService::Result::INCOMPLETE_RESPONSE;
  }

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // This installs a fake SDK manager that creates fake SDK clients when
  // its GetClient() method is called. This is needed so that calls to
  // ContentAnalysisSdkManager::Get()->GetClient() do not fail.
  FakeContentAnalysisSdkManager sdk_manager_;
#endif
};

TEST_P(ContentAnalysisDelegateResultHandlingTest, Test) {
  // This is not a desktop platform don't try the non-cloud case since it
  // is not supported.
#if !BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  if (!is_cloud())
    return;
#endif

  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  test::FakeContentAnalysisDelegate::SetResponseResult(result());
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest({FILE_PATH_LITERAL("foo.txt")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindLambdaForTesting(
                 [this, &called](const ContentAnalysisDelegate::Data& data,
                                 ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(1u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(1u, result.paths_results.size());

                   bool expected =
                       ResultShouldAllowDataUse(data.settings, this->result());
                   EXPECT_EQ(expected, result.paths_results[0]);
                   called = true;
                 }));
  RunUntilDone();
  EXPECT_TRUE(called);

  // Dialog should be shown for fail-close cases, regardless of local or cloud,
  // otherwise dialog should be hidden for local analysis.
  if (ResultIsFailClosed(result()) && should_fail_closed()) {
    EXPECT_TRUE(test::FakeContentAnalysisDelegate::WasDialogShown());
    EXPECT_FALSE(test::FakeContentAnalysisDelegate::WasDialogCanceled());
  } else {
    EXPECT_EQ(is_cloud(), test::FakeContentAnalysisDelegate::WasDialogShown());
    EXPECT_NE(is_cloud(),
              test::FakeContentAnalysisDelegate::WasDialogCanceled());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ContentAnalysisDelegateResultHandlingTest,
    testing::Combine(
        testing::Values(
            safe_browsing::BinaryUploadService::Result::UNKNOWN,
            safe_browsing::BinaryUploadService::Result::SUCCESS,
            safe_browsing::BinaryUploadService::Result::UPLOAD_FAILURE,
            safe_browsing::BinaryUploadService::Result::TIMEOUT,
            safe_browsing::BinaryUploadService::Result::FILE_TOO_LARGE,
            safe_browsing::BinaryUploadService::Result::FAILED_TO_GET_TOKEN,
            safe_browsing::BinaryUploadService::Result::UNAUTHORIZED,
            safe_browsing::BinaryUploadService::Result::FILE_ENCRYPTED),
        testing::Bool(),
        testing::Bool()));

// The following tests should only be executed on the OS that support LCAC.
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
class ContentAnalysisDelegateWithLocalClient : public BaseTest {
 public:
  ContentAnalysisDelegateWithLocalClient() = default;

 protected:
  FakeContentAnalysisSdkManager sdk_manager_;

  void SetLocalPolicies(bool should_fail_open) {
    std::string pref = base::StringPrintf(R"(
    {
      "service_provider": "local_system_agent",
      "enable": [{"url_list": ["*"], "tags": ["dlp", "malware"]}],
      "block_until_verdict": 1,
      "default_action": "%s"
    })",
                                          should_fail_open ? "allow" : "block");
    enterprise_connectors::test::SetAnalysisConnector(profile_->GetPrefs(),
                                                      BULK_DATA_ENTRY, pref);
  }

  void SetUp() override {
    BaseTest::SetUp();

    ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
        &test::FakeContentAnalysisDelegate::Create, run_loop_.QuitClosure(),
        base::BindRepeating(
            &ContentAnalysisDelegateWithLocalClient::ConnectorStatusCallback,
            base::Unretained(this)),
        kDmToken));
    test::FakeContentAnalysisDelegate::
        ResetStaticDialogFlagsAndTotalRequestsCount();
  }

  ContentAnalysisResponse ConnectorStatusCallback(const std::string& contents,
                                                  const base::FilePath& path) {
    return test::FakeContentAnalysisDelegate::SuccessfulResponse(
        {"dlp", "malware"});
  }

 private:
  ScopedSetDMToken scoped_dm_token_{
      policy::DMToken::CreateValidToken(kDmToken)};
};

TEST_F(ContentAnalysisDelegateWithLocalClient, StringDataWithValidClient) {
  SetLocalPolicies(/*should_fail_open=*/true);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(large_text());
  sdk_manager_.SetCreateClientAbility(true);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(1u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();

  EXPECT_FALSE(sdk_manager_.NoConnectionEstablished());
  EXPECT_EQ(1,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateWithLocalClient, FailOpen) {
  SetLocalPolicies(/*should_fail_open=*/true);
  sdk_manager_.SetCreateClientAbility(false);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(large_text());

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(1u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();

  EXPECT_TRUE(sdk_manager_.NoConnectionEstablished());
  // No local client found, should skip data analysis.
  EXPECT_EQ(0,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateWithLocalClient, FailClosed) {
  SetLocalPolicies(/*should_fail_open=*/false);
  sdk_manager_.SetCreateClientAbility(false);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(large_text());

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(1u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());

                   bool expected_result = true;
    // Should only fail closed on Windows.
#if BUILDFLAG(IS_WIN)
                   expected_result = false;
#endif
                   EXPECT_EQ(expected_result, result.text_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();

  EXPECT_TRUE(sdk_manager_.NoConnectionEstablished());
  // No local client found, should skip data analysis.
  EXPECT_EQ(0,
            test::FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount());
  EXPECT_TRUE(called);
}
#endif

// Calling GetRequestData() twice should return the same valid region.
TEST(StringAnalysisRequest, GetRequestData) {
  std::string contents("contents");
  StringAnalysisRequest request(AnalysisSettings().cloud_or_local_settings,
                                contents, base::DoNothing());

  safe_browsing::BinaryUploadService::Request::Data data1;
  request.GetRequestData(base::BindLambdaForTesting(
      [&data1](safe_browsing::BinaryUploadService::Result result,
               safe_browsing::BinaryUploadService::Request::Data data) {
        data1 = std::move(data);
      }));

  safe_browsing::BinaryUploadService::Request::Data data2;
  request.GetRequestData(base::BindLambdaForTesting(
      [&data2](safe_browsing::BinaryUploadService::Result result,
               safe_browsing::BinaryUploadService::Request::Data data) {
        data2 = std::move(data);
      }));

  ASSERT_EQ(data1.size, data2.size);
  ASSERT_EQ(data1.size, contents.size());
  ASSERT_EQ(data1.contents, data2.contents);
}

}  // namespace enterprise_connectors
