// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr char kDmToken[] = "dm_token";
constexpr char kTestUrl[] = "http://example.com/";

constexpr char kTestHttpsSchemePatternUrl[] = "https://*";
constexpr char kTestChromeSchemePatternUrl[] = "chrome://*";
constexpr char kTestDevtoolsSchemePatternUrl[] = "devtools://*";

constexpr char kTestPathPatternUrl[] = "*/a/specific/path/";
constexpr char kTestPortPatternUrl[] = "*:1234";
constexpr char kTestQueryPatternUrl[] = "*?q=5678";

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
    DeepScanningDialogDelegate::DisableUIForTesting();
  }

  void EnableFeatures() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {enterprise_connectors::kEnterpriseConnectorsEnabled}, {});
  }

  void DisableFeatures() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        {}, {enterprise_connectors::kEnterpriseConnectorsEnabled});
  }

  void SetDlpPolicy(CheckContentComplianceValues state) {
    SetDlpPolicyForConnectors(state);
  }

  void SetWaitPolicy(DelayDeliveryUntilVerdictValues state) {
    SetDelayDeliveryUntilVerdictPolicyForConnectors(state);
  }

  void SetAllowPasswordPolicy(AllowPasswordProtectedFilesValues state) {
    SetAllowPasswordProtectedFilesPolicyForConnectors(state);
  }

  void SetMalwarePolicy(SendFilesForMalwareCheckValues state) {
    SetMalwarePolicyForConnectors(state);
  }

  void SetBlockLargeFilePolicy(BlockLargeFileTransferValues state) {
    SetBlockLargeFileTransferPolicyForConnectors(state);
  }

  void SetUnsupportedFileTypePolicy(BlockUnsupportedFiletypesValues state) {
    SetBlockUnsupportedFileTypesPolicyForConnectors(state);
  }

  void AddUrlToList(const char* pref_name, const std::string& url) {
    AddUrlToListForConnectors(pref_name, url);
  }

  void AddUrlToList(const char* pref_name, const GURL& url) {
    AddUrlToList(pref_name, url.host());
  }

  void ScanUpload(content::WebContents* web_contents,
                  DeepScanningDialogDelegate::Data data,
                  DeepScanningDialogDelegate::CompletionCallback callback) {
    // The access point is only used for metrics and choosing the dialog text if
    // one is shown, so its value doesn't affect the tests in this file and can
    // always be the same.
    DeepScanningDialogDelegate::ShowForWebContents(
        web_contents, std::move(data), std::move(callback),
        DeepScanAccessPoint::UPLOAD);
  }

  void CreateFilesForTest(
      const std::vector<base::FilePath::StringType>& file_names,
      DeepScanningDialogDelegate::Data* data) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    for (const auto& file_name : file_names) {
      base::FilePath path = temp_dir_.GetPath().Append(file_name);
      base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      file.WriteAtCurrentPos("content", 7);
      data->paths.emplace_back(path);
    }
  }

  void SetUp() override {
    enterprise_connectors::ConnectorsManager::GetInstance()->SetUpForTesting();

    // Always set this so DeepScanningDialogDelegate::ShowForWebContents waits
    // for the verdict before running its callback.
    SetWaitPolicy(DELAY_UPLOADS);
  }

  void TearDown() override {
    enterprise_connectors::ConnectorsManager::GetInstance()
        ->TearDownForTesting();
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
    DeepScanningDialogDelegate::Data data;
    EXPECT_EQ(expect_dlp || expect_malware,
              DeepScanningDialogDelegate::IsEnabled(
                  profile(), GURL(url), &data,
                  enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
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

using DeepScanningDialogDelegateIsEnabledTest = BaseTest;

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeatureNoDMTokenNoPref) {
  DisableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoDMTokenNoPref) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoDMToken) {
  EnableFeatures();
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeatureNoPref) {
  DisableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeatureNoDMToken) {
  DisableFeatures();
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeature) {
  DisableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref2) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_NONE);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref3) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabled) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabled2) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabledWithUrl) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  GURL url(kTestUrl);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_TRUE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
  EXPECT_EQ(kTestUrl, data.url);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpDisabledByList) {
  GURL url(kTestUrl);
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent, url);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpDisabledByListWithPatterns) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent, kTestUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestHttpsSchemePatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestChromeSchemePatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestDevtoolsSchemePatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestHttpsSchemePatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestPathPatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestPortPatternUrl);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent,
               kTestQueryPatternUrl);

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

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref2) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(DO_NOT_SCAN);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref4) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoList) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoList2) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL(), &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_FALSE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareEnabled) {
  GURL url(kTestUrl);
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, url);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
  EXPECT_FALSE(data.settings.tags.count("dlp"));
  EXPECT_TRUE(data.settings.tags.count("malware"));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoScanInIncognito) {
  GURL url(kTestUrl);
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, url);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // The same URL should not trigger a scan in incognito.
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile()->GetPrimaryOTRProfile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // The same URL should not trigger a scan in non-primary OTR profiles
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID("Test::DeepScanning")),
      url, &data, enterprise_connectors::AnalysisConnector::FILE_ATTACHED));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareEnabledWithPatterns) {
  EnableFeatures();
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, kTestUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestHttpsSchemePatternUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestChromeSchemePatternUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestDevtoolsSchemePatternUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestPathPatternUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestPortPatternUrl);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent,
               kTestQueryPatternUrl);

  DeepScanningDialogDelegate::Data data;

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

class DeepScanningDialogDelegateAuditOnlyTest : public BaseTest {
 public:
  DeepScanningDialogDelegateAuditOnlyTest() = default;

 protected:
  void SetDLPResponse(enterprise_connectors::ContentAnalysisResponse response) {
    dlp_response_ = std::move(response);
  }

  void PathFailsDeepScan(
      base::FilePath path,
      enterprise_connectors::ContentAnalysisResponse response) {
    connector_failures_.insert({std::move(path), std::move(response)});
  }

  void SetPathIsEncrypted(base::FilePath path) {
    encrypted_.insert(std::move(path));
  }

  void SetScanPolicies(bool dlp, bool malware) {
    include_dlp_ = dlp;
    include_malware_ = malware;

    if (include_dlp_)
      SetDlpPolicy(CHECK_UPLOADS);
    else
      SetDlpPolicy(CHECK_NONE);

    if (include_malware_)
      SetMalwarePolicy(SEND_UPLOADS);
    else
      SetMalwarePolicy(DO_NOT_SCAN);
  }

  void SetUp() override {
    BaseTest::SetUp();

    EnableFeatures();
    SetDlpPolicy(CHECK_UPLOADS);
    SetMalwarePolicy(SEND_UPLOADS);

    DeepScanningDialogDelegate::SetFactoryForTesting(base::BindRepeating(
        &FakeDeepScanningDialogDelegate::Create, run_loop_.QuitClosure(),
        base::BindRepeating(
            &DeepScanningDialogDelegateAuditOnlyTest::ConnectorStatusCallback,
            base::Unretained(this)),
        base::BindRepeating(
            &DeepScanningDialogDelegateAuditOnlyTest::EncryptionStatusCallback,
            base::Unretained(this)),
        kDmToken));
  }

  enterprise_connectors::ContentAnalysisResponse ConnectorStatusCallback(
      const base::FilePath& path) {
    // The path succeeds if it is not in the |connector_failures_| maps.
    auto it = connector_failures_.find(path);
    enterprise_connectors::ContentAnalysisResponse response =
        it != connector_failures_.end()
            ? it->second
            : FakeDeepScanningDialogDelegate::SuccessfulResponse([this]() {
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
  std::map<base::FilePath, DeepScanningClientResponse> failures_;
  std::map<base::FilePath, enterprise_connectors::ContentAnalysisResponse>
      connector_failures_;

  // Paths in this set will be considered to contain encryption and will
  // not be uploaded.
  std::set<base::FilePath> encrypted_;

  // DLP response to ovewrite in the callback if present.
  base::Optional<enterprise_connectors::ContentAnalysisResponse> dlp_response_ =
      base::nullopt;
};

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, Empty) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  // Keep |data| empty by not setting any text or paths.

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringData) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringData2) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  data.text.emplace_back(base::UTF8ToUTF16(large_text()));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringData3) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  // Because the strings are small, they are exempt from scanning and will be
  // allowed even when a negative verdict is mocked.
  data.text.emplace_back(base::UTF8ToUTF16(small_text()));
  data.text.emplace_back(base::UTF8ToUTF16(small_text()));

  SetDLPResponse(FakeDeepScanningDialogDelegate::DlpResponse(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS, "rule",
      enterprise_connectors::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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
TEST_F(DeepScanningDialogDelegateAuditOnlyTest,
       FileDataPositiveMalwareAndDlpVerdicts) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest({FILE_PATH_LITERAL("foo.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest,
       FileDataPositiveMalwareAndDlpVerdicts2) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest,
       FileDataPositiveMalwareVerdict) {
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileIsEncrypted) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  SetScanPolicies(/*dlp=*/true, /*malware=*/true);
  SetAllowPasswordPolicy(ALLOW_NONE);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");
  data.paths.emplace_back(test_zip);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileIsEncrypted_PolicyAllows) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  SetScanPolicies(/*dlp=*/true, /*malware=*/true);
  SetAllowPasswordPolicy(ALLOW_UPLOADS);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  base::FilePath test_zip;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_zip));
  test_zip = test_zip.AppendASCII("safe_browsing")
                 .AppendASCII("download_protection")
                 .AppendASCII("encrypted.zip");
  data.paths.emplace_back(test_zip);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest,
       FileDataNegativeMalwareVerdict) {
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")}, &data);
  PathFailsDeepScan(data.paths[1],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        enterprise_connectors::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileDataPositiveDlpVerdict) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("good2.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileDataNegativeDlpVerdict) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")}, &data);

  PathFailsDeepScan(
      data.paths[1],
      FakeDeepScanningDialogDelegate::DlpResponse(
          enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS,
          "rule", enterprise_connectors::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest,
       FileDataNegativeMalwareAndDlpVerdicts) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/true);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("good.doc"), FILE_PATH_LITERAL("bad.doc")}, &data);

  PathFailsDeepScan(
      data.paths[1],
      FakeDeepScanningDialogDelegate::MalwareAndDlpResponse(
          enterprise_connectors::TriggeredRule::BLOCK,
          enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS,
          "rule", enterprise_connectors::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileData) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileDataNoDLP) {
  // Enable malware scan so deep scanning still occurs.
  SetScanPolicies(/*dlp=*/false, /*malware=*/true);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.doc"), FILE_PATH_LITERAL("bar.doc")}, &data);

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileDataFailedDLP) {
  SetScanPolicies(/*dlp=*/true, /*malware=*/false);
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  data.text.emplace_back(base::UTF8ToUTF16(large_text()));

  SetDLPResponse(FakeDeepScanningDialogDelegate::DlpResponse(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS, "rule",
      enterprise_connectors::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileDataPartialSuccess) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::BULK_DATA_ENTRY));

  data.text.emplace_back(base::UTF8ToUTF16(large_text()));
  CreateFilesForTest({FILE_PATH_LITERAL("foo.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_1.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_2.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_status.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_rule.doc")},
                     &data);

  // Mark some files with failed scans.
  PathFailsDeepScan(data.paths[1],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        enterprise_connectors::TriggeredRule::WARN));
  PathFailsDeepScan(data.paths[2],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        enterprise_connectors::TriggeredRule::BLOCK));
  PathFailsDeepScan(
      data.paths[3],
      FakeDeepScanningDialogDelegate::DlpResponse(
          enterprise_connectors::ContentAnalysisResponse::Result::FAILURE, "",
          enterprise_connectors::TriggeredRule::REPORT_ONLY));
  PathFailsDeepScan(
      data.paths[4],
      FakeDeepScanningDialogDelegate::DlpResponse(
          enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS,
          "rule", enterprise_connectors::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, NoDelay) {
  SetWaitPolicy(DELAY_NONE);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, "*");
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  data.text.emplace_back(base::UTF8ToUTF16("dlp_text"));
  CreateFilesForTest({FILE_PATH_LITERAL("foo_fail_malware_0.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_1.doc"),
                      FILE_PATH_LITERAL("foo_fail_malware_2.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_status.doc"),
                      FILE_PATH_LITERAL("foo_fail_dlp_rule.doc")},
                     &data);

  // Mark all files and text with failed scans.
  SetDLPResponse(FakeDeepScanningDialogDelegate::DlpResponse(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS, "rule",
      enterprise_connectors::TriggeredRule::BLOCK));
  PathFailsDeepScan(data.paths[0],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        enterprise_connectors::TriggeredRule::BLOCK));
  PathFailsDeepScan(data.paths[1],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        enterprise_connectors::TriggeredRule::WARN));
  PathFailsDeepScan(data.paths[2],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        enterprise_connectors::TriggeredRule::BLOCK));
  PathFailsDeepScan(
      data.paths[3],
      FakeDeepScanningDialogDelegate::DlpResponse(
          enterprise_connectors::ContentAnalysisResponse::Result::FAILURE, "",
          enterprise_connectors::TriggeredRule::REPORT_ONLY));
  PathFailsDeepScan(
      data.paths[4],
      FakeDeepScanningDialogDelegate::DlpResponse(
          enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS,
          "rule", enterprise_connectors::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, EmptyWait) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, SupportedTypes) {
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper;

  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  std::vector<base::FilePath::StringType> file_names;
  for (const base::FilePath::StringType& supported_type :
       SupportedDlpFileTypes()) {
    file_names.push_back(base::FilePath::StringType(FILE_PATH_LITERAL("foo")) +
                         supported_type);
  }
  CreateFilesForTest(file_names, &data);

  // Mark all files with failed scans.
  for (const auto& path : data.paths) {
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                enterprise_connectors::TriggeredRule::BLOCK));
  }

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, UnsupportedTypesDefaultPolicy) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.these"), FILE_PATH_LITERAL("foo.file"),
       FILE_PATH_LITERAL("foo.types"), FILE_PATH_LITERAL("foo.are"),
       FILE_PATH_LITERAL("foo.not"), FILE_PATH_LITERAL("foo.supported")},
      &data);

  // Mark all files with failed scans.
  for (const auto& path : data.paths) {
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                enterprise_connectors::TriggeredRule::WARN));
  }

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, UnsupportedTypesBlockPolicy) {
  SetUnsupportedFileTypePolicy(
      BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS);
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest(
      {FILE_PATH_LITERAL("foo.these"), FILE_PATH_LITERAL("foo.file"),
       FILE_PATH_LITERAL("foo.types"), FILE_PATH_LITERAL("foo.are"),
       FILE_PATH_LITERAL("foo.not"), FILE_PATH_LITERAL("foo.supported")},
      &data);

  // Mark all files with failed scans.
  for (const auto& path : data.paths) {
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                enterprise_connectors::TriggeredRule::WARN));
  }

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, SupportedAndUnsupportedTypes) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

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
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                enterprise_connectors::TriggeredRule::BLOCK));
  }

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, UnsupportedTypeAndDLPFailure) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest({FILE_PATH_LITERAL("foo.unsupported_extension"),
                      FILE_PATH_LITERAL("dlp_fail.doc")},
                     &data);

  // Mark DLP as failure.
  SetDLPResponse(FakeDeepScanningDialogDelegate::DlpResponse(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS, "rule",
      enterprise_connectors::TriggeredRule::BLOCK));

  bool called = false;
  ScanUpload(contents(), std::move(data),
             base::BindOnce(
                 [](bool* called, const DeepScanningDialogDelegate::Data& data,
                    const DeepScanningDialogDelegate::Result& result) {
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

class DeepScanningDialogDelegateResultHandlingTest
    : public BaseTest,
      public testing::WithParamInterface<
          std::tuple<BinaryUploadService::Result, bool>> {
 public:
  DeepScanningDialogDelegateResultHandlingTest() = default;

  void SetUp() override {
    BaseTest::SetUp();
    EnableFeatures();
    SetDlpPolicy(CHECK_UPLOADS);
    SetMalwarePolicy(SEND_UPLOADS);

    DeepScanningDialogDelegate::SetFactoryForTesting(base::BindRepeating(
        &FakeDeepScanningDialogDelegate::Create, run_loop_.QuitClosure(),
        base::BindRepeating(&DeepScanningDialogDelegateResultHandlingTest::
                                ConnectorStatusCallback,
                            base::Unretained(this)),
        /*encryption_callback=*/
        base::BindRepeating([](const base::FilePath& path) { return false; }),
        kDmToken));
  }

  BinaryUploadService::Result result() const { return std::get<0>(GetParam()); }

  enterprise_connectors::ContentAnalysisResponse ConnectorStatusCallback(
      const base::FilePath& path) {
    return FakeDeepScanningDialogDelegate::SuccessfulResponse(
        {"dlp", "malware"});
  }

 protected:
  ScopedSetDMToken scoped_dm_token_{
      policy::DMToken::CreateValidTokenForTesting(kDmToken)};
};

TEST_P(DeepScanningDialogDelegateResultHandlingTest, Test) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  FakeDeepScanningDialogDelegate::SetResponseResult(result());
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), url, &data,
      enterprise_connectors::AnalysisConnector::FILE_ATTACHED));

  CreateFilesForTest({FILE_PATH_LITERAL("foo.txt")}, &data);

  bool called = false;
  ScanUpload(
      contents(), std::move(data),
      base::BindLambdaForTesting(
          [this, &called](const DeepScanningDialogDelegate::Data& data,
                          const DeepScanningDialogDelegate::Result& result) {
            EXPECT_EQ(0u, data.text.size());
            EXPECT_EQ(1u, data.paths.size());
            EXPECT_EQ(0u, result.text_results.size());
            EXPECT_EQ(1u, result.paths_results.size());

            bool expected =
                DeepScanningDialogDelegate::ResultShouldAllowDataUse(
                    this->result(), data.settings);
            EXPECT_EQ(expected, result.paths_results[0]);
            called = true;
          }));
  RunUntilDone();
  EXPECT_TRUE(called);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    DeepScanningDialogDelegateResultHandlingTest,
    testing::Combine(
        testing::Values(BinaryUploadService::Result::UNKNOWN,
                        BinaryUploadService::Result::SUCCESS,
                        BinaryUploadService::Result::UPLOAD_FAILURE,
                        BinaryUploadService::Result::TIMEOUT,
                        BinaryUploadService::Result::FILE_TOO_LARGE,
                        BinaryUploadService::Result::FAILED_TO_GET_TOKEN,
                        BinaryUploadService::Result::UNAUTHORIZED,
                        BinaryUploadService::Result::FILE_ENCRYPTED),
        testing::Bool()));

class DeepScanningDialogDelegatePolicyResultsTest
    : public BaseTest,
      public testing::WithParamInterface<bool> {
 public:
  DeepScanningDialogDelegatePolicyResultsTest() = default;

  void SetUp() override {
    BaseTest::SetUp();
    EnableFeatures();
    // This is required since Connector policies can't return settings if
    // there are no URL patterns. Legacy policies don't need to account for
    // this since DLP is implicitly "*" on uploads.
    AddUrlsToCheckForMalwareOfUploadsForConnectors({"*"});
  }

  enterprise_connectors::AnalysisSettings settings() {
    // Clear the cache before getting settings so there's no race with the pref
    // change and the cached values being updated.
    enterprise_connectors::ConnectorsManager::GetInstance()
        ->ClearCacheForTesting();

    base::Optional<enterprise_connectors::AnalysisSettings> settings =
        enterprise_connectors::ConnectorsManager::GetInstance()
            ->GetAnalysisSettings(
                GURL(kTestUrl),
                enterprise_connectors::AnalysisConnector::FILE_ATTACHED);
    EXPECT_TRUE(settings.has_value());
    return std::move(settings.value());
  }
};

TEST_F(DeepScanningDialogDelegatePolicyResultsTest, BlockLargeFile) {
  // The value returned by ResultShouldAllowDataUse for FILE_TOO_LARGE should
  // match the BlockLargeFilePolicy.
  SetBlockLargeFilePolicy(
      BlockLargeFileTransferValues::BLOCK_LARGE_UPLOADS_AND_DOWNLOADS);
  EXPECT_FALSE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_TOO_LARGE, settings()));

  SetBlockLargeFilePolicy(BlockLargeFileTransferValues::BLOCK_LARGE_DOWNLOADS);
  EXPECT_TRUE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_TOO_LARGE, settings()));

  SetBlockLargeFilePolicy(BlockLargeFileTransferValues::BLOCK_LARGE_UPLOADS);
  EXPECT_FALSE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_TOO_LARGE, settings()));

  SetBlockLargeFilePolicy(BlockLargeFileTransferValues::BLOCK_NONE);
  EXPECT_TRUE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_TOO_LARGE, settings()));
}

TEST_F(DeepScanningDialogDelegatePolicyResultsTest,
       AllowPasswordProtectedFiles) {
  // The value returned by ResultShouldAllowDataUse for FILE_ENCRYPTED should
  // match the AllowPasswordProtectedFiles policy.
  SetAllowPasswordPolicy(
      AllowPasswordProtectedFilesValues::ALLOW_UPLOADS_AND_DOWNLOADS);
  EXPECT_TRUE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_ENCRYPTED, settings()));

  SetAllowPasswordPolicy(AllowPasswordProtectedFilesValues::ALLOW_DOWNLOADS);
  EXPECT_FALSE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_ENCRYPTED, settings()));

  SetAllowPasswordPolicy(AllowPasswordProtectedFilesValues::ALLOW_UPLOADS);
  EXPECT_TRUE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_ENCRYPTED, settings()));

  SetAllowPasswordPolicy(AllowPasswordProtectedFilesValues::ALLOW_NONE);
  EXPECT_FALSE(DeepScanningDialogDelegate::ResultShouldAllowDataUse(
      BinaryUploadService::Result::FILE_ENCRYPTED, settings()));
}

}  // namespace safe_browsing
