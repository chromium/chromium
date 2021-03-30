// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_reg_util_win.h"

#include <stdint.h>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <windows.h>

namespace registry_util {

namespace {

constexpr char16_t kTimestampDelimiter[] = u"$";
constexpr wchar_t kTempTestKeyPath[] = L"Software\\Chromium\\TempTestKeys";

void DeleteStaleTestKeys(const base::Time& now,
                         const std::wstring& test_key_root) {
  base::win::RegKey test_root_key;
  if (test_root_key.Open(HKEY_CURRENT_USER,
                         test_key_root.c_str(),
                         KEY_ALL_ACCESS) != ERROR_SUCCESS) {
    // This will occur on first-run, but is harmless.
    return;
  }

  base::win::RegistryKeyIterator iterator_test_root_key(HKEY_CURRENT_USER,
                                                        test_key_root.c_str());
  for (; iterator_test_root_key.Valid(); ++iterator_test_root_key) {
    std::wstring key_name = iterator_test_root_key.Name();
    std::vector<base::StringPiece16> tokens = base::SplitStringPiece(
        base::AsStringPiece16(key_name), kTimestampDelimiter,
        base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (tokens.empty())
      continue;
    int64_t key_name_as_number = 0;

    if (!base::StringToInt64(tokens[0], &key_name_as_number)) {
      test_root_key.DeleteKey(key_name.c_str());
      continue;
    }

    base::Time key_time = base::Time::FromInternalValue(key_name_as_number);
    base::TimeDelta age = now - key_time;

    if (age > base::TimeDelta::FromHours(24))
      test_root_key.DeleteKey(key_name.c_str());
  }
}

std::wstring GenerateTempKeyPath(const std::wstring& test_key_root,
                                 const base::Time& timestamp) {
  return base::AsWString(base::StrCat(
      {base::AsStringPiece16(test_key_root), u"\\",
       base::NumberToString16(timestamp.ToInternalValue()), kTimestampDelimiter,
       base::ASCIIToUTF16(base::GenerateGUID())}));
}

}  // namespace

RegistryOverrideManager::ScopedRegistryKeyOverride::ScopedRegistryKeyOverride(
    HKEY override,
    const std::wstring& key_path)
    : override_(override), key_path_(key_path) {}

RegistryOverrideManager::
    ScopedRegistryKeyOverride::~ScopedRegistryKeyOverride() {
  ::RegOverridePredefKey(override_, NULL);
  base::win::RegKey(HKEY_CURRENT_USER, L"", KEY_QUERY_VALUE)
      .DeleteKey(key_path_.c_str());
}

RegistryOverrideManager::RegistryOverrideManager()
    : timestamp_(base::Time::Now()), test_key_root_(kTempTestKeyPath) {
  DeleteStaleTestKeys(timestamp_, test_key_root_);
}

RegistryOverrideManager::RegistryOverrideManager(
    const base::Time& timestamp,
    const std::wstring& test_key_root)
    : timestamp_(timestamp), test_key_root_(test_key_root) {
  DeleteStaleTestKeys(timestamp_, test_key_root_);
}

RegistryOverrideManager::~RegistryOverrideManager() {}

void RegistryOverrideManager::OverrideRegistry(HKEY override) {
  OverrideRegistry(override, nullptr);
}

void RegistryOverrideManager::OverrideRegistry(HKEY override,
                                               std::wstring* override_path) {
  std::wstring key_path = GenerateTempKeyPath(test_key_root_, timestamp_);

  base::win::RegKey temp_key;
  ASSERT_EQ(ERROR_SUCCESS, temp_key.Create(HKEY_CURRENT_USER, key_path.c_str(),
                                           KEY_ALL_ACCESS));
  ASSERT_EQ(ERROR_SUCCESS, ::RegOverridePredefKey(override, temp_key.Handle()));

  overrides_.push_back(
      std::make_unique<ScopedRegistryKeyOverride>(override, key_path));
  if (override_path)
    override_path->assign(key_path);
}

std::wstring GenerateTempKeyPath() {
  return GenerateTempKeyPath(kTempTestKeyPath, base::Time::Now());
}

}  // namespace registry_util
