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
  auto reports = generator.Generate(data);
  ASSERT_EQ(1u, reports.size());

  EXPECT_EQ(data.type, reports[0]->feature_id());
  EXPECT_EQ(data.url.spec(), reports[0]->url());
  EXPECT_EQ(data.matched_url, reports[0]->allowlisted_url_match());
  EXPECT_EQ(data.filename, reports[0]->filename());
  EXPECT_EQ(data.column, reports[0]->column());
  EXPECT_EQ(data.line, reports[0]->line());
  base::Time midnight;
  ASSERT_TRUE(base::Time::FromUTCExploded(kTestDateInMidnight, &midnight));
  EXPECT_EQ(midnight.ToJavaTime(), reports[0]->event_timestamp_millis());
}

}  // namespace enterprise_reporting
