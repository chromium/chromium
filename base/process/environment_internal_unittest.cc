// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/environment_internal.h"

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using EnvironmentInternalTest = PlatformTest;

namespace base::internal {

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

std::vector<wchar_t> MakeBlock(const std::vector<std::wstring>& vars) {
  std::vector<wchar_t> block;
  for (const auto& var : vars) {
    block.insert(block.end(), var.begin(), var.end());
    block.push_back(L'\0');
  }
  block.push_back(L'\0');
  return block;
}
}  // namespace

TEST_F(EnvironmentInternalTest, AlterEnvironment) {
  EnvironmentMap changes;
  NativeEnvironmentString e;

  e = AlterEnvironment(base::span<const wchar_t>(), changes);
  ExpectEnvironmentBlock({}, e);

  changes[L"A"] = L"1";
  e = AlterEnvironment(base::span<const wchar_t>(), changes);
  ExpectEnvironmentBlock({L"A=1"}, e);

  changes.clear();
  changes[L"A"] = std::wstring();
  e = AlterEnvironment(base::span<const wchar_t>(), changes);
  ExpectEnvironmentBlock({}, e);

  changes.clear();
  auto a2 = MakeBlock({L"A=2"});
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
  auto a2b3 = MakeBlock({L"A=2", L"B=3"});
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
  // SAFETY: The environment blocks used in these tests are static and
  // null-terminated, satisfying AlterEnvironment's requirements.
  auto empty = base::span<const char* const>();
  auto a2 = std::vector<const char*>{"A=2"};
  auto a2b3 = std::vector<const char*>{"A=2", "B=3"};
  EnvironmentMap changes;
  base::HeapArray<char*> e;

  e = UNSAFE_BUFFERS(AlterEnvironment(empty, changes));
  EXPECT_TRUE(e[0] == nullptr);

  changes["A"] = "1";
  e = UNSAFE_BUFFERS(AlterEnvironment(empty, changes));
  EXPECT_EQ(std::string("A=1"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  e = UNSAFE_BUFFERS(AlterEnvironment(empty, changes));
  EXPECT_TRUE(e[0] == nullptr);

  changes.clear();
  e = UNSAFE_BUFFERS(AlterEnvironment(a2, changes));
  EXPECT_EQ(std::string("A=2"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = "1";
  e = UNSAFE_BUFFERS(AlterEnvironment(a2, changes));
  EXPECT_EQ(std::string("A=1"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  e = UNSAFE_BUFFERS(AlterEnvironment(a2, changes));
  EXPECT_TRUE(e[0] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  changes["B"] = std::string();
  e = UNSAFE_BUFFERS(AlterEnvironment(a2b3, changes));
  EXPECT_TRUE(e[0] == nullptr);

  changes.clear();
  changes["A"] = std::string();
  e = UNSAFE_BUFFERS(AlterEnvironment(a2b3, changes));
  EXPECT_EQ(std::string("B=3"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["B"] = std::string();
  e = UNSAFE_BUFFERS(AlterEnvironment(a2b3, changes));
  EXPECT_EQ(std::string("A=2"), e[0]);
  EXPECT_TRUE(e[1] == nullptr);

  changes.clear();
  changes["A"] = "1";
  changes["C"] = "4";
  e = UNSAFE_BUFFERS(AlterEnvironment(a2b3, changes));
  EXPECT_EQ(std::string("B=3"), e[0]);
  // AlterEnvironment() currently always puts changed entries at the end.
  EXPECT_EQ(std::string("A=1"), e[1]);
  EXPECT_EQ(std::string("C=4"), e[2]);
  EXPECT_TRUE(e[3] == nullptr);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace base::internal
