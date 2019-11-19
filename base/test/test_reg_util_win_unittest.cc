// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_reg_util_win.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
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
  base::Time::Exploded kTestTimeExploded = {2013, 11, 1, 4, 0, 0, 0, 0};
  base::Time kTestTime;
  EXPECT_TRUE(base::Time::FromUTCExploded(kTestTimeExploded, &kTestTime));

  std::wstring path_garbage = fake_test_key_root_ + L"\\Blah";
  std::wstring path_very_stale =
      FakeOverrideManagerPath(kTestTime - base::TimeDelta::FromDays(100));
  std::wstring path_stale =
      FakeOverrideManagerPath(kTestTime - base::TimeDelta::FromDays(5));
  std::wstring path_current =
      FakeOverrideManagerPath(kTestTime - base::TimeDelta::FromMinutes(1));
  std::wstring path_future =
      FakeOverrideManagerPath(kTestTime + base::TimeDelta::FromMinutes(1));

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

}  // namespace registry_util
