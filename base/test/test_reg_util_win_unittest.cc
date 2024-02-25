// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_reg_util_win.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace registry_util {

namespace {
const wchar_t kTestKeyPath[] = L"Software\\Chromium\\Foo\\Baz\\TestKey";
const wchar_t kTestValueName[] = L"TestValue";
}  // namespace

class RegistryOverrideManagerTest : public testing::Test {
 protected:
  RegistryOverrideManagerTest() {
    // We assign a fake test key path to our test RegistryOverrideManager
    // so we don't interfere with any actual RegistryOverrideManagers running
    // on the system. This fake path will be auto-deleted by other
    // RegistryOverrideManagers in case we crash.
    fake_test_key_root_ = registry_util::GenerateTempKeyPath();

    // Ensure a clean test environment.
    base::win::RegKey key(HKEY_CURRENT_USER);
    key.DeleteKey(fake_test_key_root_.c_str());
    key.DeleteKey(kTestKeyPath);
  }

  ~RegistryOverrideManagerTest() override {
    base::win::RegKey key(HKEY_CURRENT_USER);
    key.DeleteKey(fake_test_key_root_.c_str());
  }

  void AssertKeyExists(const std::wstring& key_path) {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ))
        << key_path << " does not exist.";
  }

  void AssertKeyAbsent(const std::wstring& key_path) {
    base::win::RegKey key;
    ASSERT_NE(ERROR_SUCCESS,
              key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ))
        << key_path << " exists but it should not.";
  }

  void CreateKey(const std::wstring& key_path) {
    base::win::RegKey key;
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(HKEY_CURRENT_USER, key_path.c_str(), KEY_ALL_ACCESS));
  }

  std::wstring FakeOverrideManagerPath(const base::Time& time) {
    return fake_test_key_root_ + L"\\" +
           base::AsWString(base::NumberToString16(time.ToInternalValue()));
  }

  void CreateManager(const base::Time& timestamp) {
    manager_.reset(new RegistryOverrideManager(timestamp, fake_test_key_root_));
    manager_->OverrideRegistry(HKEY_CURRENT_USER);
  }

  std::wstring fake_test_key_root_;
  std::unique_ptr<RegistryOverrideManager> manager_;
};

TEST_F(RegistryOverrideManagerTest, Basic) {
  ASSERT_NO_FATAL_FAILURE(CreateManager(base::Time::Now()));

  base::win::RegKey create_key;
  EXPECT_EQ(ERROR_SUCCESS,
            create_key.Create(HKEY_CURRENT_USER, kTestKeyPath, KEY_ALL_ACCESS));
  EXPECT_TRUE(create_key.Valid());
  EXPECT_EQ(ERROR_SUCCESS, create_key.WriteValue(kTestValueName, 42));
  create_key.Close();

  ASSERT_NO_FATAL_FAILURE(AssertKeyExists(kTestKeyPath));

  DWORD value;
  base::win::RegKey read_key;
  EXPECT_EQ(ERROR_SUCCESS,
            read_key.Open(HKEY_CURRENT_USER, kTestKeyPath, KEY_READ));
  EXPECT_TRUE(read_key.Valid());
  EXPECT_EQ(ERROR_SUCCESS, read_key.ReadValueDW(kTestValueName, &value));
  EXPECT_EQ(42u, value);
  read_key.Close();

  manager_.reset();

  ASSERT_NO_FATAL_FAILURE(AssertKeyAbsent(kTestKeyPath));
}

TEST_F(RegistryOverrideManagerTest, DeleteStaleKeys) {
  static constexpr base::Time::Exploded kTestTimeExploded = {
      .year = 2013, .month = 11, .day_of_week = 1, .day_of_month = 4};
  base::Time kTestTime;
  EXPECT_TRUE(base::Time::FromUTCExploded(kTestTimeExploded, &kTestTime));

  std::wstring path_garbage = fake_test_key_root_ + L"\\Blah";
  std::wstring path_very_stale =
      FakeOverrideManagerPath(kTestTime - base::Days(100));
  std::wstring path_stale = FakeOverrideManagerPath(kTestTime - base::Days(5));
  std::wstring path_current =
      FakeOverrideManagerPath(kTestTime - base::Minutes(1));
  std::wstring path_future =
      FakeOverrideManagerPath(kTestTime + base::Minutes(1));

  ASSERT_NO_FATAL_FAILURE(CreateKey(path_garbage));
  ASSERT_NO_FATAL_FAILURE(CreateKey(path_very_stale));
  ASSERT_NO_FATAL_FAILURE(CreateKey(path_stale));
  ASSERT_NO_FATAL_FAILURE(CreateKey(path_current));
  ASSERT_NO_FATAL_FAILURE(CreateKey(path_future));

  ASSERT_NO_FATAL_FAILURE(CreateManager(kTestTime));
  manager_.reset();

  ASSERT_NO_FATAL_FAILURE(AssertKeyAbsent(path_garbage));
  ASSERT_NO_FATAL_FAILURE(AssertKeyAbsent(path_very_stale));
  ASSERT_NO_FATAL_FAILURE(AssertKeyAbsent(path_stale));
  ASSERT_NO_FATAL_FAILURE(AssertKeyExists(path_current));
  ASSERT_NO_FATAL_FAILURE(AssertKeyExists(path_future));
}

TEST_F(RegistryOverrideManagerTest, DoesNotUseMockTime) {
  // This test is targeted at scenarios when multiple tests run at the same
  // time using `RegistryOverrideManager`, new instances of
  // `RegistryOverrideManager` will clean up any redirected registry paths that
  // have the timestamp generated (1970) when using `base::Time::Now()` with
  // MOCK_TIME enabled, which then cause the currently running tests to fail
  // since their expected reg keys were deleted by the other test.

  // To fix this issue, we have updated `RegistryOverrideManager` by using
  // `base::subtle::TimeNowIgnoringOverride()` instead of `base::Time::Now()`.
  // So the real current time is used instead of the mock time in 1970. This can
  // resolve related `RegistryOverrideManager` test failure when using
  // MOCK_TIME. This test ensures we are fetching the real current time even
  // when using MOCK_TIME.

  // Use mock time to init RegKey, which is based on 1970-01-03.
  // The RegKey contains information about the registry for the
  // `RegistryOverrideManager`, which also contains a time stamp, which is used
  // to delete stale keys left over from crashed tests.
  base::test::TaskEnvironment test_task_env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  const base::Time kTestTime = base::Time::Now();

  std::wstring mock_time_path_stale =
      FakeOverrideManagerPath(kTestTime - base::Days(5));
  std::wstring mock_time_path_current =
      FakeOverrideManagerPath(kTestTime - base::Minutes(1));

  ASSERT_NO_FATAL_FAILURE(CreateKey(mock_time_path_stale));
  ASSERT_NO_FATAL_FAILURE(CreateKey(mock_time_path_current));

  // Use real time to init `path_real`.
  std::wstring path_real = GenerateTempKeyPath();
  ASSERT_NO_FATAL_FAILURE(CreateKey(path_real));

  ASSERT_NO_FATAL_FAILURE(CreateManager(kTestTime));
  manager_.reset();

  ASSERT_NO_FATAL_FAILURE(AssertKeyAbsent(mock_time_path_stale));
  ASSERT_NO_FATAL_FAILURE(AssertKeyExists(mock_time_path_current));
  // `path_real` should exist as it is initiated using real time, not mock time
  // in 1970.
  ASSERT_NO_FATAL_FAILURE(AssertKeyExists(path_real));

  // Use real time to init following the new set of keys with
  // `base::subtle::TimeNowIgnoringOverride()`.
  const base::Time kTestTime_new = base::subtle::TimeNowIgnoringOverride();
  std::wstring system_time_path_stale =
      FakeOverrideManagerPath(kTestTime_new - base::Days(5));
  std::wstring system_time_path_current =
      FakeOverrideManagerPath(kTestTime_new - base::Minutes(1));

  ASSERT_NO_FATAL_FAILURE(CreateKey(system_time_path_stale));
  ASSERT_NO_FATAL_FAILURE(CreateKey(system_time_path_current));

  ASSERT_NO_FATAL_FAILURE(CreateManager(kTestTime_new));
  manager_.reset();

  // Check old keys created with mock time
  ASSERT_NO_FATAL_FAILURE(AssertKeyAbsent(mock_time_path_stale));
  // While old keys are created using mock time in 1970, these keys will be
  // deleted.
  ASSERT_NO_FATAL_FAILURE(AssertKeyAbsent(mock_time_path_current));
  // `path_real` should exist as it is initiated using real time, not mock time
  // in 1970.
  ASSERT_NO_FATAL_FAILURE(AssertKeyExists(path_real));

  // Create a new manager with real system time.
  const base::Time kTestTime_latest = base::subtle::TimeNowIgnoringOverride();
  ASSERT_NO_FATAL_FAILURE(CreateManager(kTestTime_latest));
  manager_.reset();

  // Check new keys created with current time
  ASSERT_NO_FATAL_FAILURE(AssertKeyAbsent(system_time_path_stale));
  ASSERT_NO_FATAL_FAILURE(AssertKeyExists(system_time_path_current));
}

}  // namespace registry_util
