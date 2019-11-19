// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/state_store.h"

#include <stdint.h>

#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/platform_state_store.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_factory.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/quota_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/test/test_reg_util_win.h"
#endif

namespace safe_browsing {

#if defined(OS_WIN)

// A base test fixture that redirects HKCU for testing the platform state store
// backed by the Windows registry to prevent interference with existing Chrome
// installs or other tests.
class PlatformStateStoreTestBase : public ::testing::Test {
 protected:
  PlatformStateStoreTestBase() {}

  void SetUp() override {
    ::testing::Test::SetUp();
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;

  DISALLOW_COPY_AND_ASSIGN(PlatformStateStoreTestBase);
};

#else  // OS_WIN

using PlatformStateStoreTestBase = ::testing::Test;

#endif  // !OS_WIN

// A test fixture with a testing profile that writes its user prefs to a json
// file.
class StateStoreTest : public PlatformStateStoreTestBase {
 protected:
  struct TestData {
    IncidentType type;
    const char* key;
    uint32_t digest;
  };

  StateStoreTest()
      : profile_(nullptr),
        task_runner_(new base::TestSimpleTaskRunner()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    PlatformStateStoreTestBase::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(profile_manager_.SetUp());
    CreateProfile();
  }

  void DeleteProfile() {
    if (profile_) {
      profile_ = nullptr;
      profile_manager_.DeleteTestingProfile(kProfileName_);
    }
  }

  // Commits a pending write (which posts a task to task_runner_) and waits for
  // it to finish.
  void CommitWrite() {
    ASSERT_NE(nullptr, profile_);
    profile_->GetPrefs()->CommitPendingWrite();
    task_runner_->RunUntilIdle();
  }

  // Removes the safebrowsing.incidents_sent preference from the profile's pref
  // store.
  void TrimPref() {
    ASSERT_EQ(nullptr, profile_);
    std::unique_ptr<base::Value> prefs(JSONFileValueDeserializer(GetPrefsPath())
                                           .Deserialize(nullptr, nullptr));
    ASSERT_NE(nullptr, prefs.get());
    base::DictionaryValue* dict = nullptr;
    ASSERT_TRUE(prefs->GetAsDictionary(&dict));
    ASSERT_TRUE(dict->Remove(prefs::kSafeBrowsingIncidentsSent, nullptr));
    ASSERT_TRUE(JSONFileValueSerializer(GetPrefsPath()).Serialize(*dict));
  }

  void CreateProfile() {
    ASSERT_EQ(nullptr, profile_);
    // Create the testing profile with a file-backed user pref store.
    sync_preferences::PrefServiceSyncableFactory factory;
    factory.SetUserPrefsFile(GetPrefsPath(), task_runner_.get());
    user_prefs::PrefRegistrySyncable* pref_registry =
        new user_prefs::PrefRegistrySyncable();
    RegisterUserProfilePrefs(pref_registry);
    profile_ = profile_manager_.CreateTestingProfile(
        kProfileName_, factory.CreateSyncable(pref_registry),
        base::UTF8ToUTF16(kProfileName_), 0, std::string(),
        TestingProfile::TestingFactories(),
        /*override_new_profile=*/base::Optional<bool>(false));
  }

  static const char kProfileName_[];
  static const TestData kTestData_[];
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile* profile_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

 private:
  base::FilePath GetPrefsPath() {
    return temp_dir_.GetPath().AppendASCII("prefs");
  }

  extensions::QuotaService::ScopedDisablePurgeForTesting
      disable_purge_for_testing_;
  base::ScopedTempDir temp_dir_;
  TestingProfileManager profile_manager_;

  DISALLOW_COPY_AND_ASSIGN(StateStoreTest);
};

// static
const char StateStoreTest::kProfileName_[] = "test_profile";
const StateStoreTest::TestData StateStoreTest::kTestData_[] = {
    {IncidentType::TRACKED_PREFERENCE, "tp_one", 1},
    {IncidentType::TRACKED_PREFERENCE, "tp_two", 2},
    {IncidentType::TRACKED_PREFERENCE, "tp_three", 3},
    {IncidentType::BINARY_INTEGRITY, "bi", 0},
};

TEST_F(StateStoreTest, MarkAsAndHasBeenReported) {
  StateStore state_store(profile_);

  for (const auto& data : kTestData_)
    ASSERT_FALSE(state_store.HasBeenReported(data.type, data.key, data.digest));

  {
    StateStore::Transaction transaction(&state_store);
    for (const auto& data : kTestData_) {
      transaction.MarkAsReported(data.type, data.key, data.digest);
      ASSERT_TRUE(
          state_store.HasBeenReported(data.type, data.key, data.digest));
    }
  }

  for (const auto& data : kTestData_)
    ASSERT_TRUE(state_store.HasBeenReported(data.type, data.key, data.digest));
}

TEST_F(StateStoreTest, ClearForType) {
  StateStore state_store(profile_);

  {
    StateStore::Transaction transaction(&state_store);
    for (const auto& data : kTestData_)
      transaction.MarkAsReported(data.type, data.key, data.digest);
  }

  for (const auto& data : kTestData_)
    ASSERT_TRUE(state_store.HasBeenReported(data.type, data.key, data.digest));

  const IncidentType removed_type = IncidentType::TRACKED_PREFERENCE;
  StateStore::Transaction(&state_store).ClearForType(removed_type);

  for (const auto& data : kTestData_) {
    if (data.type == removed_type) {
      ASSERT_FALSE(
          state_store.HasBeenReported(data.type, data.key, data.digest));
    } else {
      ASSERT_TRUE(
          state_store.HasBeenReported(data.type, data.key, data.digest));
    }
  }
}

TEST_F(StateStoreTest, ClearAll) {
  StateStore state_store(profile_);
  // Write some state to the store.
  {
    StateStore::Transaction transaction(&state_store);
    for (const auto& data : kTestData_)
      transaction.MarkAsReported(data.type, data.key, data.digest);
  }

  StateStore::Transaction(&state_store).ClearAll();

  for (const auto& data : kTestData_) {
    ASSERT_FALSE(state_store.HasBeenReported(data.type, data.key, data.digest));
  }

  // Write prefs out to the JsonPrefStore.
  CommitWrite();

  // Delete the profile.
  DeleteProfile();

  // Recreate the profile.
  CreateProfile();

  StateStore store_2(profile_);
  for (const auto& data : kTestData_) {
    // Verify that the state did not survive through the Platform State Store.
    ASSERT_FALSE(store_2.HasBeenReported(data.type, data.key, data.digest));
  }
}

TEST_F(StateStoreTest, Persistence) {
  // Write some state to the store.
  {
    StateStore state_store(profile_);
    StateStore::Transaction transaction(&state_store);
    for (const auto& data : kTestData_)
      transaction.MarkAsReported(data.type, data.key, data.digest);
  }

  // Run tasks to write prefs out to the JsonPrefStore.
  CommitWrite();

  // Delete the profile.
  DeleteProfile();

  // Recreate the profile.
  CreateProfile();

  // Verify that the state survived.
  StateStore state_store(profile_);
  for (const auto& data : kTestData_)
    ASSERT_TRUE(state_store.HasBeenReported(data.type, data.key, data.digest));
}

TEST_F(StateStoreTest, PersistenceWithStoreDelete) {
  // Write some state to the store.
  {
    StateStore state_store(profile_);
    StateStore::Transaction transaction(&state_store);
    for (const auto& data : kTestData_)
      transaction.MarkAsReported(data.type, data.key, data.digest);
  }

  // Write prefs out to the JsonPrefStore.
  CommitWrite();

  // Delete the profile.
  DeleteProfile();

  // Delete the state pref.
  TrimPref();

  // Recreate the profile.
  CreateProfile();

  StateStore state_store(profile_);
  for (const auto& data : kTestData_) {
#if defined(USE_PLATFORM_STATE_STORE)
    // Verify that the state survived.
    ASSERT_TRUE(state_store.HasBeenReported(data.type, data.key, data.digest));
#else
    // Verify that the state did not survive.
    ASSERT_FALSE(state_store.HasBeenReported(data.type, data.key, data.digest));
#endif
  }
}

}  // namespace safe_browsing
