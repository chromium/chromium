// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/crash_logging.h"

#include <map>
#include <memory>
#include <sstream>
#include <string_view>

#include "base/memory/raw_ref.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

namespace base {
namespace debug {

namespace {

class TestCrashKeyImplementation : public CrashKeyImplementation {
 public:
  explicit TestCrashKeyImplementation(std::map<std::string, std::string>& data)
      : data_(data) {}

  TestCrashKeyImplementation(const TestCrashKeyImplementation&) = delete;
  TestCrashKeyImplementation& operator=(const TestCrashKeyImplementation&) =
      delete;

  CrashKeyString* Allocate(const char* name, CrashKeySize size) override {
    return new CrashKeyString(name, size);
  }

  void Set(CrashKeyString* crash_key, std::string_view value) override {
    ASSERT_TRUE(data_->emplace(crash_key->name, value).second);
  }

  void Clear(CrashKeyString* crash_key) override {
    ASSERT_EQ(1u, data_->erase(crash_key->name));
  }

  void OutputCrashKeysToStream(std::ostream& out) override {
    out << "Got " << data_->size() << " crash keys.";
  }

 private:
  const raw_ref<std::map<std::string, std::string>> data_;
};

}  // namespace

class CrashLoggingTest : public ::testing::Test {
 public:
  CrashLoggingTest() {
    SetCrashKeyImplementation(
        std::make_unique<TestCrashKeyImplementation>(data_));
  }

  ~CrashLoggingTest() override { SetCrashKeyImplementation(nullptr); }

  const std::map<std::string, std::string>& data() const { return data_; }

 private:
  std::map<std::string, std::string> data_;
};

// Should not crash.
TEST(UninitializedCrashLoggingTest, Basic) {
  static auto* crash_key = AllocateCrashKeyString("test", CrashKeySize::Size32);
  EXPECT_FALSE(crash_key);

  SetCrashKeyString(crash_key, "value");

  ClearCrashKeyString(crash_key);
}

TEST_F(CrashLoggingTest, Basic) {
  static auto* crash_key = AllocateCrashKeyString("test", CrashKeySize::Size32);
  EXPECT_TRUE(crash_key);
  EXPECT_THAT(data(), IsEmpty());

  SetCrashKeyString(crash_key, "value");
  EXPECT_THAT(data(), ElementsAre(Pair("test", "value")));
  std::ostringstream stream;
  OutputCrashKeysToStream(stream);
  EXPECT_EQ("Got 1 crash keys.", stream.str());

  ClearCrashKeyString(crash_key);
  EXPECT_THAT(data(), IsEmpty());
  std::ostringstream stream2;
  OutputCrashKeysToStream(stream2);
  EXPECT_EQ("Got 0 crash keys.", stream2.str());
}

// Verify that the macros are properly setting crash keys.
TEST_F(CrashLoggingTest, Macros) {
  {
    SCOPED_CRASH_KEY_BOOL("category", "bool-value", false);
    EXPECT_THAT(data(), ElementsAre(Pair("category-bool-value", "false")));
  }

  {
    SCOPED_CRASH_KEY_BOOL("category", "bool-value", true);
    EXPECT_THAT(data(), ElementsAre(Pair("category-bool-value", "true")));
  }

  {
    SCOPED_CRASH_KEY_NUMBER("category", "float-value", 0.5);
    EXPECT_THAT(data(), ElementsAre(Pair("category-float-value", "0.5")));
  }

  {
    SCOPED_CRASH_KEY_NUMBER("category", "int-value", 1);
    EXPECT_THAT(data(), ElementsAre(Pair("category-int-value", "1")));
  }

  {
    SCOPED_CRASH_KEY_STRING32("category", "string32-value", "餅");
    EXPECT_THAT(data(), ElementsAre(Pair("category-string32-value", "餅")));
  }

  {
    SCOPED_CRASH_KEY_STRING64("category", "string64-value", "餅");
    EXPECT_THAT(data(), ElementsAre(Pair("category-string64-value", "餅")));
  }

  {
    SCOPED_CRASH_KEY_STRING256("category", "string256-value", "餅");
    EXPECT_THAT(data(), ElementsAre(Pair("category-string256-value", "餅")));
  }

  {
    SCOPED_CRASH_KEY_STRING1024("category", "string1024-value", "餅");
    EXPECT_THAT(data(), ElementsAre(Pair("category-string1024-value", "餅")));
  }
}

// Test that the helper macros properly uniqify the internal variable used for
// the scoper.
TEST_F(CrashLoggingTest, MultipleCrashKeysInSameScope) {
  SCOPED_CRASH_KEY_BOOL("category", "bool-value", false);
  SCOPED_CRASH_KEY_NUMBER("category", "int-value", 1);

  EXPECT_THAT(data(), ElementsAre(Pair("category-bool-value", "false"),
                                  Pair("category-int-value", "1")));

  std::ostringstream stream;
  OutputCrashKeysToStream(stream);
  EXPECT_EQ("Got 2 crash keys.", stream.str());
}

}  // namespace debug
}  // namespace base
