// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/test_stats_dictionary.h"

#include <memory>
#include <set>
#include <vector>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kTestStatsReportJson[] =
    R"({
  "GarbageA": {
    "id": "GarbageA",
    "timestamp": 0.0,
    "type": "garbage"
  },
  "RTCTestStatsID": {
    "id": "RTCTestStatsID",
    "timestamp": 13.37,
    "type": "test",
    "boolean": true,
    "number": 42,
    "string": "text",
    "sequenceBoolean": [ true ],
    "sequenceNumber": [ 42 ],
    "sequenceString": [ "text" ]
  },
  "GarbageB": {
    "id": "GarbageB",
    "timestamp": 0.0,
    "type": "garbage"
  }
})";

class TestStatsDictionaryTest : public testing::Test {
 public:
  TestStatsDictionaryTest() {
    std::unique_ptr<base::Value> value =
        base::JSONReader::ReadDeprecated(kTestStatsReportJson);
    CHECK(value);
    base::DictionaryValue* dictionary;
    CHECK(value->GetAsDictionary(&dictionary));
    ignore_result(value.release());
    report_ = new TestStatsReportDictionary(
        std::unique_ptr<base::DictionaryValue>(dictionary));
  }

 protected:
  scoped_refptr<TestStatsReportDictionary> report_;
};

TEST_F(TestStatsDictionaryTest, ReportGetStats) {
  EXPECT_FALSE(report_->Get("InvalidID"));
  EXPECT_TRUE(report_->Get("GarbageA"));
  EXPECT_TRUE(report_->Get("RTCTestStatsID"));
  EXPECT_TRUE(report_->Get("GarbageB"));
}

TEST_F(TestStatsDictionaryTest, ReportForEach) {
  std::set<std::string> remaining;
  remaining.insert("GarbageA");
  remaining.insert("RTCTestStatsID");
  remaining.insert("GarbageB");
  report_->ForEach([&remaining](const TestStatsDictionary& stats) {
    remaining.erase(stats.GetString("id"));
  });
  EXPECT_TRUE(remaining.empty());
}

TEST_F(TestStatsDictionaryTest, ReportFilterStats) {
  std::vector<TestStatsDictionary> filtered_stats = report_->Filter(
      [](const TestStatsDictionary& stats) -> bool {
          return false;
      });
  EXPECT_EQ(filtered_stats.size(), 0u);

  filtered_stats = report_->Filter(
      [](const TestStatsDictionary& stats) -> bool {
          return true;
      });
  EXPECT_EQ(filtered_stats.size(), 3u);

  filtered_stats = report_->Filter(
      [](const TestStatsDictionary& stats) -> bool {
          return stats.GetString("id") == "RTCTestStatsID";
      });
  EXPECT_EQ(filtered_stats.size(), 1u);
}

TEST_F(TestStatsDictionaryTest, ReportGetAll) {
  std::set<std::string> remaining;
  remaining.insert("GarbageA");
  remaining.insert("RTCTestStatsID");
  remaining.insert("GarbageB");
  for (const TestStatsDictionary& stats : report_->GetAll()) {
    remaining.erase(stats.GetString("id"));
  }
  EXPECT_TRUE(remaining.empty());
}

TEST_F(TestStatsDictionaryTest, ReportGetByType) {
  std::vector<TestStatsDictionary> stats = report_->GetByType("garbage");
  EXPECT_EQ(stats.size(), 2u);
  std::set<std::string> remaining;
  remaining.insert("GarbageA");
  remaining.insert("GarbageB");
  report_->ForEach([&remaining](const TestStatsDictionary& stats) {
    remaining.erase(stats.GetString("id"));
  });
  EXPECT_TRUE(remaining.empty());
}

TEST_F(TestStatsDictionaryTest, StatsVerifyMembers) {
  std::unique_ptr<TestStatsDictionary> stats = report_->Get("RTCTestStatsID");
  EXPECT_TRUE(stats);

  EXPECT_FALSE(stats->IsBoolean("nonexistentMember"));
  EXPECT_FALSE(stats->IsNumber("nonexistentMember"));
  EXPECT_FALSE(stats->IsString("nonexistentMember"));
  EXPECT_FALSE(stats->IsSequenceBoolean("nonexistentMember"));
  EXPECT_FALSE(stats->IsSequenceNumber("nonexistentMember"));
  EXPECT_FALSE(stats->IsSequenceString("nonexistentMember"));

  ASSERT_TRUE(stats->IsBoolean("boolean"));
  EXPECT_EQ(stats->GetBoolean("boolean"), true);

  ASSERT_TRUE(stats->IsNumber("number"));
  EXPECT_EQ(stats->GetNumber("number"), 42.0);

  ASSERT_TRUE(stats->IsString("string"));
  EXPECT_EQ(stats->GetString("string"), "text");

  ASSERT_TRUE(stats->IsSequenceBoolean("sequenceBoolean"));
  EXPECT_EQ(stats->GetSequenceBoolean("sequenceBoolean"),
            std::vector<bool> { true });

  ASSERT_TRUE(stats->IsSequenceNumber("sequenceNumber"));
  EXPECT_EQ(stats->GetSequenceNumber("sequenceNumber"),
            std::vector<double> { 42.0 });

  ASSERT_TRUE(stats->IsSequenceString("sequenceString"));
  EXPECT_EQ(stats->GetSequenceString("sequenceString"),
            std::vector<std::string> { "text" });
}

TEST_F(TestStatsDictionaryTest, TestStatsDictionaryShouldKeepReportAlive) {
  std::unique_ptr<TestStatsDictionary> stats = report_->Get("RTCTestStatsID");
  EXPECT_TRUE(stats);
  report_ = nullptr;
  EXPECT_EQ(stats->GetString("string"), "text");
}

}  // namespace

}  // namespace content
