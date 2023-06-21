// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_reporting {

namespace {
base::Time::Exploded kTestDate = {
    2023, 5,  4,  4,  // May. 4, 2023, Thu
    22,   10, 15, 0   // 22:10:15.000
};

base::Time::Exploded kTestDateInMidnight = {
    2023, 5, 4, 4,  // May. 4, 2023, Thu
    0,    0, 0, 0   // 00:00:00.000
};

}  // namespace

class LegacyTechGeneratorTest : public ::testing::Test {
 public:
  LegacyTechGeneratorTest() = default;
  ~LegacyTechGeneratorTest() override = default;
};

TEST_F(LegacyTechGeneratorTest, Test) {
  LegacyTechReportGenerator::LegacyTechData data = {
      /*type=*/"type",
      /*timestamp=*/base::Time(),
      /*url=*/GURL("https://www.example.com/path"),
      /*matched_url=*/"www.example.com",
      /*filename=*/"filename.js",
      /*line=*/10,
      /*column=*/42};
  ASSERT_TRUE(base::Time::FromUTCExploded(kTestDate, &data.timestamp));

  LegacyTechReportGenerator generator;
  auto report = generator.Generate(data);

  EXPECT_EQ(data.type, report->feature_id());
  EXPECT_EQ(data.url.spec(), report->url());
  EXPECT_EQ(data.matched_url, report->allowlisted_url_match());
  EXPECT_EQ(data.filename, report->filename());
  EXPECT_EQ(data.column, report->column());
  EXPECT_EQ(data.line, report->line());
  base::Time midnight;
  ASSERT_TRUE(base::Time::FromUTCExploded(kTestDateInMidnight, &midnight));
  EXPECT_EQ(midnight.ToJavaTime(), report->event_timestamp_millis());
}

}  // namespace enterprise_reporting
