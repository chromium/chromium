// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/managed_profile_creator.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "base/win/wmi.h"
#endif  // BUILDFLAG(IS_WIN)

using enterprise::ProfileIdServiceFactory;
using testing::_;

const char kExampleGuid[] = "GUID-1234";
constexpr char kFakeDeviceID[] = "fake-id";

class MockManagedProfileCreationDelegate
    : public ManagedProfileCreationDelegate {
 public:
  MockManagedProfileCreationDelegate() = default;
  ~MockManagedProfileCreationDelegate() override = default;

  MOCK_METHOD(void,
              SetManagedAttributesForProfile,
              (ProfileAttributesEntry*),
              (override));
  MOCK_METHOD(void, CheckManagedProfileStatus, (Profile*), (override));
  MOCK_METHOD(void,
              OnManagedProfileInitialized,
              (Profile*, Profile*, ProfileCreationCallback),
              (override));
};

class ManagedProfileCreatorTest : public testing::Test {
 public:
  ManagedProfileCreatorTest()
      : profile_manager_(std::make_unique<TestingProfileManager>(
            TestingBrowserProcess::GetGlobal())) {}

  ~ManagedProfileCreatorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test_profile");
    mock_delegate_ = std::make_unique<MockManagedProfileCreationDelegate>();
    policy::BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetClientId(kFakeDeviceID);
  }

  // Callback for the ManagedProfileCreator.
  void OnProfileCreated(base::OnceClosure quit_closure,
                        base::WeakPtr<Profile> profile) {
    creator_callback_called_ = true;
    created_profile_ = profile.get();
    if (quit_closure) {
      std::move(quit_closure).Run();
    }
  }

  void AddProfileCreationCallback() {
    EXPECT_CALL(*mock_delegate_, OnManagedProfileInitialized(_, _, _))
        .WillOnce([](Profile* source_profile, Profile* new_profile,
                     ProfileCreationCallback callback) {
          std::move(callback).Run(new_profile->GetWeakPtr());
        });
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<Profile> profile_;
  raw_ptr<Profile> created_profile_;
  bool creator_callback_called_ = false;
  std::unique_ptr<MockManagedProfileCreationDelegate> mock_delegate_;
  policy::FakeBrowserDMTokenStorage storage_;
};

TEST_F(ManagedProfileCreatorTest, CreatesNewProfile) {
  base::RunLoop loop;

  EXPECT_CALL(*mock_delegate_, SetManagedAttributesForProfile(_)).Times(1);
  EXPECT_CALL(*mock_delegate_, CheckManagedProfileStatus(_)).Times(1);
  AddProfileCreationCallback();

  ManagedProfileCreator creator(
      profile_, "id", u"local_profile_name", std::move(mock_delegate_),
      base::BindOnce(&ManagedProfileCreatorTest::OnProfileCreated,
                     base::Unretained(this), loop.QuitClosure()),
      kExampleGuid);

  loop.Run();

  EXPECT_TRUE(creator_callback_called_);
  ASSERT_TRUE(created_profile_);

  auto* entry = TestingBrowserProcess::GetGlobal()
                    ->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(created_profile_->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_EQ("id", entry->GetProfileManagementId());
  EXPECT_EQ(u"local_profile_name", entry->GetName());

  std::string device_id = kFakeDeviceID;
#if BUILDFLAG(IS_WIN)
  device_id +=
      base::WideToUTF8(base::win::WmiComputerSystemInfo::Get().serial_number());
#endif  // (BUILDFLAG(IS_WIN)

  EXPECT_EQ(ProfileIdServiceFactory::GetForProfile(profile_)
                ->GetProfileIdWithGuidAndDeviceId(kExampleGuid, device_id)
                .value(),
            ProfileIdServiceFactory::GetForProfile(created_profile_)
                ->GetProfileId()
                .value());
}

TEST_F(ManagedProfileCreatorTest, LoadsExistingProfile) {
  auto* profile_manager = g_browser_process->profile_manager();
  Profile& new_profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  base::FilePath path = new_profile.GetPath();
  {
    auto* entry = profile_manager->GetProfileAttributesStorage()
                      .GetProfileAttributesWithPath(path);
    entry->SetProfileManagementId("id");
  }
  base::RunLoop loop;
  AddProfileCreationCallback();
  ManagedProfileCreator creator(
      profile_, path, std::move(mock_delegate_),
      base::BindOnce(&ManagedProfileCreatorTest::OnProfileCreated,
                     base::Unretained(this), loop.QuitClosure()));
  loop.Run();
  EXPECT_TRUE(creator_callback_called_);
  ASSERT_TRUE(created_profile_);
  EXPECT_EQ(path, created_profile_->GetPath());

  auto* entry = profile_manager->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(created_profile_->GetPath());
  ASSERT_TRUE(entry);
  EXPECT_EQ("id", entry->GetProfileManagementId());
}
