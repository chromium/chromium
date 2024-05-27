// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"

#include "base/base64url.h"
#include "base/hash/sha1.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/uuid.h"
#include "chrome/browser/enterprise/identifiers/profile_id_delegate_impl.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/browser/identifiers/identifiers_prefs.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) &&               \
    !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#if BUILDFLAG(IS_WIN)
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
        // BUILDFLAG(IS_ANDROID)  || BUILDFLAG(IS_CHROMEOS) &&
        // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)

namespace enterprise {

namespace {

constexpr char kFakeDeviceID[] = "fake-id";

}  // namespace

class ProfileIdServiceFactoryTest : public testing::Test,
                                    public ProfileManagerObserver {
 public:
  ProfileIdServiceFactoryTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) &&               \
    !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
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
    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  kFakeDeviceID);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_ANDROID)  || BUILDFLAG(IS_CHROMEOS) &&
        // !BUILDFLAG(IS_CHROMEOS_ASH) &&
        // !BUILDFLAG(IS_CHROMEOS_LACROS)

    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");

    service_ = ProfileIdServiceFactory::GetForProfile(profile_);
    EXPECT_TRUE(service_);

    profile_manager_observer_.Observe(profile_manager_.profile_manager());
  }

  Profile* CreateProfile(const std::string& profile_name) {
    return profile_manager_.CreateTestingProfile(profile_name);
  }

 protected:
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

  void OnProfileCreationStarted(Profile* profile) override {
    if (!preset_guid_.empty()) {
      enterprise::PresetProfileManagmentData::Get(profile)->SetGuid(
          preset_guid_);
    }
  }

// TODO(b/341267441): Enable this test for chrome os ash when
// `OnProfileCreationStarted` is fixed for `FakeProfileManager`.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  Profile* CreateNewProfileWithPresetGuid(std::string preset_guid) {
    Profile* new_profile = nullptr;
    // Making sure no two profiles have duplicate names/paths.
    std::string new_profile_name =
        "Profile " + base::Uuid::GenerateRandomV4().AsLowercaseString();
    preset_guid_ = preset_guid;

    base::RunLoop run_loop;

    ProfileManager::CreateMultiProfileAsync(
        base::UTF8ToUTF16(new_profile_name), /*icon_index=*/0,
        /*is_hidden=*/false,
        base::BindLambdaForTesting([&new_profile, &run_loop](Profile* profile) {
          ASSERT_TRUE(profile);
          new_profile = profile;
          run_loop.Quit();
        }));

    run_loop.Run();
    return new_profile;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<ProfileIdService> service_;
  std::string preset_guid_;
  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID) ||                                         \
    BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_ASH) &&         \
        !BUILDFLAG(IS_CHROMEOS_LACROS)
  policy::FakeBrowserDMTokenStorage storage_;
#else
  policy::MockCloudPolicyStore store_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
#endif
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_ANDROID)  ||
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
  auto* profile_2 = CreateProfile("profile-2");
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

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
TEST_F(ProfileIdServiceFactoryTest, GetProfileIdWithPresetGuid) {
  std::string random_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::string device_id = kFakeDeviceID;
#if BUILDFLAG(IS_WIN)
  device_id +=
      base::WideToUTF8(base::win::WmiComputerSystemInfo::Get().serial_number());
#endif  // (BUILDFLAG(IS_WIN)
  std::string expected_profile_id =
      service_->GetProfileIdWithGuidAndDeviceId(random_guid, device_id).value();

  auto* new_profile = CreateNewProfileWithPresetGuid(random_guid);
  SetProfileIdService(new_profile);

  auto new_profile_id = service_->GetProfileId();
  EXPECT_EQ(new_profile_id, expected_profile_id);
}

TEST_F(ProfileIdServiceFactoryTest, PresetGuidIdUniqueness) {
  auto old_profile_id = service_->GetProfileId();
  std::string random_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  auto* new_profile = CreateNewProfileWithPresetGuid(random_guid);
  SetProfileIdService(new_profile);

  EXPECT_NE(service_->GetProfileId(), old_profile_id);
}

TEST_F(ProfileIdServiceFactoryTest, PresetGuidDataIsOneOff) {
  std::string random_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  auto* preset_guid_profile = CreateNewProfileWithPresetGuid(random_guid);
  SetProfileIdService(preset_guid_profile);
  auto preset_guid_profile_id = service_->GetProfileId();

  auto* no_preset_guid_profile = CreateNewProfileWithPresetGuid(std::string());
  SetProfileIdService(no_preset_guid_profile);

  EXPECT_NE(service_->GetProfileId(), preset_guid_profile_id);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

}  // namespace enterprise
