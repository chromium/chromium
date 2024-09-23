// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/crowd_deny_preload_data.h"

#include <string_view>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chrome/browser/permissions/crowd_deny.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kTestDomainAlpha[] = "alpha.com";
constexpr char kTestDomainBeta[] = "beta.com";
constexpr char kTestDomainGamma[] = "gamma.com";
constexpr char kTestDomainDelta[] = "delta.com";
constexpr char kTestSubdomainOfDelta1[] = "one.delta.com";
constexpr char kTestDomainEpsilon[] = "epsilon.com";

constexpr char kTestOriginAlpha[] = "https://alpha.com";
constexpr char kTestOriginSubdomainOfAlpha[] = "https://foo.alpha.com";
constexpr char kTestOriginBeta[] = "https://beta.com";
constexpr char kTestOriginGamma[] = "https://gamma.com";
constexpr char kTestOriginDelta[] = "https://delta.com";
constexpr char kTestOriginSubdomainOfDelta1[] = "https://one.delta.com";
constexpr char kTestOriginSubdomainOfDelta2[] = "https://foo.two.delta.com";
constexpr char kTestOriginNotSubdomainOfDelta[] = "https://notdelta.com";
constexpr char kTestOriginEpsilon[] = "https://epsilon.com";
constexpr char kTestOriginZeta[] = "https://zeta.com";

constexpr const char* kAllTestingOrigins[] = {
    kTestOriginAlpha, kTestOriginBeta, kTestOriginGamma, kTestOriginDelta,
    kTestOriginEpsilon};

}  // namespace

class CrowdDenyPreloadDataTest : public testing::Test {
 public:
  using SiteReputation = chrome_browser_crowd_deny::SiteReputation;

  CrowdDenyPreloadDataTest() {}

  CrowdDenyPreloadDataTest(const CrowdDenyPreloadDataTest&) = delete;
  CrowdDenyPreloadDataTest& operator=(const CrowdDenyPreloadDataTest&) = delete;

  ~CrowdDenyPreloadDataTest() override = default;

 protected:
  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }
  CrowdDenyPreloadData* preload_data() { return &preload_data_; }

  base::FilePath GetPathInTempDir(
      base::FilePath::StringPieceType filename) const {
    return scoped_temp_dir_.GetPath().Append(filename);
  }

  void SerializeTestRawData(std::string_view raw_data, base::FilePath path) {
    ASSERT_TRUE(base::WriteFile(path, raw_data));
  }

  void SerializeTestPreloadData(
      chrome_browser_crowd_deny::PreloadData preload_data,
      base::FilePath path) {
    std::string binary_preload_data;
    ASSERT_TRUE(preload_data.SerializeToString(&binary_preload_data));
    ASSERT_NO_FATAL_FAILURE(SerializeTestRawData(binary_preload_data, path));
  }

  void LoadTestDataAndWait(base::FilePath path) {
    preload_data()->LoadFromDisk(path, base::Version());
    task_environment()->RunUntilIdle();
  }

  void SerializeAndLoadTestData(
      chrome_browser_crowd_deny::PreloadData test_data) {
    const base::FilePath temp_path =
        GetPathInTempDir(FILE_PATH_LITERAL("Preload Data"));
    ASSERT_NO_FATAL_FAILURE(
        SerializeTestPreloadData(std::move(test_data), temp_path));
    LoadTestDataAndWait(temp_path);
  }

  void SerializeAndLoadCannedTestData() {
    chrome_browser_crowd_deny::PreloadData test_data;

    auto* alpha_site_reputation = test_data.add_site_reputations();
    alpha_site_reputation->set_domain(kTestDomainAlpha);
    alpha_site_reputation->set_notification_ux_quality(SiteReputation::UNKNOWN);

    auto* beta_site_reputation = test_data.add_site_reputations();
    beta_site_reputation->set_domain(kTestDomainBeta);
    beta_site_reputation->set_include_subdomains(false);
    beta_site_reputation->set_notification_ux_quality(
        SiteReputation::ACCEPTABLE);

    auto* gamma_site_reputation = test_data.add_site_reputations();
    gamma_site_reputation->set_domain(kTestDomainGamma);
    gamma_site_reputation->set_warning_only(false);
    gamma_site_reputation->set_notification_ux_quality(
        SiteReputation::UNSOLICITED_PROMPTS);

    auto* delta_site_reputation = test_data.add_site_reputations();
    delta_site_reputation->set_domain(kTestDomainDelta);
    delta_site_reputation->set_include_subdomains(true);
    delta_site_reputation->set_warning_only(true);
    delta_site_reputation->set_notification_ux_quality(
        SiteReputation::ABUSIVE_PROMPTS);

    auto* epsilon_site_reputation = test_data.add_site_reputations();
    epsilon_site_reputation->set_domain(kTestDomainEpsilon);
    // No |notification_ux_quality| field.
    // No |include_subdomains| field.
    // No |warning_only| field.

    ASSERT_NO_FATAL_FAILURE(SerializeAndLoadTestData(std::move(test_data)));
  }

  void ExpectEmptyPreloadData() {
    for (const char* origin_string : kAllTestingOrigins) {
      SCOPED_TRACE(origin_string);
      EXPECT_FALSE(preload_data()->GetReputationDataForSite(
          url::Origin::Create(GURL(origin_string))));
    }
  }

  const SiteReputation* GetReputationDataForSite(const url::Origin& origin) {
    return preload_data()->GetReputationDataForSite(origin);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
  CrowdDenyPreloadData preload_data_;
};

TEST_F(CrowdDenyPreloadDataTest, NoData) {
  ExpectEmptyPreloadData();
}

TEST_F(CrowdDenyPreloadDataTest, MissingFile) {
  LoadTestDataAndWait(GetPathInTempDir(FILE_PATH_LITERAL("NonExistentFile")));
  ExpectEmptyPreloadData();
}

TEST_F(CrowdDenyPreloadDataTest, EmptyData) {
  const base::FilePath empty_file_path =
      GetPathInTempDir(FILE_PATH_LITERAL("EmptyFile"));
  SerializeTestRawData(std::string_view(), empty_file_path);
  LoadTestDataAndWait(empty_file_path);
  ExpectEmptyPreloadData();
}

TEST_F(CrowdDenyPreloadDataTest, BadData) {
  const base::FilePath bad_data_path =
      GetPathInTempDir(FILE_PATH_LITERAL("BadFile"));
  SerializeTestRawData("This is not a proto.", bad_data_path);
  LoadTestDataAndWait(bad_data_path);
  ExpectEmptyPreloadData();
}

TEST_F(CrowdDenyPreloadDataTest, DataIntegrityAndDefaults) {
  ASSERT_NO_FATAL_FAILURE(SerializeAndLoadCannedTestData());

  const auto* data =
      GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginAlpha)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainAlpha, data->domain());
  EXPECT_EQ(SiteReputation::UNKNOWN, data->notification_ux_quality());
  EXPECT_FALSE(data->include_subdomains());
  EXPECT_FALSE(data->warning_only());

  data = GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginBeta)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainBeta, data->domain());
  EXPECT_EQ(SiteReputation::ACCEPTABLE, data->notification_ux_quality());
  EXPECT_FALSE(data->include_subdomains());
  EXPECT_FALSE(data->warning_only());

  data = GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginGamma)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainGamma, data->domain());
  EXPECT_EQ(SiteReputation::UNSOLICITED_PROMPTS,
            data->notification_ux_quality());
  EXPECT_FALSE(data->include_subdomains());
  EXPECT_FALSE(data->warning_only());

  data = GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginDelta)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainDelta, data->domain());
  EXPECT_EQ(SiteReputation::ABUSIVE_PROMPTS, data->notification_ux_quality());
  EXPECT_TRUE(data->include_subdomains());
  EXPECT_TRUE(data->warning_only());

  data =
      GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginEpsilon)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainEpsilon, data->domain());
  EXPECT_EQ(SiteReputation::UNKNOWN, data->notification_ux_quality());
  EXPECT_FALSE(data->include_subdomains());
  EXPECT_FALSE(data->warning_only());

  data = GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginZeta)));
  EXPECT_FALSE(data);
}

TEST_F(CrowdDenyPreloadDataTest, GetReputationReturnsNullForNonHttpsOrigins) {
  const char* kNonHttpsOrigins[] = {
      "http://alpha.com",
      "wss://alpha.com",
      "ftp://alpha.com",
      "file:///alpha.com",
  };

  ASSERT_NO_FATAL_FAILURE(SerializeAndLoadCannedTestData());
  EXPECT_TRUE(
      GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginAlpha))));

  for (const char* non_https_origin : kNonHttpsOrigins) {
    SCOPED_TRACE(non_https_origin);
    EXPECT_FALSE(
        GetReputationDataForSite(url::Origin::Create(GURL(non_https_origin))));
  }
}

TEST_F(CrowdDenyPreloadDataTest, GetReputationIgnoresPort) {
  ASSERT_NO_FATAL_FAILURE(SerializeAndLoadCannedTestData());

  EXPECT_TRUE(GetReputationDataForSite(
      url::Origin::Create(GURL("https://alpha.com:443"))));
  EXPECT_TRUE(GetReputationDataForSite(
      url::Origin::Create(GURL("https://alpha.com:1234"))));
}

TEST_F(CrowdDenyPreloadDataTest, GetReputationWithSubdomainMatching) {
  ASSERT_NO_FATAL_FAILURE(SerializeAndLoadCannedTestData());

  const auto* data =
      GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginDelta)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainDelta, data->domain());
  ASSERT_TRUE(data->include_subdomains());

  data = GetReputationDataForSite(
      url::Origin::Create(GURL(kTestOriginSubdomainOfDelta1)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainDelta, data->domain());

  data = GetReputationDataForSite(
      url::Origin::Create(GURL(kTestOriginSubdomainOfDelta2)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainDelta, data->domain());

  data = GetReputationDataForSite(
      url::Origin::Create(GURL(kTestOriginNotSubdomainOfDelta)));
  EXPECT_FALSE(data);

  data = GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginAlpha)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainAlpha, data->domain());
  ASSERT_FALSE(data->include_subdomains());

  // Should not return `alpha.com` because |include_subdomains| is not set.
  data = GetReputationDataForSite(
      url::Origin::Create(GURL(kTestOriginSubdomainOfAlpha)));
  EXPECT_FALSE(data);
}

TEST_F(CrowdDenyPreloadDataTest, SubdomainSpecificOverride) {
  chrome_browser_crowd_deny::PreloadData test_data;

  auto* delta_site_reputation = test_data.add_site_reputations();
  delta_site_reputation->set_domain(kTestDomainDelta);
  delta_site_reputation->set_include_subdomains(true);
  delta_site_reputation->set_notification_ux_quality(
      SiteReputation::UNSOLICITED_PROMPTS);

  auto* subdomain_site_reputation = test_data.add_site_reputations();
  subdomain_site_reputation->set_domain(kTestSubdomainOfDelta1);
  subdomain_site_reputation->set_include_subdomains(true);
  subdomain_site_reputation->set_notification_ux_quality(
      SiteReputation::ACCEPTABLE);

  ASSERT_NO_FATAL_FAILURE(SerializeAndLoadTestData(std::move(test_data)));

  const auto* data = GetReputationDataForSite(
      url::Origin::Create(GURL(kTestOriginSubdomainOfDelta1)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestSubdomainOfDelta1, data->domain());
  EXPECT_EQ(SiteReputation::ACCEPTABLE, data->notification_ux_quality());

  data = GetReputationDataForSite(
      url::Origin::Create(GURL(kTestOriginSubdomainOfDelta2)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainDelta, data->domain());
  EXPECT_EQ(SiteReputation::UNSOLICITED_PROMPTS,
            data->notification_ux_quality());
}

TEST_F(CrowdDenyPreloadDataTest, Update) {
  ASSERT_NO_FATAL_FAILURE(SerializeAndLoadCannedTestData());

  // Prepare and load an updated payload version, which updates the entry for
  // `delta.com`, adds an entry for `epsilon.com`, and removes all others.
  chrome_browser_crowd_deny::PreloadData test_data_v2;

  auto* delta_site_reputation = test_data_v2.add_site_reputations();
  delta_site_reputation->set_domain(kTestDomainDelta);
  delta_site_reputation->set_notification_ux_quality(
      SiteReputation::UNSOLICITED_PROMPTS);

  auto* epsilon_site_reputation = test_data_v2.add_site_reputations();
  epsilon_site_reputation->set_domain(kTestDomainEpsilon);
  epsilon_site_reputation->set_notification_ux_quality(
      SiteReputation::ACCEPTABLE);

  ASSERT_NO_FATAL_FAILURE(SerializeAndLoadTestData(std::move(test_data_v2)));

  // Check that the updated preload data is visible.
  EXPECT_FALSE(
      GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginAlpha))));
  EXPECT_FALSE(
      GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginBeta))));
  EXPECT_FALSE(
      GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginGamma))));

  const auto* data =
      GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginDelta)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainDelta, data->domain());
  EXPECT_EQ(SiteReputation::UNSOLICITED_PROMPTS,
            data->notification_ux_quality());

  data =
      GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginEpsilon)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainEpsilon, data->domain());
  EXPECT_EQ(SiteReputation::ACCEPTABLE, data->notification_ux_quality());
}

TEST_F(CrowdDenyPreloadDataTest, UpdateToMissingFileWipesInMemoryState) {
  ASSERT_NO_FATAL_FAILURE(SerializeAndLoadCannedTestData());

  LoadTestDataAndWait(GetPathInTempDir(FILE_PATH_LITERAL("NonExistentFile")));
  ExpectEmptyPreloadData();
}

TEST_F(CrowdDenyPreloadDataTest, UpdateToEmptyFileWipesInMemoryState) {
  ASSERT_NO_FATAL_FAILURE(SerializeAndLoadCannedTestData());

  const base::FilePath empty_file_path =
      GetPathInTempDir(FILE_PATH_LITERAL("EmptyFile"));
  SerializeTestRawData(std::string_view(), empty_file_path);
  LoadTestDataAndWait(empty_file_path);
  ExpectEmptyPreloadData();
}

TEST_F(CrowdDenyPreloadDataTest, UpdateToBadFileWipesInMemoryState) {
  ASSERT_NO_FATAL_FAILURE(SerializeAndLoadCannedTestData());

  const base::FilePath bad_data_path =
      GetPathInTempDir(FILE_PATH_LITERAL("BadFile"));
  SerializeTestRawData("This is not a proto.", bad_data_path);
  LoadTestDataAndWait(bad_data_path);
  ExpectEmptyPreloadData();
}

// During start-up congestion, it is possible that a new version of the
// component becomes available while the old version is pending being loaded.
// Ensure that when things settle down, the last version loaded will prevail,
// and nothing explodes on the way.
TEST_F(CrowdDenyPreloadDataTest, LastOneSurvivesFromUpdatesInQuickSuccession) {
  ASSERT_NO_FATAL_FAILURE(SerializeAndLoadCannedTestData());

  // Prepare and load two updated versions, each twice, in a quick success.
  chrome_browser_crowd_deny::PreloadData test_data_v2;
  auto* delta_site_reputation = test_data_v2.add_site_reputations();
  delta_site_reputation->set_domain(kTestDomainDelta);

  chrome_browser_crowd_deny::PreloadData test_data_v3;
  auto* epsilon_site_reputation = test_data_v3.add_site_reputations();
  epsilon_site_reputation->set_domain(kTestDomainEpsilon);
  epsilon_site_reputation->set_notification_ux_quality(
      SiteReputation::ACCEPTABLE);

  const base::FilePath data_path_v2 =
      GetPathInTempDir(FILE_PATH_LITERAL("DataV2"));
  const base::FilePath data_path_v3 =
      GetPathInTempDir(FILE_PATH_LITERAL("DataV3"));
  SerializeTestPreloadData(std::move(test_data_v2), data_path_v2);
  SerializeTestPreloadData(std::move(test_data_v3), data_path_v3);

  // Trigger three loads without pumping the message loop.
  //
  // TODO(crbug.com/40109238): Think about making this test stronger. Even if
  // the ordering were random, given the generous retry policy in continuous
  // build, the test would still pass most of the time.
  preload_data()->LoadFromDisk(data_path_v2, base::Version());
  preload_data()->LoadFromDisk(data_path_v3, base::Version());
  task_environment()->RunUntilIdle();

  // Expect the new version to have become visible.
  const auto* data =
      GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginEpsilon)));
  ASSERT_TRUE(data);
  EXPECT_EQ(kTestDomainEpsilon, data->domain());
  EXPECT_EQ(SiteReputation::ACCEPTABLE, data->notification_ux_quality());

  EXPECT_FALSE(
      GetReputationDataForSite(url::Origin::Create(GURL(kTestOriginDelta))));
}
