// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_manager.h"

#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

constexpr char kTestUrlMatchingPattern[] = "google.com";

constexpr char kTestUrlNotMatchingPattern[] = "chromium.org";

constexpr AnalysisConnector kAllAnalysisConnectors[] = {
    AnalysisConnector::FILE_DOWNLOADED, AnalysisConnector::FILE_ATTACHED,
    AnalysisConnector::BULK_DATA_ENTRY};

constexpr ReportingConnector kAllReportingConnectors[] = {
    ReportingConnector::SECURITY_EVENT};

constexpr safe_browsing::BlockLargeFileTransferValues
    kAllBlockLargeFilesPolicyValues[] = {
        safe_browsing::BlockLargeFileTransferValues::BLOCK_NONE,
        safe_browsing::BlockLargeFileTransferValues::BLOCK_LARGE_DOWNLOADS,
        safe_browsing::BlockLargeFileTransferValues::BLOCK_LARGE_UPLOADS,
        safe_browsing::BlockLargeFileTransferValues::
            BLOCK_LARGE_UPLOADS_AND_DOWNLOADS};

constexpr safe_browsing::BlockUnsupportedFiletypesValues
    kAllBlockUnsupportedFileTypesValues[] = {
        safe_browsing::BlockUnsupportedFiletypesValues::
            BLOCK_UNSUPPORTED_FILETYPES_NONE,
        safe_browsing::BlockUnsupportedFiletypesValues::
            BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS,
        safe_browsing::BlockUnsupportedFiletypesValues::
            BLOCK_UNSUPPORTED_FILETYPES_UPLOADS,
        safe_browsing::BlockUnsupportedFiletypesValues::
            BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS,
};

constexpr safe_browsing::AllowPasswordProtectedFilesValues
    kAllAllowEncryptedPolicyValues[] = {
        safe_browsing::AllowPasswordProtectedFilesValues::ALLOW_NONE,
        safe_browsing::AllowPasswordProtectedFilesValues::ALLOW_DOWNLOADS,
        safe_browsing::AllowPasswordProtectedFilesValues::ALLOW_UPLOADS,
        safe_browsing::AllowPasswordProtectedFilesValues::
            ALLOW_UPLOADS_AND_DOWNLOADS};

constexpr safe_browsing::DelayDeliveryUntilVerdictValues
    kAllDelayDeliveryUntilVerdictValues[] = {
        safe_browsing::DelayDeliveryUntilVerdictValues::DELAY_NONE,
        safe_browsing::DelayDeliveryUntilVerdictValues::DELAY_DOWNLOADS,
        safe_browsing::DelayDeliveryUntilVerdictValues::DELAY_UPLOADS,
        safe_browsing::DelayDeliveryUntilVerdictValues::
            DELAY_UPLOADS_AND_DOWNLOADS,
};

constexpr char kEmptySettingsPref[] = "[]";

constexpr char kNormalAnalysisSettingsPref[] = R"([
  {
    "service_provider": "google",
    "enable": [
      {"url_list": ["*"], "tags": ["dlp", "malware"]},
    ],
    "disable": [
      {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
      {"url_list": ["no.malware.com", "no.dlp.or.malware.ca"],
           "tags": ["malware"]},
    ],
    "block_until_verdict": 1,
    "block_password_protected": true,
    "block_large_files": true,
    "block_unsupported_file_types": true,
  },
])";

constexpr char kNormalReportingSettingsPref[] = R"([
  {
    "service_provider": "google"
  }
])";

constexpr char kDlpAndMalwareUrl[] = "https://foo.com";
constexpr char kOnlyDlpUrl[] = "https://no.malware.com";
constexpr char kOnlyMalwareUrl[] = "https://no.dlp.com";
constexpr char kNoTagsUrl[] = "https://no.dlp.or.malware.ca";

}  // namespace

class ConnectorsManagerTest : public testing::Test {
 public:
  ConnectorsManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void SetUp() override { ConnectorsManager::GetInstance()->SetUpForTesting(); }

  void TearDown() override {
    ConnectorsManager::GetInstance()->TearDownForTesting();
  }

  void ValidateSettings(const AnalysisSettings& settings) {
    ASSERT_EQ(settings.block_until_verdict, expected_block_until_verdict_);
    ASSERT_EQ(settings.block_password_protected_files,
              expected_block_password_protected_files_);
    ASSERT_EQ(settings.block_large_files, expected_block_large_files_);
    ASSERT_EQ(settings.block_unsupported_file_types,
              expected_block_unsupported_file_types_);
    ASSERT_EQ(settings.tags, expected_tags_);
  }

  void ValidateSettings(const ReportingSettings& settings) {
    // For now, the URL is the same for both legacy and new policies, so
    // checking the specific URL here.  When service providers become
    // configurable this will change.
    ASSERT_EQ(GURL("https://chromereporting-pa.googleapis.com/v1/events"),
              settings.reporting_url);
  }

  class ScopedConnectorPref {
   public:
    ScopedConnectorPref(const char* pref, const char* pref_value)
        : pref_(pref) {
      auto maybe_pref_value =
          base::JSONReader::Read(pref_value, base::JSON_ALLOW_TRAILING_COMMAS);
      EXPECT_TRUE(maybe_pref_value.has_value());
      TestingBrowserProcess::GetGlobal()->local_state()->Set(
          pref, maybe_pref_value.value());
    }

    ~ScopedConnectorPref() {
      TestingBrowserProcess::GetGlobal()->local_state()->ClearPref(pref_);
    }

   private:
    const char* pref_;
  };

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  GURL url_ = GURL("https://google.com");

  // Set to the default value of their legacy policy.
  std::set<std::string> expected_tags_ = {};
  BlockUntilVerdict expected_block_until_verdict_ = BlockUntilVerdict::NO_BLOCK;
  bool expected_block_password_protected_files_ = false;
  bool expected_block_large_files_ = false;
  bool expected_block_unsupported_file_types_ = false;
};

// Tests that permutations of legacy policies produce expected settings from a
// ConnectorsManager instance. T is a type used to iterate over policies with a
// {NONE, DOWNLOADS, UPLOADS, UPLOADS_AND_DOWNLOADS} pattern without testing
// every single permutation since these settings are independent.
template <typename T>
class ConnectorsManagerLegacyPoliciesTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<std::tuple<AnalysisConnector, T>> {
 public:
  ConnectorsManagerLegacyPoliciesTest<T>() {
    scoped_feature_list_.InitWithFeatures({}, {});
  }

  AnalysisConnector connector() const { return std::get<0>(this->GetParam()); }
  T tested_policy() const { return std::get<1>(this->GetParam()); }

  bool upload_scan() const {
    return connector() != AnalysisConnector::FILE_DOWNLOADED;
  }

  void TestPolicy() {
    upload_scan() ? TestPolicyOnUpload() : TestPolicyOnDownload();
  }

  void TestPolicyOnDownload() {
    // DLP only checks uploads by default and malware only checks downloads by
    // default. Overriding the appropriate policies subsequently will change the
    // tags matching the pattern.
    auto default_settings =
        ConnectorsManager::GetInstance()->GetAnalysisSettings(url_,
                                                              connector());
    ASSERT_TRUE(default_settings.has_value());
    expected_tags_ = {"malware"};
    ValidateSettings(default_settings.value());

    // The DLP tag is still absent if the patterns don't match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckComplianceOfDownloadedContent)
        ->Append(kTestUrlNotMatchingPattern);
    auto exempt_pattern_dlp_settings =
        ConnectorsManager::GetInstance()->GetAnalysisSettings(url_,
                                                              connector());
    ASSERT_TRUE(exempt_pattern_dlp_settings.has_value());
    ValidateSettings(exempt_pattern_dlp_settings.value());

    // The DLP tag is added once the patterns do match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckComplianceOfDownloadedContent)
        ->Append(kTestUrlMatchingPattern);
    auto scan_pattern_dlp_settings =
        ConnectorsManager::GetInstance()->GetAnalysisSettings(url_,
                                                              connector());
    ASSERT_TRUE(scan_pattern_dlp_settings.has_value());
    expected_tags_ = {"dlp", "malware"};
    ValidateSettings(scan_pattern_dlp_settings.value());

    // The malware tag is removed once exempt patterns match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToNotCheckForMalwareOfDownloadedContent)
        ->Append(kTestUrlMatchingPattern);
    auto exempt_pattern_malware_settings =
        ConnectorsManager::GetInstance()->GetAnalysisSettings(url_,
                                                              connector());
    ASSERT_TRUE(exempt_pattern_malware_settings.has_value());
    expected_tags_ = {"dlp"};
    ValidateSettings(exempt_pattern_malware_settings.value());

    // Both tags are removed once the patterns don't match them, resulting in no
    // settings.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckComplianceOfDownloadedContent)
        ->Remove(1, nullptr);
    auto no_settings = ConnectorsManager::GetInstance()->GetAnalysisSettings(
        url_, connector());
    ASSERT_FALSE(no_settings.has_value());
  }

  void TestPolicyOnUpload() {
    // DLP only checks uploads by default and malware only checks downloads by
    // default. Overriding the appropriate policies subsequently will change the
    // tags matching the pattern.
    auto default_settings =
        ConnectorsManager::GetInstance()->GetAnalysisSettings(url_,
                                                              connector());
    ASSERT_TRUE(default_settings.has_value());
    expected_tags_ = {"dlp"};
    ValidateSettings(default_settings.value());

    // The malware tag is still absent if the patterns don't match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckForMalwareOfUploadedContent)
        ->Append(kTestUrlNotMatchingPattern);
    auto exempt_pattern_malware_settings =
        ConnectorsManager::GetInstance()->GetAnalysisSettings(url_,
                                                              connector());
    ASSERT_TRUE(exempt_pattern_malware_settings.has_value());
    ValidateSettings(exempt_pattern_malware_settings.value());

    // The malware tag is added once the patterns do match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckForMalwareOfUploadedContent)
        ->Append(kTestUrlMatchingPattern);
    auto scan_pattern_malware_settings =
        ConnectorsManager::GetInstance()->GetAnalysisSettings(url_,
                                                              connector());
    ASSERT_TRUE(scan_pattern_malware_settings.has_value());
    expected_tags_ = {"dlp", "malware"};
    ValidateSettings(scan_pattern_malware_settings.value());

    // The DLP tag is removed once exempt patterns match it.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToNotCheckComplianceOfUploadedContent)
        ->Append(kTestUrlMatchingPattern);
    auto exempt_pattern_dlp_settings =
        ConnectorsManager::GetInstance()->GetAnalysisSettings(url_,
                                                              connector());
    ASSERT_TRUE(exempt_pattern_dlp_settings.has_value());
    expected_tags_ = {"malware"};
    ValidateSettings(exempt_pattern_dlp_settings.value());

    // Both tags are removed once the patterns don't match them, resulting in no
    // settings.
    ListPrefUpdate(TestingBrowserProcess::GetGlobal()->local_state(),
                   prefs::kURLsToCheckForMalwareOfUploadedContent)
        ->Remove(1, nullptr);
    auto no_settings = ConnectorsManager::GetInstance()->GetAnalysisSettings(
        url_, connector());
    ASSERT_FALSE(no_settings.has_value());
  }
};

class ConnectorsManagerBlockLargeFileTest
    : public ConnectorsManagerLegacyPoliciesTest<
          safe_browsing::BlockLargeFileTransferValues> {
 public:
  ConnectorsManagerBlockLargeFileTest() {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kBlockLargeFileTransfer, tested_policy());
    expected_block_large_files_ = [this]() {
      if (tested_policy() == safe_browsing::BLOCK_LARGE_UPLOADS_AND_DOWNLOADS)
        return true;
      if (tested_policy() == safe_browsing::BLOCK_NONE)
        return false;
      return upload_scan()
                 ? tested_policy() == safe_browsing::BLOCK_LARGE_UPLOADS
                 : tested_policy() == safe_browsing::BLOCK_LARGE_DOWNLOADS;
    }();
  }
};

TEST_P(ConnectorsManagerBlockLargeFileTest, Test) {
  TestPolicy();
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerBlockLargeFileTest,
    ConnectorsManagerBlockLargeFileTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::ValuesIn(kAllBlockLargeFilesPolicyValues)));

class ConnectorsManagerBlockUnsupportedFileTypesTest
    : public ConnectorsManagerLegacyPoliciesTest<
          safe_browsing::BlockUnsupportedFiletypesValues> {
 public:
  ConnectorsManagerBlockUnsupportedFileTypesTest() {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kBlockUnsupportedFiletypes, tested_policy());
    expected_block_unsupported_file_types_ = [this]() {
      if (tested_policy() ==
          safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_UPLOADS_AND_DOWNLOADS)
        return true;
      if (tested_policy() == safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_NONE)
        return false;
      return upload_scan()
                 ? tested_policy() ==
                       safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_UPLOADS
                 : tested_policy() ==
                       safe_browsing::BLOCK_UNSUPPORTED_FILETYPES_DOWNLOADS;
    }();
  }
};

TEST_P(ConnectorsManagerBlockUnsupportedFileTypesTest, Test) {
  TestPolicy();
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerBlockUnsupportedFileTypesTest,
    ConnectorsManagerBlockUnsupportedFileTypesTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::ValuesIn(kAllBlockUnsupportedFileTypesValues)));

class ConnectorsManagerAllowPasswordProtectedFilesTest
    : public ConnectorsManagerLegacyPoliciesTest<
          safe_browsing::AllowPasswordProtectedFilesValues> {
 public:
  ConnectorsManagerAllowPasswordProtectedFilesTest() {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kAllowPasswordProtectedFiles, tested_policy());
    expected_block_password_protected_files_ = [this]() {
      if (tested_policy() == safe_browsing::ALLOW_UPLOADS_AND_DOWNLOADS)
        return false;
      if (tested_policy() == safe_browsing::ALLOW_NONE)
        return true;
      return upload_scan() ? tested_policy() != safe_browsing::ALLOW_UPLOADS
                           : tested_policy() != safe_browsing::ALLOW_DOWNLOADS;
    }();
  }
};

TEST_P(ConnectorsManagerAllowPasswordProtectedFilesTest, Test) {
  TestPolicy();
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerAllowPasswordProtectedFilesTest,
    ConnectorsManagerAllowPasswordProtectedFilesTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::ValuesIn(kAllAllowEncryptedPolicyValues)));

class ConnectorsManagerDelayDeliveryUntilVerdictTest
    : public ConnectorsManagerLegacyPoliciesTest<
          safe_browsing::DelayDeliveryUntilVerdictValues> {
 public:
  ConnectorsManagerDelayDeliveryUntilVerdictTest() {
    TestingBrowserProcess::GetGlobal()->local_state()->SetInteger(
        prefs::kDelayDeliveryUntilVerdict, tested_policy());
    expected_block_until_verdict_ = [this]() {
      if (tested_policy() == safe_browsing::DELAY_UPLOADS_AND_DOWNLOADS)
        return BlockUntilVerdict::BLOCK;
      if (tested_policy() == safe_browsing::DELAY_NONE)
        return BlockUntilVerdict::NO_BLOCK;
      bool delay =
          (upload_scan() && tested_policy() == safe_browsing::DELAY_UPLOADS) ||
          (!upload_scan() && tested_policy() == safe_browsing::DELAY_DOWNLOADS);
      return delay ? BlockUntilVerdict::BLOCK : BlockUntilVerdict::NO_BLOCK;
    }();
  }
};

TEST_P(ConnectorsManagerDelayDeliveryUntilVerdictTest, Test) {
  TestPolicy();
}

INSTANTIATE_TEST_SUITE_P(
    ConnectorsManagerDelayDeliveryUntilVerdictTest,
    ConnectorsManagerDelayDeliveryUntilVerdictTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::ValuesIn(kAllDelayDeliveryUntilVerdictValues)));

class ConnectorsManagerConnectorPoliciesTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<
          std::tuple<AnalysisConnector, const char*>> {
 public:
  ConnectorsManagerConnectorPoliciesTest() {
    scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
  }

  AnalysisConnector connector() const { return std::get<0>(GetParam()); }

  const char* url() const { return std::get<1>(GetParam()); }

  const char* pref() const { return ConnectorPref(connector()); }

  void SetUpExpectedAnalysisSettings(const char* pref) {
    auto expected_settings = ExpectedAnalysisSettings(pref, url());
    expect_settings_ = expected_settings.has_value();
    if (expected_settings.has_value()) {
      expected_tags_ = expected_settings.value().tags;
      expected_block_until_verdict_ =
          expected_settings.value().block_until_verdict;
      expected_block_password_protected_files_ =
          expected_settings.value().block_password_protected_files;
      expected_block_unsupported_file_types_ =
          expected_settings.value().block_unsupported_file_types;
      expected_block_large_files_ = expected_settings.value().block_large_files;
    }
  }

  void SetUpExpectedReportingSettings(const char* pref) {
    auto expected_settings = ExpectedReportingSettings(pref);
    expect_settings_ = expected_settings.has_value();
  }

 protected:
  base::Optional<AnalysisSettings> ExpectedAnalysisSettings(const char* pref,
                                                            const char* url) {
    if (pref == kEmptySettingsPref || url == kNoTagsUrl)
      return base::nullopt;

    AnalysisSettings settings;

    settings.block_until_verdict = BlockUntilVerdict::BLOCK;
    settings.block_password_protected_files = true;
    settings.block_large_files = true;
    settings.block_unsupported_file_types = true;

    if (url == kDlpAndMalwareUrl)
      settings.tags = {"dlp", "malware"};
    else if (url == kOnlyDlpUrl)
      settings.tags = {"dlp"};
    else if (url == kOnlyMalwareUrl)
      settings.tags = {"malware"};

    return settings;
  }

  base::Optional<ReportingSettings> ExpectedReportingSettings(
      const char* pref) {
    if (pref == kEmptySettingsPref)
      return base::nullopt;

    ReportingSettings settings;
    return settings;
  }

  bool expect_settings_;
};

TEST_P(ConnectorsManagerConnectorPoliciesTest, NormalPref) {
  ASSERT_TRUE(ConnectorsManager::GetInstance()
                  ->GetAnalysisConnectorsSettingsForTesting()
                  .empty());
  ScopedConnectorPref scoped_pref(pref(), kNormalAnalysisSettingsPref);
  SetUpExpectedAnalysisSettings(kNormalAnalysisSettingsPref);

  // Verify that the expected settings are returned normally.
  auto settings_from_manager =
      ConnectorsManager::GetInstance()->GetAnalysisSettings(GURL(url()),
                                                            connector());
  ASSERT_EQ(expect_settings_, settings_from_manager.has_value());
  if (settings_from_manager.has_value())
    ValidateSettings(settings_from_manager.value());

  // Verify that the expected settings are also returned by the cached settings.
  const auto& cached_settings = ConnectorsManager::GetInstance()
                                    ->GetAnalysisConnectorsSettingsForTesting();
  ASSERT_EQ(1u, cached_settings.size());
  ASSERT_EQ(1u, cached_settings.count(connector()));
  ASSERT_EQ(1u, cached_settings.at(connector()).size());

  auto settings_from_cache =
      cached_settings.at(connector()).at(0).GetAnalysisSettings(GURL(url()));
  ASSERT_EQ(expect_settings_, settings_from_cache.has_value());
  if (settings_from_cache.has_value())
    ValidateSettings(settings_from_cache.value());
}

TEST_P(ConnectorsManagerConnectorPoliciesTest, EmptyPref) {
  // If the connector's settings list is empty, no analysis settings are ever
  // returned.
  ASSERT_TRUE(ConnectorsManager::GetInstance()
                  ->GetAnalysisConnectorsSettingsForTesting()
                  .empty());
  ScopedConnectorPref scoped_pref(pref(), kEmptySettingsPref);

  ASSERT_FALSE(ConnectorsManager::GetInstance()
                   ->GetAnalysisSettings(GURL(url()), connector())
                   .has_value());

  ASSERT_TRUE(ConnectorsManager::GetInstance()
                  ->GetAnalysisConnectorsSettingsForTesting()
                  .empty());
}

INSTANTIATE_TEST_CASE_P(
    ConnectorsManagerConnectorPoliciesTest,
    ConnectorsManagerConnectorPoliciesTest,
    testing::Combine(testing::ValuesIn(kAllAnalysisConnectors),
                     testing::Values(kDlpAndMalwareUrl,
                                     kOnlyDlpUrl,
                                     kOnlyMalwareUrl,
                                     kNoTagsUrl)));

class ConnectorsManagerAnalysisConnectorsTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<AnalysisConnector> {
 public:
  explicit ConnectorsManagerAnalysisConnectorsTest(bool enable = true) {
    if (enable) {
      scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {kEnterpriseConnectorsEnabled});
    }
  }

  AnalysisConnector connector() const { return GetParam(); }

  const char* pref() const { return ConnectorPref(connector()); }
};

TEST_P(ConnectorsManagerAnalysisConnectorsTest, DynamicPolicies) {
  // The cache is initially empty.
  auto* manager = ConnectorsManager::GetInstance();
  ASSERT_TRUE(manager->GetAnalysisConnectorsSettingsForTesting().empty());

  // Once the pref is updated, the settings should be cached, and analysis
  // settings can be obtained.
  {
    ScopedConnectorPref scoped_pref(pref(), kNormalAnalysisSettingsPref);

    const auto& cached_settings =
        manager->GetAnalysisConnectorsSettingsForTesting();
    ASSERT_FALSE(cached_settings.empty());
    ASSERT_EQ(1u, cached_settings.count(connector()));
    ASSERT_EQ(1u, cached_settings.at(connector()).size());

    auto settings = cached_settings.at(connector())
                        .at(0)
                        .GetAnalysisSettings(GURL(kDlpAndMalwareUrl));
    ASSERT_TRUE(settings.has_value());
    expected_block_until_verdict_ = BlockUntilVerdict::BLOCK;
    expected_block_password_protected_files_ = true;
    expected_block_large_files_ = true;
    expected_block_unsupported_file_types_ = true;
    expected_tags_ = {"dlp", "malware"};
    ValidateSettings(settings.value());
  }

  // The cache should be empty again after the pref is reset.
  ASSERT_TRUE(manager->GetAnalysisConnectorsSettingsForTesting().empty());
}

INSTANTIATE_TEST_CASE_P(ConnectorsManagerAnalysisConnectorsTest,
                        ConnectorsManagerAnalysisConnectorsTest,
                        testing::ValuesIn(kAllAnalysisConnectors));

class ConnectorsManagerAnalysisNoFeatureTest
    : public ConnectorsManagerAnalysisConnectorsTest {
 public:
  ConnectorsManagerAnalysisNoFeatureTest()
      : ConnectorsManagerAnalysisConnectorsTest(false) {}
};

TEST_P(ConnectorsManagerAnalysisNoFeatureTest, Test) {
  ScopedConnectorPref scoped_pref(pref(), kNormalAnalysisSettingsPref);

  if (connector() == AnalysisConnector::FILE_DOWNLOADED)
    expected_tags_ = {"malware"};
  else
    expected_tags_ = {"dlp"};

  for (const char* url :
       {kDlpAndMalwareUrl, kOnlyDlpUrl, kOnlyMalwareUrl, kNoTagsUrl}) {
    auto settings = ConnectorsManager::GetInstance()->GetAnalysisSettings(
        GURL(url), connector());
    ASSERT_TRUE(settings.has_value());
    ValidateSettings(settings.value());
  }

  // No cached settings imply the connector value was never read.
  ASSERT_TRUE(ConnectorsManager::GetInstance()
                  ->GetAnalysisConnectorsSettingsForTesting()
                  .empty());
}

INSTANTIATE_TEST_CASE_P(,
                        ConnectorsManagerAnalysisNoFeatureTest,
                        testing::ValuesIn(kAllAnalysisConnectors));

class ConnectorsManagerReportingDynamicTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<ReportingConnector> {
 public:
  ConnectorsManagerReportingDynamicTest() {
    scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
  }

  ReportingConnector connector() const { return GetParam(); }

  const char* pref() const { return ConnectorPref(connector()); }
};

TEST_P(ConnectorsManagerReportingDynamicTest, DynamicPolicies) {
  // The cache is initially empty.
  auto* manager = ConnectorsManager::GetInstance();
  ASSERT_TRUE(manager->GetReportingConnectorsSettingsForTesting().empty());

  // Once the pref is updated, the settings should be cached, and reporting
  // settings can be obtained.
  {
    ScopedConnectorPref scoped_pref(pref(), kNormalReportingSettingsPref);

    const auto& cached_settings =
        manager->GetReportingConnectorsSettingsForTesting();
    ASSERT_FALSE(cached_settings.empty());
    ASSERT_EQ(1u, cached_settings.count(connector()));
    ASSERT_EQ(1u, cached_settings.at(connector()).size());

    auto settings =
        cached_settings.at(connector()).at(0).GetReportingSettings();
    ASSERT_TRUE(settings.has_value());
    ValidateSettings(settings.value());
  }

  // The cache should be empty again after the pref is reset.
  ASSERT_TRUE(manager->GetAnalysisConnectorsSettingsForTesting().empty());
}

INSTANTIATE_TEST_CASE_P(ConnectorsManagerReportingDynamicTest,
                        ConnectorsManagerReportingDynamicTest,
                        testing::ValuesIn(kAllReportingConnectors));

// Tests to make sure getting reporting settings works with both new and legacy
// feature flags and policies.  The parameter for these tests is a tuple of:
//
//   enum class ReportingConnector[]: array of all reporting connectors.
//   bool: enable feature flag.
//   int: legacy policy value.  0: don't set, 1: set to true, 2: set to false.
//   int: new policy value.  0: don't set, 1: set to normal, 2: set to empty.
class ConnectorsManagerReportingFeatureTest
    : public ConnectorsManagerTest,
      public testing::WithParamInterface<
          std::tuple<ReportingConnector, bool, int, int>> {
 public:
  ConnectorsManagerReportingFeatureTest() {
    if (enable_feature_flag()) {
      scoped_feature_list_.InitWithFeatures({kEnterpriseConnectorsEnabled}, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {kEnterpriseConnectorsEnabled});
    }
  }

  ReportingConnector connector() const { return std::get<0>(GetParam()); }
  bool enable_feature_flag() const { return std::get<1>(GetParam()); }
  int legacy_policy_value() const { return std::get<2>(GetParam()); }
  int policy_value() const { return std::get<3>(GetParam()); }

  const char* pref() const { return ConnectorPref(connector()); }
  const char* pref_value() const {
    switch (policy_value()) {
      case 1:
        return kNormalReportingSettingsPref;
      case 2:
        return kEmptySettingsPref;
    }
    NOTREACHED();
    return nullptr;
  }
  bool legacy_pref_value() const {
    switch (legacy_policy_value()) {
      case 1:
        return true;
      case 2:
        return false;
    }
    NOTREACHED();
    return false;
  }

  bool reporting_enabled() const {
    return (enable_feature_flag() &&
            (policy_value() == 1 ||
             (policy_value() == 0 && legacy_policy_value() == 1))) ||
           (!enable_feature_flag() && legacy_policy_value() == 1);
  }
};

TEST_P(ConnectorsManagerReportingFeatureTest, Test) {
  std::unique_ptr<ScopedConnectorPref> scoped_pref;
  if (policy_value() != 0)
    scoped_pref = std::make_unique<ScopedConnectorPref>(pref(), pref_value());

  if (legacy_policy_value() != 0) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
        prefs::kUnsafeEventsReportingEnabled, legacy_pref_value());
  } else {
    TestingBrowserProcess::GetGlobal()->local_state()->ClearPref(
        prefs::kUnsafeEventsReportingEnabled);
  }

  auto settings =
      ConnectorsManager::GetInstance()->GetReportingSettings(connector());
  EXPECT_EQ(reporting_enabled(), settings.has_value());
  if (settings.has_value())
    ValidateSettings(settings.value());

  EXPECT_EQ(enable_feature_flag() && policy_value() == 1,
            !ConnectorsManager::GetInstance()
                 ->GetReportingConnectorsSettingsForTesting()
                 .empty());
}

INSTANTIATE_TEST_CASE_P(
    ,
    ConnectorsManagerReportingFeatureTest,
    testing::Combine(testing::ValuesIn(kAllReportingConnectors),
                     testing::Bool(),
                     testing::ValuesIn({0, 1, 2}),
                     testing::ValuesIn({0, 1, 2})));

}  // namespace enterprise_connectors
