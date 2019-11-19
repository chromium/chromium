// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

constexpr char kDmToken[] = "dm_token";
constexpr char kTestUrl[] = "http://example.com";

constexpr char kTestHttpsSchemePatternUrl[] = "https://*";
constexpr char kTestChromeSchemePatternUrl[] = "chrome://*";
constexpr char kTestDevtoolsSchemePatternUrl[] = "devtools://*";

constexpr char kTestPathPatternUrl[] = "*/a/specific/path/";
constexpr char kTestPortPatternUrl[] = "*:1234";
constexpr char kTestQueryPatternUrl[] = "*?q=5678";

class ScopedSetDMToken {
 public:
  explicit ScopedSetDMToken(const policy::DMToken& dm_token) {
    DeepScanningDialogDelegate::SetDMTokenForTesting(dm_token);
  }
  ~ScopedSetDMToken() {
    DeepScanningDialogDelegate::SetDMTokenForTesting(
        policy::DMToken::CreateEmptyTokenForTesting());
  }
};

class BaseTest : public testing::Test {
 public:
  BaseTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void EnableFeature(const base::Feature& feature) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeature(feature);
  }

  void DisableFeature(const base::Feature& feature) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(feature);
  }

  void SetDlpPolicy(CheckContentComplianceValues state) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kCheckContentCompliance, state);
  }

  void SetWaitPolicy(DelayDeliveryUntilVerdictValues state) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kDelayDeliveryUntilVerdict, state);
  }

  void SetMalwarePolicy(SendFilesForMalwareCheckValues state) {
    profile_->GetPrefs()->SetInteger(
        prefs::kSafeBrowsingSendFilesForMalwareCheck, state);
  }

  void AddUrlToList(const char* pref_name, const GURL& url) {
    ListPrefUpdate updater(TestingBrowserProcess::GetGlobal()->local_state(),
                           pref_name);
    updater->GetList().emplace_back(url.host());
  }

  void AddUrlToList(const char* pref_name, const char* url) {
    ListPrefUpdate updater(TestingBrowserProcess::GetGlobal()->local_state(),
                           pref_name);
    updater->GetList().emplace_back(url);
  }

  Profile* profile() { return profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingPrefServiceSimple pref_service_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
};

using DeepScanningDialogDelegateIsEnabledTest = BaseTest;

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeatureNoDMTokenNoPref) {
  DisableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoDMTokenNoPref) {
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoDMToken) {
  EnableFeature(kDeepScanningOfUploads);
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeatureNoPref) {
  DisableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeatureNoDMToken) {
  DisableFeature(kDeepScanningOfUploads);
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateInvalidTokenForTesting());

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoFeature) {
  DisableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref) {
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref2) {
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_NONE);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpNoPref3) {
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabled) {
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpEnabled2) {
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpDisabledByList) {
  GURL url(kTestUrl);
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS);
  AddUrlToList(prefs::kURLsToNotCheckComplianceOfUploadedContent, url);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, DlpDisabledByListWithPatterns) {
  EnableFeature(kDeepScanningOfUploads);
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

  DeepScanningDialogDelegate::Data data;

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://example.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("https://google.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://google.com"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("chrome://version/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://version"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("devtools://devtools/bundled/inspector.html"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://devtools/bundled/inspector.html"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/a/specific/path/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/not/a/specific/path/"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:1234"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:4321"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=5678"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=8765"), &data));
  EXPECT_TRUE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref) {
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref2) {
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(DO_NOT_SCAN);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoPref4) {
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoList) {
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareNoList2) {
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);

  DeepScanningDialogDelegate::Data data;
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(profile(), GURL(), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareEnabled) {
  GURL url(kTestUrl);
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, url);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, NoScanInIncognito) {
  GURL url(kTestUrl);
  EnableFeature(kDeepScanningOfUploads);
  ScopedSetDMToken scoped_dm_token(
      policy::DMToken::CreateValidTokenForTesting(kDmToken));
  SetDlpPolicy(CHECK_UPLOADS_AND_DOWNLOADS);
  SetMalwarePolicy(SEND_UPLOADS_AND_DOWNLOADS);
  AddUrlToList(prefs::kURLsToCheckForMalwareOfUploadedContent, url);

  DeepScanningDialogDelegate::Data data;
  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  // The same URL should not trigger a scan in incognito.
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile()->GetOffTheRecordProfile(), url, &data));
}

TEST_F(DeepScanningDialogDelegateIsEnabledTest, MalwareEnabledWithPatterns) {
  EnableFeature(kDeepScanningOfUploads);
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

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://example.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("chrome://version/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://version/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("devtools://devtools/bundled/inspector.html"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://devtools/bundled/inspector.html"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("https://google.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("custom://google.com"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/a/specific/path/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com/not/a/specific/path/"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:1234"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com:4321"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);

  EXPECT_TRUE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=5678"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_TRUE(data.do_malware_scan);
  EXPECT_FALSE(DeepScanningDialogDelegate::IsEnabled(
      profile(), GURL("http://google.com?q=8765"), &data));
  EXPECT_FALSE(data.do_dlp_scan);
  EXPECT_FALSE(data.do_malware_scan);
}

class DeepScanningDialogDelegateAuditOnlyTest : public BaseTest {
 protected:
  void RunUntilDone() { run_loop_.Run(); }

  content::WebContents* contents() {
    if (!web_contents_) {
      content::WebContents::CreateParams params(profile());
      web_contents_ = content::WebContents::Create(params);
    }
    return web_contents_.get();
  }

  void PathFailsDeepScan(base::FilePath path,
                         DeepScanningClientResponse response) {
    failures_.insert({std::move(path), std::move(response)});
  }

 private:
  void SetUp() override {
    BaseTest::SetUp();

    EnableFeature(kDeepScanningOfUploads);
    SetDlpPolicy(CHECK_UPLOADS);
    SetMalwarePolicy(SEND_UPLOADS);

    DeepScanningDialogDelegate::SetFactoryForTesting(base::BindRepeating(
        &FakeDeepScanningDialogDelegate::Create, run_loop_.QuitClosure(),
        base::Bind(&DeepScanningDialogDelegateAuditOnlyTest::StatusCallback,
                   base::Unretained(this)),
        kDmToken));
  }

  DeepScanningClientResponse StatusCallback(const base::FilePath& path) {
    // The path succeeds if it is not in the |failures_| maps.
    auto it = failures_.find(path);
    return it != failures_.end()
               ? it->second
               : FakeDeepScanningDialogDelegate::SuccessfulResponse();
  }

  base::RunLoop run_loop_;
  std::unique_ptr<content::WebContents> web_contents_;
  ScopedSetDMToken scoped_dm_token_{
      policy::DMToken::CreateValidTokenForTesting(kDmToken)};

  // Paths in this map will be consider to have failed deep scan checks.
  // The actual failure response is given for each path.
  std::map<base::FilePath, DeepScanningClientResponse> failures_;
};

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, Empty) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  // Keep |data| empty by not setting any text or paths.

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.text.emplace_back(base::UTF8ToUTF16("bar"));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileData) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, FileData2) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bar.doc"));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileData) {
  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/bar.doc"));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, StringFileDataPartialSuccess) {
  SetWaitPolicy(DELAY_UPLOADS);

  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.text.emplace_back(base::UTF8ToUTF16("foo"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_malware_1.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_malware_2.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_dlp_status.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_fail_dlp_rule.doc"));

  // Mark some files with failed scans.
  PathFailsDeepScan(data.paths[1],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        MalwareDeepScanningVerdict::UWS));
  PathFailsDeepScan(data.paths[2],
                    FakeDeepScanningDialogDelegate::MalwareResponse(
                        MalwareDeepScanningVerdict::MALWARE));
  PathFailsDeepScan(data.paths[3],
                    FakeDeepScanningDialogDelegate::DlpResponse(
                        DlpDeepScanningVerdict::FAILURE, "",
                        DlpDeepScanningVerdict::TriggeredRule::REPORT_ONLY));
  PathFailsDeepScan(data.paths[4],
                    FakeDeepScanningDialogDelegate::DlpResponse(
                        DlpDeepScanningVerdict::SUCCESS, "rule",
                        DlpDeepScanningVerdict::TriggeredRule::BLOCK));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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
            EXPECT_FALSE(result.paths_results[3]);
            EXPECT_FALSE(result.paths_results[4]);
            *called = true;
          },
          &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, EmptyWait) {
  SetWaitPolicy(DELAY_UPLOADS);

  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
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
  SetWaitPolicy(DELAY_UPLOADS);

  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.bzip"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.cab"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.docx"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.eps"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.gzip"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.hwp"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.img_for_ocr"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.kml"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.kmz"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.odp"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.ods"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.odt"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.pdf"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.ppt"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.pptx"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.ps"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.rar"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.rtf"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.sdc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.sdd"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.sdw"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.seven_z"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.sxc"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.sxi"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.sxw"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.tar"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.ttf"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.txt"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.wml"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.wpd"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.xls"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.xlsx"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.xml"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.xps"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.zip"));

  // Mark all files with failed scans.
  for (const auto& path : data.paths)
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                MalwareDeepScanningVerdict::UWS));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
      base::BindOnce(
          [](bool* called, const DeepScanningDialogDelegate::Data& data,
             const DeepScanningDialogDelegate::Result& result) {
            EXPECT_EQ(36u, data.paths.size());
            ASSERT_EQ(36u, result.paths_results.size());

            // The supported types should be marked as false.
            for (const auto& result : result.paths_results)
              EXPECT_FALSE(result);
            *called = true;
          },
          &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, UnsupportedTypes) {
  SetWaitPolicy(DELAY_UPLOADS);

  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.these"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.file"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.types"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.are"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.not"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.supported"));

  // Mark all files with failed scans.
  for (const auto& path : data.paths)
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                MalwareDeepScanningVerdict::UWS));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
      base::BindOnce(
          [](bool* called, const DeepScanningDialogDelegate::Data& data,
             const DeepScanningDialogDelegate::Result& result) {
            EXPECT_EQ(6u, data.paths.size());
            ASSERT_EQ(6u, result.paths_results.size());

            // The unsupported types should be marked as true.
            for (const bool path_result : result.paths_results)
              EXPECT_TRUE(path_result);
            *called = true;
          },
          &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

TEST_F(DeepScanningDialogDelegateAuditOnlyTest, SupportedAndUnsupportedTypes) {
  SetWaitPolicy(DELAY_UPLOADS);

  GURL url(kTestUrl);
  DeepScanningDialogDelegate::Data data;
  ASSERT_TRUE(DeepScanningDialogDelegate::IsEnabled(profile(), url, &data));

  // Only 3 of these file types are supported (bzip, cab and doc). They are
  // mixed in the list so as to show that insertion order does not matter.
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.bzip"));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.these"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.file"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.types"));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.cab"));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.are"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.not"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.supported"));
  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo_no_extension"));

  data.paths.emplace_back(FILE_PATH_LITERAL("/tmp/foo.doc"));

  // Mark all files with failed scans.
  for (const auto& path : data.paths)
    PathFailsDeepScan(path, FakeDeepScanningDialogDelegate::MalwareResponse(
                                MalwareDeepScanningVerdict::UWS));

  bool called = false;
  DeepScanningDialogDelegate::ShowForWebContents(
      contents(), std::move(data),
      base::BindOnce(
          [](bool* called, const DeepScanningDialogDelegate::Data& data,
             const DeepScanningDialogDelegate::Result& result) {
            EXPECT_EQ(10u, data.paths.size());
            ASSERT_EQ(10u, result.paths_results.size());

            // The unsupported types should be marked as true, and the valid
            // types as false since they are marked as failed scans.
            size_t i = 0;
            for (const bool expected : {false, true, true, true, false, true,
                                        true, true, true, false}) {
              ASSERT_EQ(expected, result.paths_results[i]);
              ++i;
            }
            *called = true;
          },
          &called));
  RunUntilDone();
  EXPECT_TRUE(called);
}

}  // namespace

}  // namespace safe_browsing
