// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_date_search.h"

#include <vector>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::AllOf;
using ::testing::Combine;
using ::testing::Each;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pointwise;
using ::testing::Property;
using ::testing::Values;
using ::testing::VariantWith;

base::Time TimeFromDateString(const std::string& time_string) {
  base::Time date;
  bool result = base::Time::FromString(time_string.c_str(), &date);
  CHECK(result);
  return date;
}

MATCHER(ResultMatchesDate, "") {
  const auto& [actual_result, expected_result] = arg;
  return ExplainMatchResult(
      AllOf(VariantWith<PickerTextResult>(Field("text",
                                                &PickerTextResult::primary_text,
                                                expected_result.primary_text)),
            VariantWith<PickerTextResult>(
                Field("text", &PickerTextResult::secondary_text,
                      expected_result.secondary_text)),
            VariantWith<PickerTextResult>(Field(
                "source", &PickerTextResult::source, expected_result.source))),
      actual_result, result_listener);
}

struct TestCase {
  std::string_view date;
  std::u16string_view query;
  std::vector<PickerTextResult> expected_results;
};

PickerTextResult MakeResult(std::u16string primary_text,
                            std::u16string secondary_text = u"") {
  return PickerTextResult(primary_text, secondary_text, ui::ImageModel(),
                          PickerTextResult::Source::kDate);
}

class PickerDateSearchTest
    : public ::testing::TestWithParam<std::tuple<std::string_view, TestCase>> {
};

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerDateSearchTest,
    Combine(Values("00:00", "12:00", "23:59"),
            Values(
                // No result
                TestCase{
                    .date = "23 Jan 2000",
                    .query = u"abc",
                    .expected_results = {},
                },
                // Today
                TestCase{
                    .date = "23 Jan 2000",
                    .query = u"today",
                    .expected_results = {MakeResult(u"Jan 23")},
                },
                // Yesterday
                TestCase{
                    .date = "23 Jan 2000",
                    .query = u"yesterday",
                    .expected_results = {MakeResult(u"Jan 22")},
                },
                // Tomorrow
                TestCase{
                    .date = "23 Jan 2000",
                    .query = u"tomorrow",
                    .expected_results = {MakeResult(u"Jan 24")},
                },
                // X days from now
                TestCase{
                    .date = "23 Jan 2000",
                    .query = u"10 days from now",
                    .expected_results = {MakeResult(u"Feb 2")},
                },
                // X days ago
                TestCase{
                    .date = "23 Jan 2000",
                    .query = u"five days ago",
                    .expected_results = {MakeResult(u"Jan 18")},
                },
                // X weeks from now
                TestCase{
                    .date = "23 Jan 2000",
                    .query = u"three weeks from now",
                    .expected_results = {MakeResult(u"Feb 13")},
                },
                // X weeks ago
                TestCase{
                    .date = "23 Jan 2000",
                    .query = u"2 weeks ago",
                    .expected_results = {MakeResult(u"Jan 9")},
                },
                // search for Friday on Tuesday
                TestCase{
                    .date = "19 Mar 2024",
                    .query = u"Friday",
                    .expected_results = {MakeResult(u"Mar 22")},
                },
                // search for this Friday on Tuesday
                TestCase{
                    .date = "19 Mar 2024",
                    .query = u"this Friday",
                    .expected_results = {MakeResult(u"Mar 22")},
                },
                // search for next Friday on Tuesday
                TestCase{
                    .date = "19 Mar 2024",
                    .query = u"next Friday",
                    .expected_results =
                        {MakeResult(u"Mar 29", u"Friday next week"),
                         MakeResult(u"Mar 22", u"This coming Friday")},
                },
                // search for last Friday on Tuesday
                TestCase{
                    .date = "19 Mar 2024",
                    .query = u"last Friday",
                    .expected_results = {MakeResult(u"Mar 15")},
                },
                // search for Tuesday on Friday
                TestCase{
                    .date = "22 Mar 2024",
                    .query = u"Tuesday",
                    .expected_results =
                        {MakeResult(u"Mar 26", u"This coming Tuesday"),
                         MakeResult(u"Mar 19", u"This past Tuesday")},
                },
                // search for this Tuesday on Friday
                TestCase{
                    .date = "22 Mar 2024",
                    .query = u"this Tuesday",
                    .expected_results =
                        {MakeResult(u"Mar 26", u"This coming Tuesday"),
                         MakeResult(u"Mar 19", u"This past Tuesday")},
                },
                // search for next Tuesday on Friday
                TestCase{
                    .date = "22 Mar 2024",
                    .query = u"next Tuesday",
                    .expected_results = {MakeResult(u"Mar 26")},
                },
                // search for last Tuesday on Friday
                TestCase{
                    .date = "22 Mar 2024",
                    .query = u"last Tuesday",
                    .expected_results =
                        {MakeResult(u"Mar 12", u"Tuesday last week"),
                         MakeResult(u"Mar 19", u"This past Tuesday")},
                },
                // search for Monday on Monday
                TestCase{
                    .date = "18 Mar 2024",
                    .query = u"Monday",
                    .expected_results = {MakeResult(u"Mar 18")},
                },
                // search for this Monday on Monday
                TestCase{
                    .date = "18 Mar 2024",
                    .query = u"this Monday",
                    .expected_results = {MakeResult(u"Mar 18")},
                },
                // search for next Monday on Monday
                TestCase{
                    .date = "18 Mar 2024",
                    .query = u"next Monday",
                    .expected_results = {MakeResult(u"Mar 25")},
                },
                // search for last Monday on Monday
                TestCase{
                    .date = "18 Mar 2024",
                    .query = u"last Monday",
                    .expected_results = {MakeResult(u"Mar 11")},
                })));

TEST_P(PickerDateSearchTest, ReturnsExpectedDates) {
  std::string_view time = std::get<0>(GetParam());
  const auto& [date, query, expected_results] = std::get<1>(GetParam());
  EXPECT_THAT(PickerDateSearch(
                  TimeFromDateString(base::StrCat({date, " ", time})), query),
              Pointwise(ResultMatchesDate(), expected_results));
}

TEST(PickerSuggestedDateResults, ReturnsSuggestedResults) {
  std::vector<PickerSearchResult> results = PickerSuggestedDateResults();
  EXPECT_THAT(results, Not(IsEmpty()));
  EXPECT_THAT(
      results,
      Each(VariantWith<PickerSearchRequestResult>(AllOf(
          Field("primary_text", &PickerSearchRequestResult::primary_text,
                Not(IsEmpty())),
          Field("secondary_text", &PickerSearchRequestResult::secondary_text,
                Not(IsEmpty()))))));
}
}  // namespace
}  // namespace ash
