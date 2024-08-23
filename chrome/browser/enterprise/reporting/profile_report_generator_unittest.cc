// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/profile_report_generator.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"
#else
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/pref_names.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#endif

using ::testing::NiceMock;

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

constexpr char kProfile[] = "Default";
constexpr char16_t kProfile16[] = u"Profile";
constexpr char kIdleProfile[] = "IdleProfile";
constexpr char16_t kIdleProfile16[] = u"IdleProfile";

#if !BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kAffiliationId1[] = "affiliation-id-1";
constexpr char kAffiliationId2[] = "affiliation-id-2";
#endif


#if !BUILDFLAG(IS_ANDROID)
const int kMaxNumberOfExtensionRequest = 1000;
constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId2[] = "abcdefghijklmnopabcdefghijklmnpo";
constexpr int kFakeTime = 123456;
constexpr char kJustification[] = "I really need to change my boring cursor.";

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
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

#if BUILDFLAG(IS_ANDROID)
typedef ReportingDelegateFactoryAndroid PlatformReportingDelegateFactory;
#else
typedef ReportingDelegateFactoryDesktop PlatformReportingDelegateFactory;
#endif  // BUILDFLAG(IS_ANDROID)

class ProfileReportGeneratorTest : public ::testing::Test {
 public:
  ProfileReportGeneratorTest()
      : generator_(&reporting_delegate_factory_),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ProfileReportGeneratorTest(const ProfileReportGeneratorTest&) = delete;
  ProfileReportGeneratorTest& operator=(const ProfileReportGeneratorTest&) =
      delete;
  ~ProfileReportGeneratorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    InitMockPolicyService();
    InitPolicyMap();

    profile_ = profile_manager_.CreateTestingProfile(
        kProfile, {}, kProfile16, 0,
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories(),
        /*is_supervised_profile=*/false, std::nullopt,
        std::move(policy_service_));
  }

  void InitMockPolicyService() {
    policy_service_ = std::make_unique<NiceMock<policy::MockPolicyService>>();

    ON_CALL(*policy_service_.get(),
            GetPolicies(::testing::Eq(policy::PolicyNamespace(
                policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));
  }

  void InitPolicyMap() {
    policy_map_.Set("kPolicyName1", policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                    base::Value(true), nullptr);
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void SetExtensionToPendingList(const std::vector<std::string>& ids) {
    base::Value::Dict id_values;
    for (const auto& id : ids) {
      id_values.Set(
          id,
          base::Value::Dict()
              .Set(extension_misc::kExtensionRequestTimestamp,
                   ::base::TimeToValue(
                       base::Time::FromMillisecondsSinceUnixEpoch(kFakeTime)))
              .Set(extension_misc::kExtensionWorkflowJustification,
                   base::Value(kJustification)));
    }
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kCloudExtensionRequestIds, std::move(id_values));
  }

  void SetExtensionSettings(const std::string& settings_string) {
    std::optional<base::Value> settings =
        base::JSONReader::Read(settings_string);
    ASSERT_TRUE(settings.has_value());
    profile()->GetTestingPrefService()->SetManagedPref(
        extensions::pref_names::kExtensionManagement,
        base::Value::ToUniquePtrValue(std::move(*settings)));
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  TestingProfile* profile() { return profile_; }
  TestingProfileManager* profile_manager() { return &profile_manager_; }

  PlatformReportingDelegateFactory reporting_delegate_factory_;
  ProfileReportGenerator generator_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;

  std::unique_ptr<NiceMock<policy::MockPolicyService>> policy_service_;
  policy::PolicyMap policy_map_;
};

TEST_F(ProfileReportGeneratorTest, ProfileNotActivated) {
  const base::FilePath profile_path =
      profile_manager()->profiles_dir().AppendASCII(kIdleProfile);
  ProfileAttributesInitParams params;
  params.profile_path = profile_path;
  params.profile_name = kIdleProfile16;
  profile_manager()->profile_attributes_storage()->AddProfile(
      std::move(params));
  std::unique_ptr<em::ChromeUserProfileInfo> response =
      generator_.MaybeGenerate(profile_path, kIdleProfile, ReportType::kFull);
  ASSERT_FALSE(response.get());
}

TEST_F(ProfileReportGeneratorTest, UnsignedInProfile) {
  auto report = GenerateReport();
  EXPECT_FALSE(report->has_chrome_signed_in_user());
}

TEST_F(ProfileReportGeneratorTest, SignedInProfile) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);

  IdentityTestEnvironmentProfileAdaptor identity_test_env_adaptor(profile());
  auto expected_info =
      identity_test_env_adaptor.identity_test_env()->SetPrimaryAccount(
          "test@mail.com", signin::ConsentLevel::kSync);
  auto report = GenerateReport();
  EXPECT_TRUE(report->has_chrome_signed_in_user());
  EXPECT_EQ(expected_info.email, report->chrome_signed_in_user().email());
  EXPECT_EQ(expected_info.gaia,
            report->chrome_signed_in_user().obfuscated_gaia_id());
}

TEST_F(ProfileReportGeneratorTest,
       SignedInProfileReplaceSyncPromosWithSigninPromos) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(syncer::kReplaceSyncPromosWithSignInPromos);

  IdentityTestEnvironmentProfileAdaptor identity_test_env_adaptor(profile());
  auto expected_info =
      identity_test_env_adaptor.identity_test_env()->SetPrimaryAccount(
          "test@mail.com", signin::ConsentLevel::kSignin);
  auto report = GenerateReport();
  EXPECT_TRUE(report->has_chrome_signed_in_user());
  EXPECT_EQ(expected_info.email, report->chrome_signed_in_user().email());
  EXPECT_EQ(expected_info.gaia,
            report->chrome_signed_in_user().obfuscated_gaia_id());
}

TEST_F(ProfileReportGeneratorTest, ProfileIdObfuscate) {
  auto report = generator_.MaybeGenerate(profile()->GetPath(),
                                         profile()->GetProfileUserName(),
                                         ReportType::kProfileReport);
  ASSERT_TRUE(report);
  EXPECT_EQ(profile()->GetProfileUserName(), report->name());
  EXPECT_NE(profile()->GetPath().AsUTF8Unsafe(), report->id());
  EXPECT_TRUE(report->is_detail_available());

  auto report2 = generator_.MaybeGenerate(profile()->GetPath(),
                                          profile()->GetProfileUserName(),
                                          ReportType::kProfileReport);
  // Profile id is obfuscated with `kProfileReport` type, but the obfuscated
  // result is consistent.
  EXPECT_EQ(report->id(), report2->id());

  TestingProfile* another_profile =
      profile_manager()->CreateTestingProfile("another_profile");
  auto report3 = generator_.MaybeGenerate(another_profile->GetPath(),
                                          another_profile->GetProfileUserName(),
                                          ReportType::kProfileReport);
  // Different profiles' id will be different even after obfuscation.
  EXPECT_NE(report->id(), report3->id());
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

TEST_F(ProfileReportGeneratorTest, PoliciesHidden) {
  std::unique_ptr<em::ChromeUserProfileInfo> report = GenerateReport();

  for (const auto& policy : report->chrome_policies()) {
    EXPECT_EQ("true", policy.value());
  }

  generator_.set_is_machine_scope(false);
  report = GenerateReport();

  for (const auto& policy : report->chrome_policies()) {
    if (policy.scope() == em::Policy_PolicyScope_SCOPE_MACHINE) {
      EXPECT_EQ("\"********\"", policy.value());
    } else {
      EXPECT_EQ("true", policy.value());
    }
  }
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ProfileReportGeneratorTest, IsAffiliated) {
  profile()->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting(
      {kAffiliationId1});
  g_browser_process->browser_policy_connector()
      ->SetDeviceAffiliatedIdsForTesting({kAffiliationId1});

  std::unique_ptr<em::ChromeUserProfileInfo> report = GenerateReport();

  ASSERT_TRUE(report->has_affiliation());

  EXPECT_TRUE(report->affiliation().is_affiliated());
  EXPECT_FALSE(report->affiliation().has_unaffiliation_reason());
}

TEST_F(ProfileReportGeneratorTest, NotAffiliated) {
  profile()->GetProfilePolicyConnector()->SetUserAffiliationIdsForTesting(
      {kAffiliationId1});
  g_browser_process->browser_policy_connector()
      ->SetDeviceAffiliatedIdsForTesting({kAffiliationId2});

  std::unique_ptr<em::ChromeUserProfileInfo> report = GenerateReport();

  ASSERT_TRUE(report->has_affiliation());

  EXPECT_FALSE(report->affiliation().is_affiliated());
  EXPECT_EQ(em::AffiliationState_UnaffiliationReason_USER_UNMANAGED,
            report->affiliation().unaffiliation_reason());
}
#endif // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_EXTENSIONS)
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
  EXPECT_EQ(kJustification, report->extension_requests(0).justification());
}

TEST_F(ProfileReportGeneratorTest, PendingRequestNotSupportProfileReporting) {
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kCloudExtensionRequestEnabled,
      std::make_unique<base::Value>(true));
  std::vector<std::string> ids = {kExtensionId};
  SetExtensionToPendingList(ids);

  generator_.set_is_machine_scope(false);
  auto report = GenerateReport();
  ASSERT_EQ(0, report->extension_requests_size());
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

TEST_F(ProfileReportGeneratorTest, DisableExtensionInfo) {
  extensions::ExtensionBuilder builder(
      "name", extensions::ExtensionBuilder::Type::EXTENSION);

  auto extension = builder.SetID("abcdefghijklmnoabcdefghijklmnoab").Build();
  extensions::ExtensionRegistry::Get(profile())->AddEnabled(extension);

  EXPECT_EQ(1, GenerateReport()->extensions_size());

  bool is_extension_enabled = false;
  generator_.SetExtensionsEnabledCallback(base::BindRepeating(
      [](bool* is_extension_enabled) { return *is_extension_enabled; },
      &is_extension_enabled));

  EXPECT_EQ(0, GenerateReport()->extensions_size());

  is_extension_enabled = true;
  EXPECT_EQ(1, GenerateReport()->extensions_size());
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace enterprise_reporting
