// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_aggregator.h"

#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Property;
using ::testing::VariantWith;

using MockSearchResultsCallback =
    ::testing::MockFunction<PickerViewDelegate::SearchResultsCallback>;

constexpr base::TimeDelta kBurnInPeriod = base::Milliseconds(400);

// Matcher for the last element of a collection.
MATCHER_P(LastElement, matcher, "") {
  return !arg.empty() &&
         ExplainMatchResult(matcher, arg.back(), result_listener);
}

struct TestCase {
  PickerSearchSource source;
  PickerSectionType section_type;
};

class PickerSearchAggregatorTest : public testing::TestWithParam<TestCase> {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

INSTANTIATE_TEST_SUITE_P(
    ,
    PickerSearchAggregatorTest,
    testing::Values(
        TestCase{
            .source = PickerSearchSource::kOmnibox,
            .section_type = PickerSectionType::kLinks,
        },
        TestCase{
            .source = PickerSearchSource::kTenor,
            .section_type = PickerSectionType::kGifs,
        },
        TestCase{
            .source = PickerSearchSource::kEmoji,
            .section_type = PickerSectionType::kExpressions,
        },
        TestCase{
            .source = PickerSearchSource::kDate,
            .section_type = PickerSectionType::kSuggestions,
        },
        TestCase{
            .source = PickerSearchSource::kCategory,
            .section_type = PickerSectionType::kCategories,
        },
        TestCase{
            .source = PickerSearchSource::kLocalFile,
            .section_type = PickerSectionType::kFiles,
        },
        TestCase{
            .source = PickerSearchSource::kDrive,
            .section_type = PickerSectionType::kDriveFiles,
        },
        TestCase{
            .source = PickerSearchSource::kMath,
            .section_type = PickerSectionType::kSuggestions,
        }));

TEST_P(PickerSearchAggregatorTest, DoesNotPublishResultsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  PickerSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerSearchResult::Text(u"test")});
  task_environment().FastForwardBy(base::Milliseconds(99));
}

TEST_P(PickerSearchAggregatorTest,
       PublishesResultsInCorrectSectionAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           GetParam().section_type),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(Property(
                               "data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"test")))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerSearchResult::Text(u"test")});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_P(PickerSearchAggregatorTest, PublishesResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           GetParam().section_type),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(Property(
                               "data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"test")))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerSearchResult::Text(u"test")});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_P(PickerSearchAggregatorTest, DoNotPublishEmptySectionsAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(_)).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(Contains(Property("type", &PickerSearchResultsSection::type,
                                     GetParam().section_type))))
      .Times(0);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source, {});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_P(PickerSearchAggregatorTest, DoNotPublishEmptySectionsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(_)).Times(AnyNumber());
  EXPECT_CALL(search_results_callback,
              Call(Contains(Property("type", &PickerSearchResultsSection::type,
                                     GetParam().section_type))))
      .Times(0);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);

  aggregator.HandleSearchSourceResults(GetParam().source, {});
}

class PickerSearchAggregatorMultipleSourcesTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       PublishesEmptySectionsIfNoResultsCameBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       PublishesEmptySectionsIfOnlyEmptyResultsCameBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox, {});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor, {});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEmoji, {});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDate, {});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kCategory, {});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLocalFile, {});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive, {});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kMath, {});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       CombinesSearchResultsInNewOrderBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(
          AllOf(
              Property("type", &PickerSearchResultsSection::type,
                       PickerSectionType::kSuggestions),
              Property(
                  "results", &PickerSearchResultsSection::results,
                  ElementsAre(
                      Property("data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"date"))),
                      Property("data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"math")))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kCategories),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"category")))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kExpressions),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"emoji")))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kLinks),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"omnibox")))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kFiles),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"local")))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kDriveFiles),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"drive")))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kGifs),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"tenor")))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                       {PickerSearchResult::Text(u"omnibox")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor,
                                       {PickerSearchResult::Text(u"tenor")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEmoji,
                                       {PickerSearchResult::Text(u"emoji")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDate,
                                       {PickerSearchResult::Text(u"date")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kCategory,
                                       {PickerSearchResult::Text(u"category")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLocalFile,
                                       {PickerSearchResult::Text(u"local")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerSearchResult::Text(u"drive")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kMath,
                                       {PickerSearchResult::Text(u"math")});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       AppendsSearchResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);
  // Suggested section do not appear post burn-in.
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kLinks),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(Property(
                               "data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"omnibox")))))))))
      .Times(1);
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kDriveFiles),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(Property(
                               "data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"drive")))))))))
      .Times(1);
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kGifs),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(Property(
                               "data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"tenor")))))))))
      .Times(1);
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kExpressions),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(Property(
                               "data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"emoji")))))))))
      .Times(1);
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kCategories),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(Property(
                               "data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"category")))))))))
      .Times(1);
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kFiles),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(Property(
                               "data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"local")))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                       {PickerSearchResult::Text(u"omnibox")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerSearchResult::Text(u"drive")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor,
                                       {PickerSearchResult::Text(u"tenor")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEmoji,
                                       {PickerSearchResult::Text(u"emoji")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDate,
                                       {PickerSearchResult::Text(u"date")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kCategory,
                                       {PickerSearchResult::Text(u"category")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLocalFile,
                                       {PickerSearchResult::Text(u"local")});
  aggregator.HandleSearchSourceResults(PickerSearchSource::kMath,
                                       {PickerSearchResult::Text(u"math")});
}

}  // namespace
}  // namespace ash
