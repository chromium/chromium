// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/platform_state_store.h"

#include <windows.h>

#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace platform_state_store {

namespace {

const char kTestData_[] = "comme un poisson";
const DWORD kTestDataSize_ = sizeof(kTestData_) - 1;

}  // namespace

class PlatformStateStoreWinTest : public ::testing::Test {
 protected:
  PlatformStateStoreWinTest()
      : profile_(nullptr),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  PlatformStateStoreWinTest(const PlatformStateStoreWinTest&) = delete;
  PlatformStateStoreWinTest& operator=(const PlatformStateStoreWinTest&) =
      delete;

  void SetUp() override {
    ::testing::Test::SetUp();
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_TRUE(profile_manager_.SetUp());
  }

  // Creates/resets |profile_|. If |new_profile| is true, the profile will
  // believe that it is new (Profile::IsNewProfile() will return true).
  void ResetProfile(bool new_profile) {
    if (profile_) {
      profile_manager_.DeleteTestingProfile(kProfileName_);
      profile_ = nullptr;
    }
    std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs(
        new sync_preferences::TestingPrefServiceSyncable);
    RegisterUserProfilePrefs(prefs->registry());
    profile_ = profile_manager_.CreateTestingProfile(
        kProfileName_, std::move(prefs), base::UTF8ToUTF16(kProfileName_), 0,
        TestingProfile::TestingFactories(), /*is_supervised_profile=*/false,
        std::optional<bool>(new_profile));
    if (new_profile)
      ASSERT_TRUE(profile_->IsNewProfile());
    else
      ASSERT_FALSE(profile_->IsNewProfile());
  }

  void WriteTestData() {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, kStoreKeyName_,
                                        KEY_SET_VALUE | KEY_WOW64_32KEY));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(base::UTF8ToWide(kProfileName_).c_str(),
                             &kTestData_[0], kTestDataSize_, REG_BINARY));
  }

  void AssertTestDataIsAbsent() {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, kStoreKeyName_,
                                      KEY_QUERY_VALUE | KEY_WOW64_32KEY));
    ASSERT_FALSE(key.HasValue(base::UTF8ToWide(kProfileName_).c_str()));
  }

  void AssertTestDataIsPresent() {
    char buffer[kTestDataSize_] = {};
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, kStoreKeyName_,
                                      KEY_QUERY_VALUE | KEY_WOW64_32KEY));
    DWORD data_size = kTestDataSize_;
    DWORD data_type = REG_NONE;
    ASSERT_EQ(ERROR_SUCCESS,
              key.ReadValue(base::UTF8ToWide(kProfileName_).c_str(), &buffer[0],
                            &data_size, &data_type));
    EXPECT_EQ(static_cast<DWORD>(REG_BINARY), data_type);
    ASSERT_EQ(kTestDataSize_, data_size);
    EXPECT_EQ(std::string(&buffer[0], data_size),
              std::string(&kTestData_[0], kTestDataSize_));
  }

  static const char kProfileName_[];
  static const wchar_t kStoreKeyName_[];
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  TestingProfileManager profile_manager_;
};

// static
const char PlatformStateStoreWinTest::kProfileName_[] = "test_profile";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const wchar_t PlatformStateStoreWinTest::kStoreKeyName_[] =
    L"Software\\Google\\Chrome\\IncidentsSent";
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
const wchar_t PlatformStateStoreWinTest::kStoreKeyName_[] =
    L"Software\\Google\\Chrome for Testing\\IncidentsSent";
#else
const wchar_t PlatformStateStoreWinTest::kStoreKeyName_[] =
    L"Software\\Chromium\\IncidentsSent";
#endif

// Tests that store data is written correctly to the proper location.
TEST_F(PlatformStateStoreWinTest, WriteStoreData) {
  ResetProfile(false /* !new_profile */);

  ASSERT_FALSE(base::win::RegKey(HKEY_CURRENT_USER, kStoreKeyName_,
                                 KEY_QUERY_VALUE | KEY_WOW64_32KEY)
                   .HasValue(base::UTF8ToWide(kProfileName_).c_str()));
  WriteStoreData(profile_, kTestData_);
  AssertTestDataIsPresent();
}

// Tests that store data is read from the proper location.
TEST_F(PlatformStateStoreWinTest, ReadStoreData) {
  // Put some data in the registry.
  WriteTestData();

  ResetProfile(false /* !new_profile */);
  std::string data;
  PlatformStateStoreLoadResult result = ReadStoreData(profile_, &data);
  EXPECT_EQ(PlatformStateStoreLoadResult::SUCCESS, result);
  EXPECT_EQ(std::string(&kTestData_[0], kTestDataSize_), data);
}

// Tests that an empty write clears the stored data.
TEST_F(PlatformStateStoreWinTest, WriteEmptyStoreData) {
  // Put some data in the registry.
  WriteTestData();

  ResetProfile(false /* !new_profile */);

  WriteStoreData(profile_, std::string());
  AssertTestDataIsAbsent();
}

// Tests that data in the registry is ignored if the profile is new.
TEST_F(PlatformStateStoreWinTest, ReadNewProfileClearData) {
  ResetProfile(true /* new_profile */);

  // Put some data in the registry.
  WriteTestData();

  std::string data;
  PlatformStateStoreLoadResult result = ReadStoreData(profile_, &data);
  EXPECT_EQ(PlatformStateStoreLoadResult::CLEARED_DATA, result);
  EXPECT_EQ(std::string(), data);
  AssertTestDataIsAbsent();
}

}  // namespace platform_state_store
}  // namespace safe_browsing
