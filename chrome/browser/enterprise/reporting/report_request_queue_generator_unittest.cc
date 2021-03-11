// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_request_queue_generator.h"

#include <vector>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/enterprise/browser/reporting/report_request_definition.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

const char kIdleProfileName1[] = "idle_profile1";
const char kIdleProfileName2[] = "idle_profile2";
const char kActiveProfileName1[] = "active_profile1";
const char kActiveProfileName2[] = "active_profile2";

}  // namespace

// TODO(crbug.com/1103732): Get rid of chrome/browser dependencies and then
// move this file to components/enterprise/browser.
class ReportRequestQueueGeneratorTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<bool> {
 public:
  using ReportRequest = definition::ReportRequest;

  ReportRequestQueueGeneratorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        browser_report_generator_(&reporting_delegate_factory_),
        report_request_queue_generator_(&reporting_delegate_factory_) {
    TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
        scoped_feature_list_, GetParam());
  }

  ~ReportRequestQueueGeneratorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_manager_.CreateGuestProfile();
    profile_manager_.CreateSystemProfile();
    content::PluginService::GetInstance()->Init();
  }

  std::set<std::string> CreateIdleProfiles() {
    CreateIdleProfile(kIdleProfileName1);
    CreateIdleProfile(kIdleProfileName2);
    return std::set<std::string>{kIdleProfileName1, kIdleProfileName2};
  }

  std::set<std::string> CreateActiveProfiles() {
    CreateActiveProfile(kActiveProfileName1);
    CreateActiveProfile(kActiveProfileName2);
    return std::set<std::string>{kActiveProfileName1, kActiveProfileName2};
  }

  std::set<std::string> CreateActiveProfilesWithContent() {
    CreateActiveProfileWithContent(kActiveProfileName1);
    CreateActiveProfileWithContent(kActiveProfileName2);
    return std::set<std::string>{kActiveProfileName1, kActiveProfileName2};
  }

  void CreateIdleProfile(std::string profile_name) {
    profile_manager_.profile_attributes_storage()->AddProfile(
        profile_manager()->profiles_dir().AppendASCII(profile_name),
        base::ASCIIToUTF16(profile_name), std::string(), std::u16string(),
        false, 0, std::string(), EmptyAccountId());
  }

  TestingProfile* CreateActiveProfile(std::string profile_name) {
    return profile_manager_.CreateTestingProfile(profile_name);
  }

  TestingProfile* CreateActiveProfileWithPolicies(
      std::string profile_name,
      std::unique_ptr<policy::PolicyService> policy_service) {
    return profile_manager_.CreateTestingProfile(
        profile_name, {}, base::UTF8ToUTF16(profile_name), 0, {},
        TestingProfile::TestingFactories(), base::nullopt,
        std::move(policy_service));
  }

  void CreateActiveProfileWithContent(std::string profile_name) {
    TestingProfile* active_profile = CreateActiveProfile(profile_name);

    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(active_profile);
    std::string extension_name =
        "a super super super super super super super super super super super "
        "super super super super super super long extension name";
    extension_registry->AddEnabled(
        extensions::ExtensionBuilder(extension_name)
            .SetID("abcdefghijklmnoabcdefghijklmnoab")
            .Build());
  }

  std::unique_ptr<ReportRequest> GenerateBasicRequest() {
    auto request = std::make_unique<ReportRequest>();
    base::RunLoop run_loop;

    browser_report_generator_.Generate(
        ReportType::kFull,
        base::BindLambdaForTesting(
            [&run_loop, &request](std::unique_ptr<em::BrowserReport> report) {
              request->set_allocated_browser_report(report.release()),
                  run_loop.Quit();
            }));

    run_loop.Run();
    return request;
  }

  std::vector<std::unique_ptr<ReportRequest>> GenerateRequests(
      const ReportRequest& request) {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    std::queue<std::unique_ptr<ReportRequest>> requests =
        report_request_queue_generator_.Generate(ReportType::kFull, request);
    std::vector<std::unique_ptr<ReportRequest>> result;
    while (!requests.empty()) {
      result.push_back(std::move(requests.front()));
      requests.pop();
    }

    VerifyMetrics(result);
    return result;
  }

  void SetAndVerifyMaximumRequestSize(size_t size) {
    report_request_queue_generator_.SetMaximumReportSizeForTesting(size);
    EXPECT_EQ(size,
              report_request_queue_generator_.GetMaximumReportSizeForTesting());
  }

  void VerifyProfiles(const em::BrowserReport& report,
                      const std::set<std::string>& idle_profile_names,
                      const std::set<std::string>& active_profile_names) {
    EXPECT_EQ((size_t)report.chrome_user_profile_infos_size(),
              idle_profile_names.size() + active_profile_names.size());

    std::set<std::string> mutable_idle_profile_names(idle_profile_names);
    std::set<std::string> mutable_active_profile_names(active_profile_names);
    std::string profiles_dir = profile_manager_.profiles_dir().AsUTF8Unsafe();

    for (auto profile : report.chrome_user_profile_infos()) {
      // Verify the generated profile id, whose mapping rule varies in
      // different cases.
      // - Idle: <profiles_dir>/<profile_name>
      // - Active: <profiles_dir>/u-<profile_name>-hash
      EXPECT_EQ(0u, profile.id().find(profiles_dir));
      EXPECT_LE(0u, profile.id().find(profile.name()));

      if (profile.is_detail_available())
        FindAndRemove(mutable_active_profile_names, profile.name());
      else
        FindAndRemove(mutable_idle_profile_names, profile.name());
    }

    EXPECT_TRUE(mutable_idle_profile_names.empty());
    EXPECT_TRUE(mutable_active_profile_names.empty());
  }

  void VerifyMetrics(
      const std::vector<std::unique_ptr<ReportRequest>>& requests) {
    histogram_tester_->ExpectUniqueSample(
        "Enterprise.CloudReportingRequestCount", requests.size(), 1);
    histogram_tester_->ExpectUniqueSample(
        "Enterprise.CloudReportingBasicRequestSize",
        /*basic request size floor to KB*/ 0, 1);
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

  ReportRequestQueueGenerator* report_request_queue_generator() {
    return &report_request_queue_generator_;
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  void FindAndRemove(std::set<std::string>& names, const std::string& name) {
    auto it = names.find(name);
    EXPECT_NE(names.end(), it);
    names.erase(it);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  ReportingDelegateFactoryDesktop reporting_delegate_factory_;
  BrowserReportGenerator browser_report_generator_;
  ReportRequestQueueGenerator report_request_queue_generator_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ReportRequestQueueGeneratorTest);
};

TEST_P(ReportRequestQueueGeneratorTest, GenerateReport) {
  auto idle_profile_names = CreateIdleProfiles();
  auto basic_request = GenerateBasicRequest();
  auto requests = GenerateRequests(*basic_request);
  EXPECT_EQ(1u, requests.size());

  VerifyProfiles(requests[0]->browser_report(), idle_profile_names, {});
  histogram_tester()->ExpectBucketCount("Enterprise.CloudReportingRequestSize",
                                        /*report size floor to KB*/ 0, 1);
}

TEST_P(ReportRequestQueueGeneratorTest, GenerateActiveProfiles) {
  auto idle_profile_names = CreateIdleProfiles();
  auto active_profile_names = CreateActiveProfiles();
  auto basic_request = GenerateBasicRequest();
  auto requests = GenerateRequests(*basic_request);
  EXPECT_EQ(1u, requests.size());

  VerifyProfiles(requests[0]->browser_report(), idle_profile_names,
                 active_profile_names);
  histogram_tester()->ExpectBucketCount("Enterprise.CloudReportingRequestSize",
                                        /*report size floor to KB*/ 0, 1);
}

TEST_P(ReportRequestQueueGeneratorTest, BasicReportIsTooBig) {
  // Set a super small limitation.
  SetAndVerifyMaximumRequestSize(5);

  // Because the limitation is so small, no request can be created.
  CreateIdleProfiles();
  auto basic_request = GenerateBasicRequest();
  auto requests = GenerateRequests(*basic_request);
  EXPECT_EQ(0u, requests.size());

  histogram_tester()->ExpectTotalCount("Enterprise.CloudReportingRequestSize",
                                       0);
}

TEST_P(ReportRequestQueueGeneratorTest, ReportSeparation) {
  auto active_profiles = CreateActiveProfilesWithContent();
  auto basic_request = GenerateBasicRequest();
  auto requests = GenerateRequests(*basic_request);
  EXPECT_EQ(1u, requests.size());

  // Set the limitation just below the size of the report so that it needs to be
  // separated into two requests later.
  SetAndVerifyMaximumRequestSize(requests[0]->ByteSizeLong() - 30);
  requests = GenerateRequests(*basic_request);
  EXPECT_EQ(2u, requests.size());

  // The profile order in requests should match the return value of
  // GetAllProfilesAttributes().
  std::vector<std::string> expected_active_profiles_in_requests;
  for (const auto* entry : profile_manager()
                               ->profile_attributes_storage()
                               ->GetAllProfilesAttributes()) {
    std::string profile_name = base::UTF16ToUTF8(entry->GetName());
    if (active_profiles.find(profile_name) != active_profiles.end())
      expected_active_profiles_in_requests.push_back(profile_name);
  }

  // The first profile is activated in the first request only while the second
  // profile is activated in the second request.
  VerifyProfiles(
      requests[0]->browser_report(),
      {/* idle_profile_names */ expected_active_profiles_in_requests[1]},
      {/* active_profile_names */ expected_active_profiles_in_requests[0]});
  VerifyProfiles(
      requests[1]->browser_report(),
      {/* idle_profile_names */ expected_active_profiles_in_requests[0]},
      {/* active_profile_names */ expected_active_profiles_in_requests[1]});
  histogram_tester()->ExpectBucketCount("Enterprise.CloudReportingRequestSize",
                                        /*report size floor to KB*/ 0, 2);
}

TEST_P(ReportRequestQueueGeneratorTest, ProfileReportIsTooBig) {
  CreateActiveProfileWithContent(kActiveProfileName1);
  auto basic_request = GenerateBasicRequest();
  auto requests = GenerateRequests(*basic_request);
  EXPECT_EQ(1u, requests.size());

  // Set the limitation just below the size of the report.
  SetAndVerifyMaximumRequestSize(requests[0]->ByteSizeLong() - 30);

  // Add a smaller Profile.
  CreateActiveProfile(kActiveProfileName2);
  basic_request = GenerateBasicRequest();
  requests = GenerateRequests(*basic_request);
  EXPECT_EQ(1u, requests.size());

  // Only the second Profile is activated while the first one is too big to be
  // reported.
  VerifyProfiles(requests[0]->browser_report(), {kActiveProfileName1},
                 {kActiveProfileName2});
  histogram_tester()->ExpectBucketCount("Enterprise.CloudReportingRequestSize",
                                        /*report size floor to KB*/ 0, 2);
}

TEST_P(ReportRequestQueueGeneratorTest, ChromePoliciesCollection) {
  auto policy_service = std::make_unique<policy::MockPolicyService>();
  policy::PolicyMap policy_map;

  ON_CALL(*policy_service.get(),
          GetPolicies(::testing::Eq(policy::PolicyNamespace(
              policy::POLICY_DOMAIN_CHROME, std::string()))))
      .WillByDefault(::testing::ReturnRef(policy_map));

  policy_map.Set("kPolicyName1", policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                 base::Value(std::vector<base::Value>()), nullptr);
  policy_map.Set("kPolicyName2", policy::POLICY_LEVEL_RECOMMENDED,
                 policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_MERGED,
                 base::Value(true), nullptr);

  CreateActiveProfileWithPolicies(kActiveProfileName1,
                                  std::move(policy_service));

  auto basic_request = GenerateBasicRequest();
  auto requests = GenerateRequests(*basic_request);
  EXPECT_EQ(1u, requests.size());

  auto browser_report = requests[0]->browser_report();
  EXPECT_EQ(1, browser_report.chrome_user_profile_infos_size());

  auto profile_info = browser_report.chrome_user_profile_infos(0);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // In Chrome OS, the collection of policies is disabled.
  EXPECT_EQ(0, profile_info.chrome_policies_size());
#else
  // In desktop Chrome, the collection of policies is enabled.
  EXPECT_EQ(2, profile_info.chrome_policies_size());
#endif
}

INSTANTIATE_TEST_SUITE_P(AllGuestTypes,
                         ReportRequestQueueGeneratorTest,
                         /*is_ephemeral=*/testing::Bool());

}  // namespace enterprise_reporting
