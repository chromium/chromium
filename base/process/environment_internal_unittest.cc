// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/environment_internal.h"

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using EnvironmentInternalTest = PlatformTest;

namespace base {
namespace internal {

#if BUILDFLAG(IS_WIN)

namespace {
void ExpectEnvironmentBlock(const std::vector<std::wstring>& vars,
                            const std::wstring& block) {
  std::wstring expected;
  for (const auto& var : vars) {
    expected += var;
    expected.push_back('\0');
  }
  expected.push_back('\0');
  EXPECT_EQ(expected, block);
}
}  // namespace

TEST_F(EnvironmentInternalTest, AlterEnvironment) {
  const wchar_t empty[] = {'\0'};
  const wchar_t a2[] = {'A', '=', '2', '\0', '\0'};
  const wchar_t a2b3[] = {'A', '=', '2', '\0', 'B', '=', '3', '\0', '\0'};
  EnvironmentMap changes;
  NativeEnvironmentString e;

  e = AlterEnvironment(empty, changes);
  ExpectEnvironmentBlock({}, e);

  changes[L"A"] = L"1";
  e = AlterEnvironment(empty, changes);
  ExpectEnvironmentBlock({L"A=1"}, e);

  changes.clear();
  changes[L"A"] = std::wstring();
  e = AlterEnvironment(empty, changes);
  ExpectEnvironmentBlock({}, e);

  changes.clear();
  e = AlterEnvironment(a2, changes);
  ExpectEnvironmentBlock({L"A=2"}, e);

  changes.clear();
  changes[L"A"] = L"1";
  e = AlterEnvironment(a2, changes);
  ExpectEnvironmentBlock({L"A=1"}, e);

  changes.clear();
  changes[L"A"] = std::wstring();
  e = AlterEnvironment(a2, changes);
  ExpectEnvironmentBlock({}, e);

  changes.clear();
  changes[L"A"] = std::wstring();
  changes[L"B"] = std::wstring();
  e = AlterEnvironment(a2b3, changes);
  ExpectEnvironmentBlock({}, e);

  changes.clear();
  changes[L"A"] = std::wstring();
  e = AlterEnvironment(a2b3, changes);
  ExpectEnvironmentBlock({L"B=3"}, e);

  changes.clear();
  changes[L"B"] = std::wstring();
  e = AlterEnvironment(a2b3, changes);
  ExpectEnvironmentBlock({L"A=2"}, e);

  changes.clear();
  changes[L"A"] = L"1";
  changes[L"C"] = L"4";
  e = AlterEnvironment(a2b3, changes);
  // AlterEnvironment() currently always puts changed entries at the end.
  ExpectEnvironmentBlock({L"B=3", L"A=1", L"C=4"}, e);
}

#else  // !BUILDFLAG(IS_WIN)

TEST_F(EnvironmentInternalTest, AlterEnvironment) {
  const char* const empty[] = {nullptr};
  const char* const a2[] = {"A=2", nullptr};
  const char* const a2b3[] = {"A=2", "B=3", nullptr};
  EnvironmentMap changes;
  base::HeapArray<char*> e;

  e = AlterEnvironment(empty, changes);
  EXPECT_TRUE(e[0] == nullptr);

  changes["A"] = "1";
  e = AlterEnvironment(empty, changes);
  EXPECT_EQ(std::string("A=1"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  e = AlterEnvironment(empty, changes);
  EXPECT_TRUE(e[0] == nullptr);

  changes.clear();
  e = AlterEnvironment(a2, changes);
  EXPECT_EQ(std::string("A=2"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = "1";
  e = AlterEnvironment(a2, changes);
  EXPECT_EQ(std::string("A=1"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  e = AlterEnvironment(a2, changes);
  EXPECT_TRUE(e[0] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  changes["B"] = std::string();
  e = AlterEnvironment(a2b3, changes);
  EXPECT_TRUE(e[0] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  e = AlterEnvironment(a2b3, changes);
  EXPECT_EQ(std::string("B=3"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["B"] = std::string();
  e = AlterEnvironment(a2b3, changes);
  EXPECT_EQ(std::string("A=2"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = "1";
  changes["C"] = "4";
  e = AlterEnvironment(a2b3, changes);
  EXPECT_EQ(std::string("B=3"), e[0]);
  // AlterEnvironment() currently always puts changed entries at the end.
  EXPECT_EQ(std::string("A=1"), e[1]);
  EXPECT_EQ(std::string("C=4"), e[2]);
  EXPECT_TRUE(e[3] == nullptr);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace internal
}  // namespace base
