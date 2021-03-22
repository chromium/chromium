// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_delegate.h"
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
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    ContentAnalysisDelegate::DisableUIForTesting();
  }

  void EnableFeatures() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
  }

  void DisableFeatures() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures({}, {kEnterpriseConnectorsEnabled});
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
      ContentAnalysisDelegate::Data* data) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    for (const auto& file_name : file_names) {
      base::FilePath path = temp_dir_.GetPath().Append(file_name);
      base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      file.WriteAtCurrentPos("content", 7);
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
  TestingProfile* profile_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::RunLoop run_loop_;
};

}  // namespace

using ContentAnalysisDelegateIsEnabledTest = BaseTest;

TEST_F(ContentAnalysisDelegateIsEnabledTest, NoFeatureNoDMTokenNoPref) {
  DisableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, NoDMTokenNoPref) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, NoDMToken) {
  EnableFeatures();
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      kBlockingScansForDlpAndMalware);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, NoFeatureNoPref) {
  DisableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, NoFeatureNoDMToken) {
  DisableFeatures();
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      kBlockingScansForDlpAndMalware);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, NoFeature) {
  DisableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      kBlockingScansForDlpAndMalware);

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpNoPref) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpNoPref2) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      kNothingEnabled);

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpNoPref3) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_DOWNLOADED,
                                      kBlockingScansForDlpAndMalware);

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpEnabled) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      kBlockingScansForDlp);

  ContentAnalysisDelegate::Data data;
  EXPECT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                 FILE_ATTACHED));
  EXPECT_TRUE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpEnabled2) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      kBlockingScansForDlp);
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_DOWNLOADED,
                                      kBlockingScansForDlp);

  ContentAnalysisDelegate::Data data;
  EXPECT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                 FILE_ATTACHED));
  EXPECT_TRUE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpEnabledWithUrl) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      kBlockingScansForDlp);
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_DOWNLOADED,
                                      kBlockingScansForDlp);
  GURL url(kTestUrl);

  ContentAnalysisDelegate::Data data;
  EXPECT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));
  EXPECT_TRUE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
  EXPECT_EQ(kTestUrl, data.url);
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, DlpDisabledByList) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
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
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
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
                "chrome://*",
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
  ValidateIsEnabled("chrome://version/", /*dlp*/ false, /*malware*/ false);
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
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, MalwareNoPref2) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      kNothingEnabled);

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, MalwareNoPref3) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_DOWNLOADED,
                                      kBlockingScansForDlpAndMalware);

  ContentAnalysisDelegate::Data data;
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(profile(), GURL(), &data,
                                                  FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, MalwareEnabled) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
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
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      kBlockingScansForDlpAndMalware);

  ContentAnalysisDelegate::Data data;
  EXPECT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  // The same URL should not trigger a scan in incognito.
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(
      profile()->GetPrimaryOTRProfile(), url, &data, FILE_ATTACHED));

  // The same URL should not trigger a scan in non-primary OTR profiles
  EXPECT_FALSE(ContentAnalysisDelegate::IsEnabled(
      profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID("Test::DeepScanning")),
      url, &data, FILE_ATTACHED));
}

TEST_F(ContentAnalysisDelegateIsEnabledTest, MalwareEnabledWithPatterns) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      R"(
        {
          "service_provider": "google",
          "enable": [
            {
              "url_list": [
                "http://example.com/",
                "https://*",
                "chrome://*",
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
  ValidateIsEnabled("chrome://version/", /*dlp*/ false, /*malware*/ true);
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

  void SetPathIsEncrypted(base::FilePath path) {
    encrypted_.insert(std::move(path));
  }

  void SetScanPolicies(bool dlp, bool malware) {
    include_dlp_ = dlp;
    include_malware_ = malware;

    for (auto connector : {FILE_ATTACHED, BULK_DATA_ENTRY}) {
      if (include_dlp_ && include_malware_) {
        safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), connector,
                                            kBlockingScansForDlpAndMalware);
      } else if (include_dlp_) {
        safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), connector,
                                            kBlockingScansForDlp);
      } else if (include_malware_) {
        safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), connector,
                                            kBlockingScansForMalware);
      } else {
        safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), connector,
                                            kNothingEnabled);
      }
    }
  }

  void SetUp() override {
    BaseTest::SetUp();

    EnableFeatures();
    safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                        kBlockingScansForDlpAndMalware);
    safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), BULK_DATA_ENTRY,
                                        kBlockingScansForDlpAndMalware);

    ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
        &FakeContentAnalysisDelegate::Create, run_loop_.QuitClosure(),
        base::BindRepeating(
            &ContentAnalysisDelegateAuditOnlyTest::ConnectorStatusCallback,
            base::Unretained(this)),
        base::BindRepeating(
            &ContentAnalysisDelegateAuditOnlyTest::EncryptionStatusCallback,
            base::Unretained(this)),
        kDmToken));
  }

  ContentAnalysisResponse ConnectorStatusCallback(const base::FilePath& path) {
    // The path succeeds if it is not in the |failures_| maps.
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

  bool EncryptionStatusCallback(const base::FilePath& path) {
    return encrypted_.count(path) > 0;
  }

 private:
  ScopedSetDMToken scoped_dm_token_{
      policy::DMToken::CreateValidTokenForTesting(kDmToken)};
  bool include_dlp_ = true;
  bool include_malware_ = true;

  // Paths in this map will be consider to have failed deep scan checks.
  // The actual failure response is given for each path.
  std::map<base::FilePath, ContentAnalysisResponse> failures_;

  // Paths in this set will be considered to contain encryption and will
  // not be uploaded.
  std::set<base::FilePath> encrypted_;

  // DLP response to ovewrite in the callback if present.
  base::Optional<ContentAnalysisResponse> dlp_response_ = base::nullopt;
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
                    const ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringData) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(1u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(1u, result.text_results.size());
                   EXPECT_EQ(0u, result.paths_results.size());
                   EXPECT_TRUE(result.text_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringData2) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  data.text.emplace_back(base::UTF8ToUTF16(large_text()));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
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
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringData3) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  // Because the strings are small, they are exempt from scanning and will be
  // allowed even when a negative verdict is mocked.
  data.text.emplace_back(base::UTF8ToUTF16(small_text()));
  data.text.emplace_back(base::UTF8ToUTF16(small_text()));

  SetDLPResponse(FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
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
                    const ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(1u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   ASSERT_EQ(1u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
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
                    const ContentAnalysisDelegate::Result& result) {
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
                    const ContentAnalysisDelegate::Result& result) {
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
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, FileIsEncrypted) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED, R"(
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
                    const ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(1u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(1u, result.paths_results.size());
                   EXPECT_FALSE(result.paths_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

// Flaky on Mac: https://crbug.com/1143782:
#if defined(OS_MAC)
#define MAYBE_FileIsEncrypted_PolicyAllows DISABLED_FileIsEncrypted_PolicyAllows
#else
#define MAYBE_FileIsEncrypted_PolicyAllows FileIsEncrypted_PolicyAllows
#endif
TEST_F(ContentAnalysisDelegateAuditOnlyTest,
       MAYBE_FileIsEncrypted_PolicyAllows) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED, R"(
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
                    const ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(1u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(1u, result.paths_results.size());
                   EXPECT_TRUE(result.paths_results[0]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
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
  PathFailsDeepScan(data.paths[1], FakeContentAnalysisDelegate::MalwareResponse(
                                       TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
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
                    const ContentAnalysisDelegate::Result& result) {
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

  PathFailsDeepScan(data.paths[1], FakeContentAnalysisDelegate::DlpResponse(
                                       ContentAnalysisResponse::Result::SUCCESS,
                                       "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
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
      FakeContentAnalysisDelegate::MalwareAndDlpResponse(
          TriggeredRule::BLOCK, ContentAnalysisResponse::Result::SUCCESS,
          "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
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
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringFileData) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
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
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringFileDataNoDLP) {
  // Enable malware scan so deep scanning still occurs.
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
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
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringFileDataFailedDLP) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  data.text.emplace_back(base::UTF8ToUTF16(large_text()));

  SetDLPResponse(FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
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
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, StringFileDataPartialSuccess) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(ContentAnalysisDelegate::IsEnabled(profile(), url, &data,
                                                 BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  CreateFilesForTest({FILE_PATH_LITERAL("foo.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_1.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_2.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_status.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_rule.doc")},
                     &data);

  // Mark some files with failed scans.
  PathFailsDeepScan(data.paths[1], FakeContentAnalysisDelegate::MalwareResponse(
                                       TriggeredRule::WARN));
  PathFailsDeepScan(data.paths[2], FakeContentAnalysisDelegate::MalwareResponse(
                                       TriggeredRule::BLOCK));
  PathFailsDeepScan(data.paths[3], FakeContentAnalysisDelegate::DlpResponse(
                                       ContentAnalysisResponse::Result::FAILURE,
                                       "", TriggeredRule::REPORT_ONLY));
  PathFailsDeepScan(data.paths[4], FakeContentAnalysisDelegate::DlpResponse(
                                       ContentAnalysisResponse::Result::SUCCESS,
                                       "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
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
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, NoDelay) {
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED, R"(
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

  data.text.emplace_back(u"dlp_text");
  CreateFilesForTest({FILE_PATH_LITERAL("foo_fail_malware_0.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_1.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_2.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_status.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_rule.doc")},
                     &data);

  // Mark all files and text with failed scans.
  SetDLPResponse(FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));
  PathFailsDeepScan(data.paths[0], FakeContentAnalysisDelegate::MalwareResponse(
                                       TriggeredRule::BLOCK));
  PathFailsDeepScan(data.paths[1], FakeContentAnalysisDelegate::MalwareResponse(
                                       TriggeredRule::WARN));
  PathFailsDeepScan(data.paths[2], FakeContentAnalysisDelegate::MalwareResponse(
                                       TriggeredRule::BLOCK));
  PathFailsDeepScan(data.paths[3], FakeContentAnalysisDelegate::DlpResponse(
                                       ContentAnalysisResponse::Result::FAILURE,
                                       "", TriggeredRule::REPORT_ONLY));
  PathFailsDeepScan(data.paths[4], FakeContentAnalysisDelegate::DlpResponse(
                                       ContentAnalysisResponse::Result::SUCCESS,
                                       "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
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
                    const ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(0u, data.paths.size());
                   ASSERT_EQ(0u, result.text_results.size());
                   ASSERT_EQ(0u, result.paths_results.size());
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, SupportedTypes) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  std::vector<base::FilePath::StringType> file_names;
  for (const base::FilePath::StringType& supported_type :
       safe_browsing::SupportedDlpFileTypes()) {
    file_names.push_back(base::FilePath::StringType(FILE_PATH_LITERAL("foo")) +
                         supported_type);
  }
  CreateFilesForTest(file_names, &data);

  // Mark all files with failed scans.
  for (const auto& path : data.paths) {
    PathFailsDeepScan(path, FakeContentAnalysisDelegate::MalwareResponse(
                                TriggeredRule::BLOCK));
  }

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(24u, data.paths.size());
                   EXPECT_EQ(24u, result.paths_results.size());

                   // The supported types should be marked as false.
                   for (const auto& result : result.paths_results)
                     EXPECT_FALSE(result);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, UnsupportedTypesDefaultPolicy) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.these"), FILE_PATH_LITERAL("foo.file"),
       FILE_PATH_LITERAL("foo.types"), FILE_PATH_LITERAL("foo.are"),
       FILE_PATH_LITERAL("foo.not"), FILE_PATH_LITERAL("foo.supported")},
      &data);

  // Mark all files with failed scans.
  for (const auto& path : data.paths) {
    PathFailsDeepScan(path, FakeContentAnalysisDelegate::DlpResponse(
                                ContentAnalysisResponse::Result::SUCCESS,
                                "rule", TriggeredRule::WARN));
  }

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(6u, data.paths.size());
                   ASSERT_EQ(6u, result.paths_results.size());

                   // The unsupported types should be marked as true since the
                   // default policy behavior is to allow them through.
                   for (const bool path_result : result.paths_results)
                     EXPECT_TRUE(path_result);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, UnsupportedTypesBlockPolicy) {
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED, R"(
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
  ContentAnalysisDelegate::Data data;
  EXPECT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.these"), FILE_PATH_LITERAL("foo.file"),
       FILE_PATH_LITERAL("foo.types"), FILE_PATH_LITERAL("foo.are"),
       FILE_PATH_LITERAL("foo.not"), FILE_PATH_LITERAL("foo.supported")},
      &data);

  // Mark all files with failed scans.
  for (const auto& path : data.paths) {
    PathFailsDeepScan(path, FakeContentAnalysisDelegate::DlpResponse(
                                ContentAnalysisResponse::Result::SUCCESS,
                                "rule", TriggeredRule::WARN));
  }

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(6u, data.paths.size());
                   ASSERT_EQ(6u, result.paths_results.size());

                   // The unsupported types should be marked as false since the
                   // block policy behavior is to not allow them through.
                   for (const bool path_result : result.paths_results)
                     EXPECT_FALSE(path_result);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, SupportedAndUnsupportedTypes) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  // Only 3 of these file types are supported (bzip, cab and doc). They are
  // mixed in the list so as to show that insertion order does not matter.
  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.bzip"), FILE_PATH_LITERAL("foo.these"),
       FILE_PATH_LITERAL("foo.file"), FILE_PATH_LITERAL("foo.types"),
       FILE_PATH_LITERAL("foo.cab"), FILE_PATH_LITERAL("foo.are"),
       FILE_PATH_LITERAL("foo.not"), FILE_PATH_LITERAL("foo.supported"),
       FILE_PATH_LITERAL("foo_no_extension"), FILE_PATH_LITERAL("foo.doc")},
      &data);

  // Mark all files with failed scans.
  for (const auto& path : data.paths) {
    PathFailsDeepScan(path, FakeContentAnalysisDelegate::DlpResponse(
                                ContentAnalysisResponse::Result::SUCCESS,
                                "rule", TriggeredRule::BLOCK));
  }

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(10u, data.paths.size());
                   ASSERT_EQ(10u, result.paths_results.size());

                   // The unsupported types should be marked as true, and the
                   // valid types as false since they are marked as failed
                   // scans.
                   size_t i = 0;
                   for (const bool expected : {false, true, true, true, false,
                                               true, true, true, true, false}) {
                     ASSERT_EQ(expected, result.paths_results[i]);
                     ++i;
                   }
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(ContentAnalysisDelegateAuditOnlyTest, UnsupportedTypeAndDLPFailure) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest({FILE_PATH_LITERAL("foo.unsupported_extension"),
                      FILE_PATH_LITERAL("dlp_fail.doc")},
                     &data);

  // Mark DLP as failure.
  SetDLPResponse(FakeContentAnalysisDelegate::DlpResponse(
      ContentAnalysisResponse::Result::SUCCESS, "rule", TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const ContentAnalysisDelegate::Data& data,
                    const ContentAnalysisDelegate::Result& result) {
                   EXPECT_EQ(0u, data.text.size());
                   EXPECT_EQ(2u, data.paths.size());
                   EXPECT_EQ(0u, result.text_results.size());
                   EXPECT_EQ(2u, result.paths_results.size());

                   // The unsupported type file should be marked as true, and
                   // the valid type file as false.
                   EXPECT_TRUE(result.paths_results[0]);
                   EXPECT_FALSE(result.paths_results[1]);
                   *called = true;
                 },
                 &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

class ContentAnalysisDelegateResultHandlingTest
    : public BaseTest,
      public testing::WithParamInterface<
          std::tuple<safe_browsing::BinaryUploadService::Result, bool>> {
 public:
  ContentAnalysisDelegateResultHandlingTest() = default;

  void SetUp() override {
    BaseTest::SetUp();
    EnableFeatures();
    safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                        kBlockingScansForDlpAndMalware);

    ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
        &FakeContentAnalysisDelegate::Create, run_loop_.QuitClosure(),
        base::BindRepeating(
            &ContentAnalysisDelegateResultHandlingTest::ConnectorStatusCallback,
            base::Unretained(this)),
        /*encryption_callback=*/
        base::BindRepeating([](const base::FilePath& path) { return false; }),
        kDmToken));
  }

  safe_browsing::BinaryUploadService::Result result() const {
    return std::get<0>(GetParam());
  }

  ContentAnalysisResponse ConnectorStatusCallback(const base::FilePath& path) {
    return FakeContentAnalysisDelegate::SuccessfulResponse({"dlp", "malware"});
  }

 protected:
  ScopedSetDMToken scoped_dm_token_{
      policy::DMToken::CreateValidTokenForTesting(kDmToken)};
};

TEST_P(ContentAnalysisDelegateResultHandlingTest, Test) {
  GURL url(kTestUrl);
  ContentAnalysisDelegate::Data data;
  FakeContentAnalysisDelegate::SetResponseResult(result());
  ASSERT_TRUE(
      ContentAnalysisDelegate::IsEnabled(profile(), url, &data, FILE_ATTACHED));

  CreateFilesForTest({FILE_PATH_LITERAL("foo.txt")}, &data);

  bool called = false;
  ScanUpload(
      contents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const ContentAnalysisDelegate::Data& data,
                          const ContentAnalysisDelegate::Result& result) {
            EXPECT_EQ(0u, data.text.size());
            EXPECT_EQ(1u, data.paths.size());
            EXPECT_EQ(0u, result.text_results.size());
            EXPECT_EQ(1u, result.paths_results.size());

            bool expected = ContentAnalysisDelegate::ResultShouldAllowDataUse(
                this->result(), data.settings);
            EXPECT_EQ(expected, result.paths_results[0]);
            called = true;
          }));
  RunUntilDone();
  EXPECT_TRUE(called);
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
        testing::Bool()));

class ContentAnalysisDelegateSettingsTest
    : public BaseTest,
      public testing::WithParamInterface<bool> {
 public:
  ContentAnalysisDelegateSettingsTest() = default;

  void SetUp() override {
    BaseTest::SetUp();
    EnableFeatures();

    // Settings can't be returned if no DM token exists.
    SetDMTokenForTesting(policy::DMToken::CreateValidTokenForTesting(kDmToken));
  }

  bool allowed() const { return !GetParam(); }
  const char* bool_setting() const { return GetParam() ? "true" : "false"; }

  AnalysisSettings settings() {
    base::Optional<AnalysisSettings> settings =
        ConnectorsServiceFactory::GetForBrowserContext(profile())
            ->GetAnalysisSettings(GURL(kTestUrl), FILE_ATTACHED);
    EXPECT_TRUE(settings.has_value());
    return std::move(settings.value());
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         ContentAnalysisDelegateSettingsTest,
                         testing::Bool());

TEST_P(ContentAnalysisDelegateSettingsTest, BlockLargeFile) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "block_large_files": %s
    })",
                                 bool_setting());
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      pref);
  EXPECT_EQ(allowed(),
            ContentAnalysisDelegate::ResultShouldAllowDataUse(
                safe_browsing::BinaryUploadService::Result::FILE_TOO_LARGE,
                settings()));
}

TEST_P(ContentAnalysisDelegateSettingsTest, BlockPasswordProtected) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "block_password_protected": %s
    })",
                                 bool_setting());
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      pref);
  EXPECT_EQ(allowed(),
            ContentAnalysisDelegate::ResultShouldAllowDataUse(
                safe_browsing::BinaryUploadService::Result::FILE_ENCRYPTED,
                settings()));
}

TEST_P(ContentAnalysisDelegateSettingsTest, BlockUnsupportedFileTypes) {
  auto pref = base::StringPrintf(R"(
    {
      "service_provider": "google",
      "enable": [{"url_list": ["*"], "tags": ["dlp"]}],
      "block_unsupported_file_types": %s
    })",
                                 bool_setting());
  safe_browsing::SetAnalysisConnector(profile_->GetPrefs(), FILE_ATTACHED,
                                      pref);
  EXPECT_EQ(allowed(), ContentAnalysisDelegate::ResultShouldAllowDataUse(
                           safe_browsing::BinaryUploadService::Result::
                               DLP_SCAN_UNSUPPORTED_FILE_TYPE,
                           settings()));
}

}  // namespace enterprise_connectors
