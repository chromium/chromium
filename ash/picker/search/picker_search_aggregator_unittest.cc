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

// Gifs are tested separately since they have a special dependency on Drive
// results.
INSTANTIATE_TEST_SUITE_P(
    ,
    PickerSearchAggregatorTest,
    testing::Values(
        TestCase{
            .source = PickerSearchSource::kOmnibox,
            .section_type = PickerSectionType::kLinks,
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
        },
        TestCase{
            .source = PickerSearchSource::kEditorWrite,
            .section_type = PickerSectionType::kEditorWrite,
        },
        TestCase{
            .source = PickerSearchSource::kEditorRewrite,
            .section_type = PickerSectionType::kEditorRewrite,
        }));

TEST_P(PickerSearchAggregatorTest, DoesNotPublishResultsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  PickerSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerSearchResult::Text(u"test")},
                                       /*has_more_results=*/false);
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
                                       {PickerSearchResult::Text(u"test")},
                                       /*has_more_results=*/false);
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
                                       {PickerSearchResult::Text(u"test")},
                                       /*has_more_results=*/false);
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

  aggregator.HandleSearchSourceResults(GetParam().source, {},
                                       /*has_more_results=*/false);
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

  aggregator.HandleSearchSourceResults(GetParam().source, {},
                                       /*has_more_results=*/false);
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

  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEmoji, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDate, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kCategory, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLocalFile, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kMath, {},
                                       /*has_more_results=*/false);
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
                         PickerSectionType::kEditorWrite),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"write")))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kEditorRewrite),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"rewrite")))))),
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
                                       {PickerSearchResult::Text(u"omnibox")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor,
                                       {PickerSearchResult::Text(u"tenor")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEmoji,
                                       {PickerSearchResult::Text(u"emoji")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDate,
                                       {PickerSearchResult::Text(u"date")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kCategory,
                                       {PickerSearchResult::Text(u"category")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLocalFile,
                                       {PickerSearchResult::Text(u"local")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerSearchResult::Text(u"drive")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kMath,
                                       {PickerSearchResult::Text(u"math")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEditorWrite,
                                       {PickerSearchResult::Text(u"write")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEditorRewrite,
                                       {PickerSearchResult::Text(u"rewrite")},
                                       /*has_more_results=*/false);
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
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kEditorWrite),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(Property(
                               "data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"write")))))))))
      .Times(1);
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kEditorRewrite),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(Property(
                               "data", &PickerSearchResult::data,
                               VariantWith<PickerSearchResult::TextData>(Field(
                                   "primary_text",
                                   &PickerSearchResult::TextData::primary_text,
                                   u"rewrite")))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                       {PickerSearchResult::Text(u"omnibox")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerSearchResult::Text(u"drive")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor,
                                       {PickerSearchResult::Text(u"tenor")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEmoji,
                                       {PickerSearchResult::Text(u"emoji")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDate,
                                       {PickerSearchResult::Text(u"date")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kCategory,
                                       {PickerSearchResult::Text(u"category")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLocalFile,
                                       {PickerSearchResult::Text(u"local")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kMath,
                                       {PickerSearchResult::Text(u"math")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEditorWrite,
                                       {PickerSearchResult::Text(u"write")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEditorRewrite,
                                       {PickerSearchResult::Text(u"rewrite")},
                                       /*has_more_results=*/false);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       CombinesSearchResultsRetainingHasMoreResultsBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback,
              Call(Each(Property("has_more_results",
                                 &PickerSearchResultsSection::has_more_results,
                                 true))));

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                       {PickerSearchResult::Text(u"omnibox")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor,
                                       {PickerSearchResult::Text(u"tenor")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEmoji,
                                       {PickerSearchResult::Text(u"emoji")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLocalFile,
                                       {PickerSearchResult::Text(u"local")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerSearchResult::Text(u"drive")},
                                       /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       AppendsSearchResultsRetainingSeeMoreResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(Each(Property("has_more_results",
                         &PickerSearchResultsSection::has_more_results, true))))
      .Times(5);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                       {PickerSearchResult::Text(u"omnibox")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor,
                                       {PickerSearchResult::Text(u"tenor")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEmoji,
                                       {PickerSearchResult::Text(u"emoji")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLocalFile,
                                       {PickerSearchResult::Text(u"local")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerSearchResult::Text(u"drive")},
                                       /*has_more_results=*/true);
}

class PickerSearchAggregatorGifTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PickerSearchAggregatorGifTest,
       GifsAreNotPublishedWithoutDriveResultsAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor,
                                       {PickerSearchResult::Text(u"test")},
                                       /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorGifTest,
       GifsAreNotPublishedWithoutDriveResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor,
                                       {PickerSearchResult::Text(u"test")},
                                       /*has_more_results=*/true);
}

TEST_F(PickerSearchAggregatorGifTest,
       GifsPublishedAfterDriveResultsAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kDriveFiles),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(Property(
                             "data", &PickerSearchResult::data,
                             VariantWith<PickerSearchResult::TextData>(Field(
                                 "primary_text",
                                 &PickerSearchResult::TextData::primary_text,
                                 u"drive")))))),
          AllOf(
              Property("type", &PickerSearchResultsSection::type,
                       PickerSectionType::kGifs),
              Property("results", &PickerSearchResultsSection::results,
                       ElementsAre(Property(
                           "data", &PickerSearchResult::data,
                           VariantWith<PickerSearchResult::TextData>(Field(
                               "primary_text",
                               &PickerSearchResult::TextData::primary_text,
                               u"tenor"))))),
              Property("has_more_results",
                       &PickerSearchResultsSection::has_more_results, true)))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor,
                                       {PickerSearchResult::Text(u"tenor")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerSearchResult::Text(u"drive")},
                                       /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorGifTest,
       GifsPublishedAfterDriveResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);
  // Suggested section do not appear post burn-in.
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
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kGifs),
          Property("results", &PickerSearchResultsSection::results,
                   ElementsAre(
                       Property("data", &PickerSearchResult::data,
                                VariantWith<PickerSearchResult::TextData>(Field(
                                    "primary_text",
                                    &PickerSearchResult::TextData::primary_text,
                                    u"tenor"))))),
          Property("has_more_results",
                   &PickerSearchResultsSection::has_more_results, true)))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kTenor,
                                       {PickerSearchResult::Text(u"tenor")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerSearchResult::Text(u"drive")},
                                       /*has_more_results=*/true);
}

}  // namespace
}  // namespace ash
