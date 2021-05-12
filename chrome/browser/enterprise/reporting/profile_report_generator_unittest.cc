// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/profile_report_generator.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/util/values/values_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

const int kMaxNumberOfExtensionRequest = 1000;

constexpr char kProfile[] = "Profile";
constexpr char kIdleProfile[] = "IdleProfile";
constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId2[] = "abcdefghijklmnopabcdefghijklmnpo";
constexpr int kFakeTime = 123456;

constexpr char kAllowedExtensionSettings[] = R"({
  "abcdefghijklmnopabcdefghijklmnop" : {
    "installation_mode": "allowed"
  }
})";

constexpr char kBlockedExtensionSettings[] = R"({
  "abcdefghijklmnopabcdefghijklmnop" : {
    "installation_mode": "blocked"
  }
})";

}  // namespace

class ProfileReportGeneratorTest : public ::testing::Test {
 public:
  ProfileReportGeneratorTest()
      : generator_(&reporting_delegate_factory_),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ProfileReportGeneratorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    InitMockPolicyService();
    InitPolicyMap();

    profile_ = profile_manager_.CreateTestingProfile(
        kProfile, {}, base::UTF8ToUTF16(kProfile), 0, {},
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories(),
        base::nullopt, std::move(policy_service_));
  }

  void InitMockPolicyService() {
    policy_service_ = std::make_unique<policy::MockPolicyService>();

    ON_CALL(*policy_service_.get(),
            GetPolicies(::testing::Eq(policy::PolicyNamespace(
                policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));
  }

  void InitPolicyMap() {
    policy_map_.Set("kPolicyName1", policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    base::Value(std::vector<base::Value>()), nullptr);
    policy_map_.Set("kPolicyName2", policy::POLICY_LEVEL_RECOMMENDED,
                    policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_MERGED,
                    base::Value(true), nullptr);
  }

  std::unique_ptr<em::ChromeUserProfileInfo> GenerateReport(
      const base::FilePath& path,
      const std::string& name) {
    std::unique_ptr<em::ChromeUserProfileInfo> report =
        generator_.MaybeGenerate(path, name, ReportType::kFull);
    return report;
  }

  std::unique_ptr<em::ChromeUserProfileInfo> GenerateReport() {
    auto report =
        GenerateReport(profile()->GetPath(), profile()->GetProfileUserName());
    EXPECT_TRUE(report);
    EXPECT_EQ(profile()->GetProfileUserName(), report->name());
    EXPECT_EQ(profile()->GetPath().AsUTF8Unsafe(), report->id());
    EXPECT_TRUE(report->is_detail_available());

    return report;
  }

  void SetExtensionToPendingList(const std::vector<std::string>& ids) {
    std::unique_ptr<base::Value> id_values =
        std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
    for (const auto& id : ids) {
      base::Value request_data(base::Value::Type::DICTIONARY);
      request_data.SetKey(
          extension_misc::kExtensionRequestTimestamp,
          ::util::TimeToValue(base::Time::FromJavaTime(kFakeTime)));
      id_values->SetKey(id, std::move(request_data));
    }
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kCloudExtensionRequestIds, std::move(id_values));
  }

  void SetExtensionSettings(const std::string& settings_string) {
    base::Optional<base::Value> settings =
        base::JSONReader::Read(settings_string);
    ASSERT_TRUE(settings.has_value());
    profile()->GetTestingPrefService()->SetManagedPref(
        extensions::pref_names::kExtensionManagement,
        base::Value::ToUniquePtrValue(std::move(*settings)));
  }

  TestingProfile* profile() { return profile_; }
  TestingProfileManager* profile_manager() { return &profile_manager_; }

  ReportingDelegateFactoryDesktop reporting_delegate_factory_;
  ProfileReportGenerator generator_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;

  std::unique_ptr<policy::MockPolicyService> policy_service_;
  policy::PolicyMap policy_map_;

  DISALLOW_COPY_AND_ASSIGN(ProfileReportGeneratorTest);
};

TEST_F(ProfileReportGeneratorTest, ProfileNotActivated) {
  const base::FilePath profile_path =
      profile_manager()->profiles_dir().AppendASCII(kIdleProfile);
  profile_manager()->profile_attributes_storage()->AddProfile(
      profile_path, base::ASCIIToUTF16(kIdleProfile), std::string(),
      std::u16string(), false, 0, std::string(), EmptyAccountId());
  std::unique_ptr<em::ChromeUserProfileInfo> response =
      generator_.MaybeGenerate(profile_path, kIdleProfile, ReportType::kFull);
  ASSERT_FALSE(response.get());
}

TEST_F(ProfileReportGeneratorTest, UnsignedInProfile) {
  auto report = GenerateReport();
  EXPECT_FALSE(report->has_chrome_signed_in_user());
}

TEST_F(ProfileReportGeneratorTest, SignedInProfile) {
  IdentityTestEnvironmentProfileAdaptor identity_test_env_adaptor(profile());
  auto expected_info =
      identity_test_env_adaptor.identity_test_env()->SetPrimaryAccount(
          "test@mail.com");
  auto report = GenerateReport();
  EXPECT_TRUE(report->has_chrome_signed_in_user());
  EXPECT_EQ(expected_info.email, report->chrome_signed_in_user().email());
  EXPECT_EQ(expected_info.gaia,
            report->chrome_signed_in_user().obfuscated_gaia_id());
}

TEST_F(ProfileReportGeneratorTest, PoliciesDisabled) {
  // Users' profile info is collected by default.
  std::unique_ptr<em::ChromeUserProfileInfo> report = GenerateReport();
  EXPECT_EQ(2, report->chrome_policies_size());

  // Stop to collect profile info after |set_policies_enabled| is set as false.
  generator_.set_policies_enabled(false);
  report = GenerateReport();
  EXPECT_EQ(0, report->chrome_policies_size());

  // Start to collect profile info again after |set_policies_enabled| is set as
  // true.
  generator_.set_policies_enabled(true);
  report = GenerateReport();
  EXPECT_EQ(2, report->chrome_policies_size());
}

TEST_F(ProfileReportGeneratorTest, PendingRequest) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kCloudExtensionRequestEnabled,
      std::make_unique<base::Value>(true));
  std::vector<std::string> ids = {kExtensionId};
  SetExtensionToPendingList(ids);

  auto report = GenerateReport();
  ASSERT_EQ(1, report->extension_requests_size());
  EXPECT_EQ(kExtensionId, report->extension_requests(0).id());
  EXPECT_EQ(kFakeTime, report->extension_requests(0).request_timestamp());
}

TEST_F(ProfileReportGeneratorTest, NoPendingRequestWhenItsDisabled) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kCloudExtensionRequestEnabled,
      std::make_unique<base::Value>(false));
  std::vector<std::string> ids = {kExtensionId};
  SetExtensionToPendingList(ids);

  auto report = GenerateReport();
  EXPECT_EQ(0, report->extension_requests_size());
}

TEST_F(ProfileReportGeneratorTest, FilterOutApprovedPendingRequest) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kCloudExtensionRequestEnabled,
      std::make_unique<base::Value>(true));
  SetExtensionSettings(kAllowedExtensionSettings);
  std::vector<std::string> ids = {kExtensionId, kExtensionId2};
  SetExtensionToPendingList(ids);

  auto report = GenerateReport();
  ASSERT_EQ(1, report->extension_requests_size());
  EXPECT_EQ(kExtensionId2, report->extension_requests(0).id());
}

TEST_F(ProfileReportGeneratorTest, FilterOutBlockedPendingRequest) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kCloudExtensionRequestEnabled,
      std::make_unique<base::Value>(true));
  SetExtensionSettings(kBlockedExtensionSettings);
  std::vector<std::string> ids = {kExtensionId, kExtensionId2};
  SetExtensionToPendingList(ids);

  auto report = GenerateReport();
  ASSERT_EQ(1, report->extension_requests_size());
  EXPECT_EQ(kExtensionId2, report->extension_requests(0).id());
}

TEST_F(ProfileReportGeneratorTest, TooManyRequests) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kCloudExtensionRequestEnabled,
      std::make_unique<base::Value>(true));
  const int extension_request_count = kMaxNumberOfExtensionRequest;
  std::vector<std::string> ids(extension_request_count);
  for (int extension_id = 0; extension_id < extension_request_count;
       extension_id += 1) {
    ids[extension_id] = base::NumberToString(extension_id);
  }
  SetExtensionToPendingList(ids);

  auto report = GenerateReport();
  // At most 1000 requests will be uploaded.
  EXPECT_EQ(kMaxNumberOfExtensionRequest, report->extension_requests_size());

  // And the filter is stable.
  auto report2 = GenerateReport();
  for (int id = 0; id < kMaxNumberOfExtensionRequest; id += 1)
    EXPECT_EQ(report->extension_requests(id).id(),
              report2->extension_requests(id).id());
}

TEST_F(ProfileReportGeneratorTest, ExtensionRequestOnlyReport) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kCloudExtensionRequestEnabled,
      std::make_unique<base::Value>(true));
  std::vector<std::string> ids = {kExtensionId};
  SetExtensionToPendingList(ids);

  IdentityTestEnvironmentProfileAdaptor identity_test_env_adaptor(profile());
  auto expected_info =
      identity_test_env_adaptor.identity_test_env()->SetPrimaryAccount(
          "test@mail.com");

  auto report = generator_.MaybeGenerate(profile()->GetPath(),
                                         profile()->GetProfileUserName(),
                                         ReportType::kExtensionRequest);

  // Extension request and profile id are included. Profile name and sign in
  // users info are included on CrOS only.
  EXPECT_TRUE(report);
  EXPECT_EQ(profile()->GetPath().AsUTF8Unsafe(), report->id());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(profile()->GetProfileUserName(), report->name());
  EXPECT_TRUE(report->has_chrome_signed_in_user());
#else
  EXPECT_FALSE(report->has_name());
  EXPECT_FALSE(report->has_chrome_signed_in_user());
#endif
  ASSERT_EQ(1, report->extension_requests_size());
  EXPECT_EQ(kExtensionId, report->extension_requests(0).id());
  EXPECT_EQ(kFakeTime, report->extension_requests(0).request_timestamp());

  // Policies and extensions info should not be added.
  EXPECT_EQ(0, report->chrome_policies_size());
  EXPECT_EQ(0, report->extensions_size());
  EXPECT_EQ(0, report->policy_fetched_timestamps_size());
  EXPECT_TRUE(report->is_detail_available());
}

TEST_F(ProfileReportGeneratorTest, ExtensionRequestOnlyReportWithoutPolicy) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kCloudExtensionRequestEnabled,
      std::make_unique<base::Value>(false));
  IdentityTestEnvironmentProfileAdaptor identity_test_env_adaptor(profile());
  auto expected_info =
      identity_test_env_adaptor.identity_test_env()->SetPrimaryAccount(
          "test@mail.com");

  auto report = generator_.MaybeGenerate(profile()->GetPath(),
                                         profile()->GetProfileUserName(),
                                         ReportType::kExtensionRequest);
  EXPECT_TRUE(report);
  EXPECT_EQ(0, report->extension_requests_size());
}

TEST_F(ProfileReportGeneratorTest,
       ExtensionRequestOnlyReportWithoutAnyRequest) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kCloudExtensionRequestEnabled,
      std::make_unique<base::Value>(true));
  std::vector<std::string> ids;
  SetExtensionToPendingList(ids);

  IdentityTestEnvironmentProfileAdaptor identity_test_env_adaptor(profile());
  auto expected_info =
      identity_test_env_adaptor.identity_test_env()->SetPrimaryAccount(
          "test@mail.com");

  auto report = generator_.MaybeGenerate(profile()->GetPath(),
                                         profile()->GetProfileUserName(),
                                         ReportType::kExtensionRequest);

  EXPECT_TRUE(report);
  EXPECT_EQ(0, report->extension_requests_size());
}

}  // namespace enterprise_reporting
