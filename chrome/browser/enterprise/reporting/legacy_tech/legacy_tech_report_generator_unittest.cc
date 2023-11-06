// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/legacy_tech/legacy_tech_report_generator.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_reporting {

namespace {

constexpr base::Time::Exploded kTestDate = {.year = 2023,
                                            .month = 5,
                                            .day_of_week = 4,
                                            .day_of_month = 4,
                                            .hour = 22,
                                            .minute = 10,
                                            .second = 15};

constexpr base::Time::Exploded kTestDateInMidnight = {.year = 2023,
                                                      .month = 5,
                                                      .day_of_week = 4,
                                                      .day_of_month = 4};

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
  EXPECT_EQ(midnight.InMillisecondsSinceUnixEpoch(),
            report->event_timestamp_millis());
}

}  // namespace enterprise_reporting
