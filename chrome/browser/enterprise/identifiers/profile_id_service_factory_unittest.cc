// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"

#include "base/base64url.h"
#include "base/hash/sha1.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/browser/identifiers/identifiers_prefs.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) ||                \
    BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_ASH) &&         \
        !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "base/win/wmi.h"
#endif  // BUILDFLAG(IS_WIN)
#else
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_init_params.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) ||
        // BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_ASH) &&
        // !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace enterprise {

namespace {

constexpr char kFakeDeviceID[] = "fake-id";

}  // namespace

class ProfileIdServiceFactoryTest : public testing::Test {
 public:
  ProfileIdServiceFactoryTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  Profile* get_new_profile(const std::string& profile_name) {
    return profile_manager_.CreateTestingProfile(profile_name);
  }

 protected:
  void SetUp() override {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) ||                \
    BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_ASH) &&         \
        !BUILDFLAG(IS_CHROMEOS_LACROS)
    policy::BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetClientId(kFakeDeviceID);
#else
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_machine_name(kFakeDeviceID);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto init_params = crosapi::mojom::BrowserInitParams::New();
    init_params->device_properties = crosapi::mojom::DeviceProperties::New();
    init_params->device_properties->serial_number = kFakeDeviceID;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    store_.set_policy_data_for_testing(std::move(policy_data));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    fake_statistics_provider_.SetMachineStatistic(
        ash::system::kSerialNumberKeyForTest, kFakeDeviceID);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) ||
        // BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_ASH) &&
        // !BUILDFLAG(IS_CHROMEOS_LACROS)

    service_ = ProfileIdServiceFactory::GetForProfile(profile_);
    ASSERT_TRUE(service_);
  }

  std::string GetTestProfileId(const Profile* profile) {
    std::string encoded_string;
    std::string device_id = kFakeDeviceID;
#if BUILDFLAG(IS_WIN)
    device_id += base::WideToUTF8(
        base::win::WmiComputerSystemInfo::Get().serial_number());
#endif  // (BUILDFLAG(IS_WIN)
    base::Base64UrlEncode(
        base::SHA1HashString(profile->GetPrefs()->GetString(kProfileGUIDPref) +
                             device_id),
        base::Base64UrlEncodePolicy::OMIT_PADDING, &encoded_string);
    return encoded_string;
  }

  void SetProfileIdService(Profile* profile) {
    service_ = ProfileIdServiceFactory::GetForProfile(profile);
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<ProfileIdService> service_;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) ||                \
    BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_ASH) &&         \
        !BUILDFLAG(IS_CHROMEOS_LACROS)
  policy::FakeBrowserDMTokenStorage storage_;
#else
  policy::MockCloudPolicyStore store_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) ||
        // BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_ASH) &&
        // !BUILDFLAG(IS_CHROMEOS_LACROS)
};

// Tests multiple calls to get the profile identifier for the same profile has
// the same profile identifiers each time.
TEST_F(ProfileIdServiceFactoryTest, GetProfileId_MultipleCalls) {
  auto profile_id1 = service_->GetProfileId();
  EXPECT_EQ(GetTestProfileId(profile_), profile_id1.value());
  auto profile_id2 = service_->GetProfileId();
  EXPECT_EQ(profile_id2.value(), profile_id1.value());
  auto profile_id3 = service_->GetProfileId();
  EXPECT_EQ(profile_id3.value(), profile_id2.value());
}

// Tests that multiple profiles have different profile identifiers.
TEST_F(ProfileIdServiceFactoryTest, GetProfileId_MultipleProfiles) {
  // The original profile is the profile set in BaseTest.
  auto profile_id_1 = service_->GetProfileId();
  EXPECT_EQ(GetTestProfileId(profile_), profile_id_1.value());
  auto* profile_2 = get_new_profile("profile-2");
  SetProfileIdService(profile_2);
  auto profile_id_2 = service_->GetProfileId();
  EXPECT_EQ(GetTestProfileId(profile_2), profile_id_2.value());
  EXPECT_FALSE(profile_id_1.value() == profile_id_2.value());
}

// Tests that no profile identifier is created and no profile GUID is
// persisted(in the case that a profile guid did not previously exist) in guest
// profile sessions.
TEST_F(ProfileIdServiceFactoryTest, GetProfileId_Guest_Profile) {
  profile_->GetPrefs()->SetString(kProfileGUIDPref, "");
  profile_->SetGuestSession(true);
  SetProfileIdService(profile_);
  std::string profile_guid = profile_->GetPrefs()->GetString(kProfileGUIDPref);
  EXPECT_TRUE(profile_guid.empty());
}

// Tests that no service is created in OTR profiles since the factory has a
// profile preference for only creating the service for regular profiles.
TEST_F(ProfileIdServiceFactoryTest, GetProfileId_Incognito_Profile) {
  auto* otr_profile = profile_->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);
  SetProfileIdService(otr_profile);
  EXPECT_FALSE(service_);
}

}  // namespace enterprise
