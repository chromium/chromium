// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/search/quick_insert_search_aggregator.h"

#include <optional>

#include "ash/quick_insert/model/quick_insert_search_results_section.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/search/quick_insert_search_source.h"
#include "ash/quick_insert/views/quick_insert_view_delegate.h"
#include "base/files/file_path.h"
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
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;

using MockSearchResultsCallback =
    ::testing::MockFunction<QuickInsertViewDelegate::SearchResultsCallback>;

constexpr base::TimeDelta kBurnInPeriod = base::Milliseconds(400);

// Matcher for the last element of a collection.
MATCHER_P(LastElement, matcher, "") {
  return !arg.empty() &&
         ExplainMatchResult(matcher, arg.back(), result_listener);
}

struct TestCase {
  QuickInsertSearchSource source;
  QuickInsertSectionType section_type;
};

class QuickInsertSearchAggregatorTest
    : public testing::TestWithParam<TestCase> {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

const TestCase kNamedSectionTestCases[] = {
    TestCase{
        .source = QuickInsertSearchSource::kOmnibox,
        .section_type = QuickInsertSectionType::kLinks,
    },
    TestCase{
        .source = QuickInsertSearchSource::kLocalFile,
        .section_type = QuickInsertSectionType::kLocalFiles,
    },
    TestCase{
        .source = QuickInsertSearchSource::kDrive,
        .section_type = QuickInsertSectionType::kDriveFiles,
    },
    TestCase{
        .source = QuickInsertSearchSource::kClipboard,
        .section_type = QuickInsertSectionType::kClipboard,
    },
};

const TestCase kNoneSectionTestCases[] = {
    TestCase{
        .source = QuickInsertSearchSource::kAction,
        .section_type = QuickInsertSectionType::kNone,
    },
    TestCase{
        .source = QuickInsertSearchSource::kDate,
        .section_type = QuickInsertSectionType::kNone,
    },
    TestCase{
        .source = QuickInsertSearchSource::kMath,
        .section_type = QuickInsertSectionType::kNone,
    },
};

const TestCase kContentEditorSectionTestCases[] = {
    TestCase{
        .source = QuickInsertSearchSource::kEditorWrite,
        .section_type = QuickInsertSectionType::kContentEditor,
    },
    TestCase{
        .source = QuickInsertSearchSource::kEditorRewrite,
        .section_type = QuickInsertSectionType::kContentEditor,
    },
    TestCase{
        .source = QuickInsertSearchSource::kLobsterWithNoSelectedText,
        .section_type = QuickInsertSectionType::kContentEditor,
    },
    TestCase{
        .source = QuickInsertSearchSource::kLobsterWithSelectedText,
        .section_type = QuickInsertSectionType::kContentEditor,
    },
};

INSTANTIATE_TEST_SUITE_P(NamedSections,
                         QuickInsertSearchAggregatorTest,
                         testing::ValuesIn(kNamedSectionTestCases));

INSTANTIATE_TEST_SUITE_P(NoneSections,
                         QuickInsertSearchAggregatorTest,
                         testing::ValuesIn(kNoneSectionTestCases));

INSTANTIATE_TEST_SUITE_P(ContentEditorSections,
                         QuickInsertSearchAggregatorTest,
                         testing::ValuesIn(kContentEditorSectionTestCases));

class QuickInsertSearchAggregatorNamedSectionTest
    : public QuickInsertSearchAggregatorTest {};

INSTANTIATE_TEST_SUITE_P(,
                         QuickInsertSearchAggregatorNamedSectionTest,
                         testing::ValuesIn(kNamedSectionTestCases));

TEST_P(QuickInsertSearchAggregatorTest, DoesNotPublishResultsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  QuickInsertSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {QuickInsertTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(base::Milliseconds(99));
}

TEST_P(QuickInsertSearchAggregatorTest,
       DoesNotPublishResultsDuringBurnInIfInterruptedNoMoreResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  QuickInsertSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {QuickInsertTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(base::Milliseconds(99));
  aggregator.HandleNoMoreResults(/*interrupted=*/true);
}

TEST_P(QuickInsertSearchAggregatorTest,
       ImmediatelyPublishesResultsDuringBurnInIfNoMoreResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(Property("type", &QuickInsertSearchResultsSection::type,
                                GetParam().section_type))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {QuickInsertTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(base::Milliseconds(99));
  aggregator.HandleNoMoreResults(/*interrupted=*/false);
}

TEST_P(QuickInsertSearchAggregatorTest,
       PublishesResultsInCorrectSectionAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   GetParam().section_type),
          Property("results", &QuickInsertSearchResultsSection::results,
                   ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                       "primary_text", &QuickInsertTextResult::primary_text,
                       u"test"))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {QuickInsertTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_P(QuickInsertSearchAggregatorTest, PublishesResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   GetParam().section_type),
          Property("results", &QuickInsertSearchResultsSection::results,
                   ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                       "primary_text", &QuickInsertTextResult::primary_text,
                       u"test"))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {QuickInsertTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_P(QuickInsertSearchAggregatorTest, DoNotPublishEmptySectionsAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(_)).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Contains(Property("type", &QuickInsertSearchResultsSection::type,
                             GetParam().section_type))))
      .Times(0);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source, {},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_P(QuickInsertSearchAggregatorTest, DoNotPublishEmptySectionsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(_)).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Contains(Property("type", &QuickInsertSearchResultsSection::type,
                             GetParam().section_type))))
      .Times(0);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);

  aggregator.HandleSearchSourceResults(GetParam().source, {},
                                       /*has_more_results=*/false);
}

TEST_P(QuickInsertSearchAggregatorTest, DoNotPublishEmptySearchAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source, {},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_P(QuickInsertSearchAggregatorTest, DoNotPublishEmptySearchPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);

  aggregator.HandleSearchSourceResults(GetParam().source, {},
                                       /*has_more_results=*/false);
}

TEST_P(QuickInsertSearchAggregatorTest,
       PublishesEmptyAfterResultsIfNoMoreResultsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(search_results_callback,
                Call(ElementsAre(
                    Property("type", &QuickInsertSearchResultsSection::type,
                             GetParam().section_type))))
        .Times(1);
    EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);
  }

  QuickInsertSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {QuickInsertTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(base::Milliseconds(99));
  aggregator.HandleNoMoreResults(/*interrupted=*/false);
}

TEST_P(QuickInsertSearchAggregatorTest,
       PublishesEmptyAfterResultsIfNoMoreResultsAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(search_results_callback,
                Call(ElementsAre(
                    Property("type", &QuickInsertSearchResultsSection::type,
                             GetParam().section_type))))
        .Times(1);
    EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);
  }

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {QuickInsertTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleNoMoreResults(/*interrupted=*/false);
}

// Results in the "none" section are never published post burn in, so don't test
// on those.
TEST_P(QuickInsertSearchAggregatorNamedSectionTest,
       PublishesEmptyAfterResultsIfNoMoreResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(search_results_callback,
                Call(ElementsAre(
                    Property("type", &QuickInsertSearchResultsSection::type,
                             GetParam().section_type))))
        .Times(1);
    EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);
  }

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {QuickInsertTextResult(u"test")},
                                       /*has_more_results=*/false);
  aggregator.HandleNoMoreResults(/*interrupted=*/false);
}

TEST_P(QuickInsertSearchAggregatorTest,
       DoesNotPublishEmptyAfterResultsIfInterruptedNoMoreResultsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(0);

  QuickInsertSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {QuickInsertTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(base::Milliseconds(99));
  aggregator.HandleNoMoreResults(/*interrupted=*/true);
}

TEST_P(QuickInsertSearchAggregatorTest,
       DoesNotPublishEmptyAfterResultsIfInterruptedNoMoreResultsAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(0);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {QuickInsertTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleNoMoreResults(/*interrupted=*/true);
}

TEST_P(QuickInsertSearchAggregatorTest,
       DoesNotPublishEmptyAfterResultsIfInterruptedNoMoreResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(0);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {QuickInsertTextResult(u"test")},
                                       /*has_more_results=*/false);
  aggregator.HandleNoMoreResults(/*interrupted=*/true);
}

class QuickInsertSearchAggregatorMultipleSourcesTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       PublishesEmptySectionsIfNoResultsCameBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(_)).Times(0);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       PublishesEmptySectionsIfOnlyEmptyResultsCameBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(_)).Times(0);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kOmnibox, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kDate, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kAction, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kLocalFile, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kDrive, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kMath, {},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       CombinesSearchResultsWithPredefinedTypeOrderBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(
          AllOf(Property("type", &QuickInsertSearchResultsSection::type,
                         QuickInsertSectionType::kNone),
                Property(
                    "results", &QuickInsertSearchResultsSection::results,
                    ElementsAre(
                        VariantWith<QuickInsertTextResult>(Field(
                            "primary_text",
                            &QuickInsertTextResult::primary_text, u"date")),
                        VariantWith<QuickInsertTextResult>(Field(
                            "primary_text",
                            &QuickInsertTextResult::primary_text, u"category")),
                        VariantWith<QuickInsertTextResult>(Field(
                            "primary_text",
                            &QuickInsertTextResult::primary_text, u"math"))))),
          AllOf(
              Property("type", &QuickInsertSearchResultsSection::type,
                       QuickInsertSectionType::kLinks),
              Property("results", &QuickInsertSearchResultsSection::results,
                       ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                           "primary_text", &QuickInsertTextResult::primary_text,
                           u"omnibox"))))),
          AllOf(
              Property("type", &QuickInsertSearchResultsSection::type,
                       QuickInsertSectionType::kDriveFiles),
              Property("results", &QuickInsertSearchResultsSection::results,
                       ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                           "primary_text", &QuickInsertTextResult::primary_text,
                           u"drive"))))),
          AllOf(Property("type", &QuickInsertSearchResultsSection::type,
                         QuickInsertSectionType::kLocalFiles),
                Property("results", &QuickInsertSearchResultsSection::results,
                         ElementsAre(VariantWith<QuickInsertLocalFileResult>(
                             Field("title", &QuickInsertLocalFileResult::title,
                                   u"local"))))),
          AllOf(Property("type", &QuickInsertSearchResultsSection::type,
                         QuickInsertSectionType::kClipboard),
                Property("results", &QuickInsertSearchResultsSection::results,
                         ElementsAre(VariantWith<QuickInsertClipboardResult>(
                             Field("display_text",
                                   &QuickInsertClipboardResult::display_text,
                                   u"clipboard"))))),
          AllOf(
              Property("type", &QuickInsertSearchResultsSection::type,
                       QuickInsertSectionType::kContentEditor),
              Property(
                  "results", &QuickInsertSearchResultsSection::results,
                  ElementsAre(
                      VariantWith<QuickInsertTextResult>(Field(
                          "primary_text", &QuickInsertTextResult::primary_text,
                          u"write")),
                      VariantWith<QuickInsertTextResult>(Field(
                          "primary_text", &QuickInsertTextResult::primary_text,
                          u"rewrite")),
                      VariantWith<QuickInsertTextResult>(Field(
                          "primary_text", &QuickInsertTextResult::primary_text,
                          u"lobster_with_no_selected_text")),
                      VariantWith<QuickInsertTextResult>(Field(
                          "primary_text", &QuickInsertTextResult::primary_text,
                          u"lobster_with_selected_text"))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kOmnibox,
                                       {QuickInsertTextResult(u"omnibox")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kClipboard,
      {QuickInsertClipboardResult(
          base::UnguessableToken::Create(),
          QuickInsertClipboardResult::DisplayFormat::kText,
          /*file_count=*/0, u"clipboard", std::nullopt,
          /*is_recent=*/false)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kDate,
                                       {QuickInsertTextResult(u"date")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kAction,
                                       {QuickInsertTextResult(u"category")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kLocalFile,
      {QuickInsertLocalFileResult(u"local", base::FilePath("fake_path"),
                                  /*best_match=*/false)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kDrive,
                                       {QuickInsertTextResult(u"drive")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kMath,
                                       {QuickInsertTextResult(u"math")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kEditorWrite,
                                       {QuickInsertTextResult(u"write")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kEditorRewrite,
                                       {QuickInsertTextResult(u"rewrite")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kLobsterWithNoSelectedText,
      {QuickInsertTextResult(u"lobster_with_no_selected_text")},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kLobsterWithSelectedText,
      {QuickInsertTextResult(u"lobster_with_selected_text")},
      /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       CombinesSearchResultsAndPromotesBestMatchBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(
          AllOf(Property("type", &QuickInsertSearchResultsSection::type,
                         QuickInsertSectionType::kLocalFiles),
                Property("results", &QuickInsertSearchResultsSection::results,
                         ElementsAre(VariantWith<QuickInsertLocalFileResult>(
                             Field("title", &QuickInsertLocalFileResult::title,
                                   u"local"))))),
          AllOf(
              Property("type", &QuickInsertSearchResultsSection::type,
                       QuickInsertSectionType::kLinks),
              Property("results", &QuickInsertSearchResultsSection::results,
                       ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                           "primary_text", &QuickInsertTextResult::primary_text,
                           u"omnibox"))))),
          AllOf(Property("type", &QuickInsertSearchResultsSection::type,
                         QuickInsertSectionType::kClipboard),
                Property("results", &QuickInsertSearchResultsSection::results,
                         ElementsAre(VariantWith<QuickInsertClipboardResult>(
                             Field("display_text",
                                   &QuickInsertClipboardResult::display_text,
                                   u"clipboard"))))),
          AllOf(
              Property("type", &QuickInsertSearchResultsSection::type,
                       QuickInsertSectionType::kContentEditor),
              Property("results", &QuickInsertSearchResultsSection::results,
                       ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                           "primary_text", &QuickInsertTextResult::primary_text,
                           u"write"))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kOmnibox,
                                       {QuickInsertTextResult(u"omnibox")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kClipboard,
      {QuickInsertClipboardResult(
          base::UnguessableToken::Create(),
          QuickInsertClipboardResult::DisplayFormat::kText,
          /*file_count=*/0, u"clipboard", std::nullopt,
          /*is_recent=*/false)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kLocalFile,
      {QuickInsertLocalFileResult(u"local", base::FilePath("fake_path"),
                                  /*best_match=*/true)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kEditorWrite,
                                       {QuickInsertTextResult(u"write")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       CombinesSearchResultsAndPromotesRecentClipboardBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(
          AllOf(Property("type", &QuickInsertSearchResultsSection::type,
                         QuickInsertSectionType::kLocalFiles),
                Property("results", &QuickInsertSearchResultsSection::results,
                         ElementsAre(VariantWith<QuickInsertLocalFileResult>(
                             Field("title", &QuickInsertLocalFileResult::title,
                                   u"local"))))),
          AllOf(Property("type", &QuickInsertSearchResultsSection::type,
                         QuickInsertSectionType::kClipboard),
                Property("results", &QuickInsertSearchResultsSection::results,
                         ElementsAre(VariantWith<QuickInsertClipboardResult>(
                             Field("display_text",
                                   &QuickInsertClipboardResult::display_text,
                                   u"clipboard"))))),
          AllOf(
              Property("type", &QuickInsertSearchResultsSection::type,
                       QuickInsertSectionType::kLinks),
              Property("results", &QuickInsertSearchResultsSection::results,
                       ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                           "primary_text", &QuickInsertTextResult::primary_text,
                           u"omnibox"))))),
          AllOf(
              Property("type", &QuickInsertSearchResultsSection::type,
                       QuickInsertSectionType::kContentEditor),
              Property("results", &QuickInsertSearchResultsSection::results,
                       ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                           "primary_text", &QuickInsertTextResult::primary_text,
                           u"write"))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kOmnibox,
                                       {QuickInsertTextResult(u"omnibox")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kClipboard,
      {QuickInsertClipboardResult(
          base::UnguessableToken::Create(),
          QuickInsertClipboardResult::DisplayFormat::kText,
          /*file_count=*/0, u"clipboard", std::nullopt,
          /*is_recent=*/true)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kLocalFile,
      {QuickInsertLocalFileResult(u"local", base::FilePath("fake_path"),
                                  /*best_match=*/true)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kEditorWrite,
                                       {QuickInsertTextResult(u"write")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       AppendsSearchResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(search_results_callback, Call(_)).Times(0);
  // Suggested section do not appear post burn-in.
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kLinks),
          Property("results", &QuickInsertSearchResultsSection::results,
                   ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                       "primary_text", &QuickInsertTextResult::primary_text,
                       u"omnibox"))))))))
      .Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kDriveFiles),
          Property("results", &QuickInsertSearchResultsSection::results,
                   ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                       "primary_text", &QuickInsertTextResult::primary_text,
                       u"drive"))))))))
      .Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kClipboard),
          Property("results", &QuickInsertSearchResultsSection::results,
                   ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                       "primary_text", &QuickInsertTextResult::primary_text,
                       u"clipboard"))))))))
      .Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kLocalFiles),
          Property("results", &QuickInsertSearchResultsSection::results,
                   ElementsAre(VariantWith<QuickInsertTextResult>(Field(
                       "primary_text", &QuickInsertTextResult::primary_text,
                       u"local"))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kOmnibox,
                                       {QuickInsertTextResult(u"omnibox")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kDrive,
                                       {QuickInsertTextResult(u"drive")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kDate,
                                       {QuickInsertTextResult(u"date")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kAction,
                                       {QuickInsertTextResult(u"category")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kClipboard,
                                       {QuickInsertTextResult(u"clipboard")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kLocalFile,
                                       {QuickInsertTextResult(u"local")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kMath,
                                       {QuickInsertTextResult(u"math")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kEditorWrite,
                                       {QuickInsertTextResult(u"write")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kEditorRewrite,
                                       {QuickInsertTextResult(u"rewrite")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kLobsterWithNoSelectedText,
      {QuickInsertTextResult(u"lobster")},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kLobsterWithSelectedText,
      {QuickInsertTextResult(u"lobster")},
      /*has_more_results=*/false);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       CombinesSearchResultsRetainingHasMoreResultsBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback,
              Call(Each(Property(
                  "has_more_results",
                  &QuickInsertSearchResultsSection::has_more_results, true))));

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kOmnibox,
                                       {QuickInsertTextResult(u"omnibox")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kLocalFile,
                                       {QuickInsertTextResult(u"local")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kDrive,
                                       {QuickInsertTextResult(u"drive")},
                                       /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       AppendsSearchResultsRetainingSeeMoreResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(search_results_callback, Call(_)).Times(0);
  EXPECT_CALL(search_results_callback,
              Call(Each(Property(
                  "has_more_results",
                  &QuickInsertSearchResultsSection::has_more_results, true))))
      .Times(3);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kOmnibox,
                                       {QuickInsertTextResult(u"omnibox")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kLocalFile,
                                       {QuickInsertTextResult(u"local")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(QuickInsertSearchSource::kDrive,
                                       {QuickInsertTextResult(u"drive")},
                                       /*has_more_results=*/true);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       PreBurnInLinksAreDeduplicatedWithPreBurnInDriveFilesWhichCameBefore) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(UnorderedElementsAre(
          AllOf(Property("type", &QuickInsertSearchResultsSection::type,
                         QuickInsertSectionType::kDriveFiles),
                Property(
                    "results", &QuickInsertSearchResultsSection::results,
                    ElementsAre(VariantWith<QuickInsertDriveFileResult>(
                                    Field("id", &QuickInsertDriveFileResult::id,
                                          std::nullopt)),
                                VariantWith<QuickInsertDriveFileResult>(
                                    Field("id", &QuickInsertDriveFileResult::id,
                                          Optional(Eq("driveid1")))),
                                VariantWith<QuickInsertDriveFileResult>(
                                    Field("id", &QuickInsertDriveFileResult::id,
                                          Optional(Eq("driveid2")))),
                                VariantWith<QuickInsertDriveFileResult>(
                                    Field("id", &QuickInsertDriveFileResult::id,
                                          Optional(Eq("driveid3"))))))),
          AllOf(
              Property("type", &QuickInsertSearchResultsSection::type,
                       QuickInsertSectionType::kLinks),
              Property("results", &QuickInsertSearchResultsSection::results,
                       ElementsAre(
                           VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                               "url", &QuickInsertBrowsingHistoryResult::url,
                               GURL("https://example.com"))),
                           VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                               "url", &QuickInsertBrowsingHistoryResult::url,
                               GURL("https://docs.google.com/notmatched"))),
                           VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                               "url", &QuickInsertBrowsingHistoryResult::url,
                               GURL("https://drive.google.com/"
                                    "notmatched")))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kDrive,
      {
          QuickInsertDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                     base::FilePath()),
      },
      /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kOmnibox,
      {
          QuickInsertBrowsingHistoryResult(GURL("https://example.com"), u"",
                                           ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/driveid2"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       PreBurnInLinksAreDeduplicatedWithPreBurnInDriveFilesWhichCameAfter) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(UnorderedElementsAre(
          AllOf(Property("type", &QuickInsertSearchResultsSection::type,
                         QuickInsertSectionType::kDriveFiles),
                Property(
                    "results", &QuickInsertSearchResultsSection::results,
                    ElementsAre(VariantWith<QuickInsertDriveFileResult>(
                                    Field("id", &QuickInsertDriveFileResult::id,
                                          std::nullopt)),
                                VariantWith<QuickInsertDriveFileResult>(
                                    Field("id", &QuickInsertDriveFileResult::id,
                                          Optional(Eq("driveid1")))),
                                VariantWith<QuickInsertDriveFileResult>(
                                    Field("id", &QuickInsertDriveFileResult::id,
                                          Optional(Eq("driveid2")))),
                                VariantWith<QuickInsertDriveFileResult>(
                                    Field("id", &QuickInsertDriveFileResult::id,
                                          Optional(Eq("driveid3"))))))),
          AllOf(
              Property("type", &QuickInsertSearchResultsSection::type,
                       QuickInsertSectionType::kLinks),
              Property("results", &QuickInsertSearchResultsSection::results,
                       ElementsAre(
                           VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                               "url", &QuickInsertBrowsingHistoryResult::url,
                               GURL("https://example.com"))),
                           VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                               "url", &QuickInsertBrowsingHistoryResult::url,
                               GURL("https://docs.google.com/notmatched"))),
                           VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                               "url", &QuickInsertBrowsingHistoryResult::url,
                               GURL("https://drive.google.com/"
                                    "notmatched")))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kOmnibox,
      {
          QuickInsertBrowsingHistoryResult(GURL("https://example.com"), u"",
                                           ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/driveid2"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kDrive,
      {
          QuickInsertDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                     base::FilePath()),
      },
      /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       PostBurnInLinksAreDeduplicatedWithPreBurnInDriveFiles) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kDriveFiles),
          Property("results", &QuickInsertSearchResultsSection::results,
                   ElementsAre(VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         std::nullopt)),
                               VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         Optional(Eq("driveid1")))),
                               VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         Optional(Eq("driveid2")))),
                               VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         Optional(Eq("driveid3"))))))))))
      .Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kLinks),
          Property(
              "results", &QuickInsertSearchResultsSection::results,
              ElementsAre(VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://example.com"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/notmatched"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://drive.google.com/"
                                   "notmatched")))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kDrive,
      {
          QuickInsertDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                     base::FilePath()),
      },
      /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kOmnibox,
      {
          QuickInsertBrowsingHistoryResult(GURL("https://example.com"), u"",
                                           ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/driveid2"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       PostBurnInLinksAreDeduplicatedWithPostBurnInDriveFilesWhichCameBefore) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kDriveFiles),
          Property("results", &QuickInsertSearchResultsSection::results,
                   ElementsAre(VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         std::nullopt)),
                               VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         Optional(Eq("driveid1")))),
                               VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         Optional(Eq("driveid2")))),
                               VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         Optional(Eq("driveid3"))))))))))
      .Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kLinks),
          Property(
              "results", &QuickInsertSearchResultsSection::results,
              ElementsAre(VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://example.com"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/notmatched"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://drive.google.com/"
                                   "notmatched")))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kDrive,
      {
          QuickInsertDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                     base::FilePath()),
      },
      /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kOmnibox,
      {
          QuickInsertBrowsingHistoryResult(GURL("https://example.com"), u"",
                                           ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/driveid2"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       PostBurnInDriveFilesAreDeduplicatedWithPreBurnInLinks) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kLinks),
          Property(
              "results", &QuickInsertSearchResultsSection::results,
              ElementsAre(VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://example.com"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/notmatched"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/driveid1"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/driveid1?edit"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://drive.google.com/driveid2"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://drive.google.com/"
                                   "notmatched")))))))))
      .Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kDriveFiles),
          Property("results", &QuickInsertSearchResultsSection::results,
                   ElementsAre(VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         std::nullopt)),
                               VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         Optional(Eq("driveid3"))))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kOmnibox,
      {
          QuickInsertBrowsingHistoryResult(GURL("https://example.com"), u"",
                                           ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/driveid2"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kDrive,
      {
          QuickInsertDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                     base::FilePath()),
      },
      /*has_more_results=*/true);
}

TEST_F(QuickInsertSearchAggregatorMultipleSourcesTest,
       PostBurnInDriveFilesAreDeduplicatedWithPostBurnInLinksWhichCameBefore) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kLinks),
          Property(
              "results", &QuickInsertSearchResultsSection::results,
              ElementsAre(VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://example.com"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/notmatched"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/driveid1"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/driveid1?edit"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://drive.google.com/driveid2"))),
                          VariantWith<QuickInsertBrowsingHistoryResult>(Field(
                              "url", &QuickInsertBrowsingHistoryResult::url,
                              GURL("https://drive.google.com/"
                                   "notmatched")))))))))
      .Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &QuickInsertSearchResultsSection::type,
                   QuickInsertSectionType::kDriveFiles),
          Property("results", &QuickInsertSearchResultsSection::results,
                   ElementsAre(VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         std::nullopt)),
                               VariantWith<QuickInsertDriveFileResult>(
                                   Field("id", &QuickInsertDriveFileResult::id,
                                         Optional(Eq("driveid3"))))))))))
      .Times(1);

  QuickInsertSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kOmnibox,
      {
          QuickInsertBrowsingHistoryResult(GURL("https://example.com"), u"",
                                           ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/driveid2"), u"", ui::ImageModel()),
          QuickInsertBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(
      QuickInsertSearchSource::kDrive,
      {
          QuickInsertDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                     base::FilePath()),
          QuickInsertDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                     base::FilePath()),
      },
      /*has_more_results=*/true);
}

}  // namespace
}  // namespace ash
