// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/search/picker_search_aggregator.h"

#include <optional>

#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/search/picker_search_source.h"
#include "ash/picker/views/picker_view_delegate.h"
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

const TestCase kNamedSectionTestCases[] = {
    TestCase{
        .source = PickerSearchSource::kOmnibox,
        .section_type = PickerSectionType::kLinks,
    },
    TestCase{
        .source = PickerSearchSource::kLocalFile,
        .section_type = PickerSectionType::kLocalFiles,
    },
    TestCase{
        .source = PickerSearchSource::kDrive,
        .section_type = PickerSectionType::kDriveFiles,
    },
    TestCase{
        .source = PickerSearchSource::kClipboard,
        .section_type = PickerSectionType::kClipboard,
    },
};

const TestCase kNoneSectionTestCases[] = {
    TestCase{
        .source = PickerSearchSource::kAction,
        .section_type = PickerSectionType::kNone,
    },
    TestCase{
        .source = PickerSearchSource::kDate,
        .section_type = PickerSectionType::kNone,
    },
    TestCase{
        .source = PickerSearchSource::kMath,
        .section_type = PickerSectionType::kNone,
    },
};

const TestCase kContentEditorSectionTestCases[] = {
    TestCase{
        .source = PickerSearchSource::kEditorWrite,
        .section_type = PickerSectionType::kContentEditor,
    },
    TestCase{
        .source = PickerSearchSource::kEditorRewrite,
        .section_type = PickerSectionType::kContentEditor,
    },
    TestCase{
        .source = PickerSearchSource::kLobster,
        .section_type = PickerSectionType::kContentEditor,
    },
};

INSTANTIATE_TEST_SUITE_P(NamedSections,
                         PickerSearchAggregatorTest,
                         testing::ValuesIn(kNamedSectionTestCases));

INSTANTIATE_TEST_SUITE_P(NoneSections,
                         PickerSearchAggregatorTest,
                         testing::ValuesIn(kNoneSectionTestCases));

INSTANTIATE_TEST_SUITE_P(ContentEditorSections,
                         PickerSearchAggregatorTest,
                         testing::ValuesIn(kContentEditorSectionTestCases));

class PickerSearchAggregatorNamedSectionTest
    : public PickerSearchAggregatorTest {};

INSTANTIATE_TEST_SUITE_P(,
                         PickerSearchAggregatorNamedSectionTest,
                         testing::ValuesIn(kNamedSectionTestCases));

TEST_P(PickerSearchAggregatorTest, DoesNotPublishResultsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  PickerSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(base::Milliseconds(99));
}

TEST_P(PickerSearchAggregatorTest,
       DoesNotPublishResultsDuringBurnInIfInterruptedNoMoreResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  PickerSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(base::Milliseconds(99));
  aggregator.HandleNoMoreResults(/*interrupted=*/true);
}

TEST_P(PickerSearchAggregatorTest,
       ImmediatelyPublishesResultsDuringBurnInIfNoMoreResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(Property("type", &PickerSearchResultsSection::type,
                                GetParam().section_type))))
      .Times(1);

  PickerSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(base::Milliseconds(99));
  aggregator.HandleNoMoreResults(/*interrupted=*/false);
}

TEST_P(PickerSearchAggregatorTest,
       PublishesResultsInCorrectSectionAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           GetParam().section_type),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(VariantWith<PickerTextResult>(Field(
                               "primary_text", &PickerTextResult::primary_text,
                               u"test"))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerTextResult(u"test")},
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
                           ElementsAre(VariantWith<PickerTextResult>(Field(
                               "primary_text", &PickerTextResult::primary_text,
                               u"test"))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerTextResult(u"test")},
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

TEST_P(PickerSearchAggregatorTest, DoNotPublishEmptySearchAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source, {},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_P(PickerSearchAggregatorTest, DoNotPublishEmptySearchPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);

  aggregator.HandleSearchSourceResults(GetParam().source, {},
                                       /*has_more_results=*/false);
}

TEST_P(PickerSearchAggregatorTest,
       PublishesEmptyAfterResultsIfNoMoreResultsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(
        search_results_callback,
        Call(ElementsAre(Property("type", &PickerSearchResultsSection::type,
                                  GetParam().section_type))))
        .Times(1);
    EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);
  }

  PickerSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(base::Milliseconds(99));
  aggregator.HandleNoMoreResults(/*interrupted=*/false);
}

TEST_P(PickerSearchAggregatorTest,
       PublishesEmptyAfterResultsIfNoMoreResultsAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(
        search_results_callback,
        Call(ElementsAre(Property("type", &PickerSearchResultsSection::type,
                                  GetParam().section_type))))
        .Times(1);
    EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);
  }

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleNoMoreResults(/*interrupted=*/false);
}

// Results in the "none" section are never published post burn in, so don't test
// on those.
TEST_P(PickerSearchAggregatorNamedSectionTest,
       PublishesEmptyAfterResultsIfNoMoreResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  {
    ::testing::InSequence seq;
    EXPECT_CALL(
        search_results_callback,
        Call(ElementsAre(Property("type", &PickerSearchResultsSection::type,
                                  GetParam().section_type))))
        .Times(1);
    EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(1);
  }

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerTextResult(u"test")},
                                       /*has_more_results=*/false);
  aggregator.HandleNoMoreResults(/*interrupted=*/false);
}

TEST_P(PickerSearchAggregatorTest,
       DoesNotPublishEmptyAfterResultsIfInterruptedNoMoreResultsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(0);

  PickerSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(base::Milliseconds(99));
  aggregator.HandleNoMoreResults(/*interrupted=*/true);
}

TEST_P(PickerSearchAggregatorTest,
       DoesNotPublishEmptyAfterResultsIfInterruptedNoMoreResultsAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(0);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerTextResult(u"test")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleNoMoreResults(/*interrupted=*/true);
}

TEST_P(PickerSearchAggregatorTest,
       DoesNotPublishEmptyAfterResultsIfInterruptedNoMoreResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(search_results_callback, Call(IsEmpty())).Times(0);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(GetParam().source,
                                       {PickerTextResult(u"test")},
                                       /*has_more_results=*/false);
  aggregator.HandleNoMoreResults(/*interrupted=*/true);
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
  EXPECT_CALL(search_results_callback, Call(_)).Times(0);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       PublishesEmptySectionsIfOnlyEmptyResultsCameBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call(_)).Times(0);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDate, {},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kAction, {},
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
       CombinesSearchResultsWithPredefinedTypeOrderBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kNone),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(
                             VariantWith<PickerTextResult>(Field(
                                 "primary_text",
                                 &PickerTextResult::primary_text, u"date")),
                             VariantWith<PickerTextResult>(Field(
                                 "primary_text",
                                 &PickerTextResult::primary_text, u"category")),
                             VariantWith<PickerTextResult>(Field(
                                 "primary_text",
                                 &PickerTextResult::primary_text, u"math"))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kLinks),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(VariantWith<PickerTextResult>(Field(
                             "primary_text", &PickerTextResult::primary_text,
                             u"omnibox"))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kDriveFiles),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(VariantWith<PickerTextResult>(Field(
                             "primary_text", &PickerTextResult::primary_text,
                             u"drive"))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kLocalFiles),
                Property(
                    "results", &PickerSearchResultsSection::results,
                    ElementsAre(VariantWith<PickerLocalFileResult>(Field(
                        "title", &PickerLocalFileResult::title, u"local"))))),
          AllOf(
              Property("type", &PickerSearchResultsSection::type,
                       PickerSectionType::kClipboard),
              Property("results", &PickerSearchResultsSection::results,
                       ElementsAre(VariantWith<PickerClipboardResult>(Field(
                           "display_text", &PickerClipboardResult::display_text,
                           u"clipboard"))))),
          AllOf(
              Property("type", &PickerSearchResultsSection::type,
                       PickerSectionType::kContentEditor),
              Property("results", &PickerSearchResultsSection::results,
                       ElementsAre(
                           VariantWith<PickerTextResult>(Field(
                               "primary_text", &PickerTextResult::primary_text,
                               u"write")),
                           VariantWith<PickerTextResult>(Field(
                               "primary_text", &PickerTextResult::primary_text,
                               u"rewrite")),
                           VariantWith<PickerTextResult>(Field(
                               "primary_text", &PickerTextResult::primary_text,
                               u"lobster"))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                       {PickerTextResult(u"omnibox")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kClipboard,
      {PickerClipboardResult(base::UnguessableToken::Create(),
                             PickerClipboardResult::DisplayFormat::kText,
                             /*file_count=*/0, u"clipboard", std::nullopt,
                             /*is_recent=*/false)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDate,
                                       {PickerTextResult(u"date")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kAction,
                                       {PickerTextResult(u"category")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kLocalFile,
      {PickerLocalFileResult(u"local", base::FilePath("fake_path"),
                             /*best_match=*/false)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerTextResult(u"drive")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kMath,
                                       {PickerTextResult(u"math")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEditorWrite,
                                       {PickerTextResult(u"write")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEditorRewrite,
                                       {PickerTextResult(u"rewrite")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLobster,
                                       {PickerTextResult(u"lobster")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       CombinesSearchResultsAndPromotesBestMatchBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kLocalFiles),
                Property(
                    "results", &PickerSearchResultsSection::results,
                    ElementsAre(VariantWith<PickerLocalFileResult>(Field(
                        "title", &PickerLocalFileResult::title, u"local"))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kLinks),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(VariantWith<PickerTextResult>(Field(
                             "primary_text", &PickerTextResult::primary_text,
                             u"omnibox"))))),
          AllOf(
              Property("type", &PickerSearchResultsSection::type,
                       PickerSectionType::kClipboard),
              Property("results", &PickerSearchResultsSection::results,
                       ElementsAre(VariantWith<PickerClipboardResult>(Field(
                           "display_text", &PickerClipboardResult::display_text,
                           u"clipboard"))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kContentEditor),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(VariantWith<PickerTextResult>(Field(
                             "primary_text", &PickerTextResult::primary_text,
                             u"write"))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                       {PickerTextResult(u"omnibox")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kClipboard,
      {PickerClipboardResult(base::UnguessableToken::Create(),
                             PickerClipboardResult::DisplayFormat::kText,
                             /*file_count=*/0, u"clipboard", std::nullopt,
                             /*is_recent=*/false)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kLocalFile,
      {PickerLocalFileResult(u"local", base::FilePath("fake_path"),
                             /*best_match=*/true)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEditorWrite,
                                       {PickerTextResult(u"write")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       CombinesSearchResultsAndPromotesRecentClipboardBeforeBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kLocalFiles),
                Property(
                    "results", &PickerSearchResultsSection::results,
                    ElementsAre(VariantWith<PickerLocalFileResult>(Field(
                        "title", &PickerLocalFileResult::title, u"local"))))),
          AllOf(
              Property("type", &PickerSearchResultsSection::type,
                       PickerSectionType::kClipboard),
              Property("results", &PickerSearchResultsSection::results,
                       ElementsAre(VariantWith<PickerClipboardResult>(Field(
                           "display_text", &PickerClipboardResult::display_text,
                           u"clipboard"))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kLinks),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(VariantWith<PickerTextResult>(Field(
                             "primary_text", &PickerTextResult::primary_text,
                             u"omnibox"))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kContentEditor),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(VariantWith<PickerTextResult>(Field(
                             "primary_text", &PickerTextResult::primary_text,
                             u"write"))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                       {PickerTextResult(u"omnibox")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kClipboard,
      {PickerClipboardResult(base::UnguessableToken::Create(),
                             PickerClipboardResult::DisplayFormat::kText,
                             /*file_count=*/0, u"clipboard", std::nullopt,
                             /*is_recent=*/true)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kLocalFile,
      {PickerLocalFileResult(u"local", base::FilePath("fake_path"),
                             /*best_match=*/true)},
      /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEditorWrite,
                                       {PickerTextResult(u"write")},
                                       /*has_more_results=*/false);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       AppendsSearchResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(search_results_callback, Call(_)).Times(0);
  // Suggested section do not appear post burn-in.
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kLinks),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(VariantWith<PickerTextResult>(Field(
                               "primary_text", &PickerTextResult::primary_text,
                               u"omnibox"))))))))
      .Times(1);
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kDriveFiles),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(VariantWith<PickerTextResult>(Field(
                               "primary_text", &PickerTextResult::primary_text,
                               u"drive"))))))))
      .Times(1);
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kClipboard),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(VariantWith<PickerTextResult>(Field(
                               "primary_text", &PickerTextResult::primary_text,
                               u"clipboard"))))))))
      .Times(1);
  EXPECT_CALL(search_results_callback,
              Call(ElementsAre(AllOf(
                  Property("type", &PickerSearchResultsSection::type,
                           PickerSectionType::kLocalFiles),
                  Property("results", &PickerSearchResultsSection::results,
                           ElementsAre(VariantWith<PickerTextResult>(Field(
                               "primary_text", &PickerTextResult::primary_text,
                               u"local"))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                       {PickerTextResult(u"omnibox")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerTextResult(u"drive")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDate,
                                       {PickerTextResult(u"date")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kAction,
                                       {PickerTextResult(u"category")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kClipboard,
                                       {PickerTextResult(u"clipboard")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLocalFile,
                                       {PickerTextResult(u"local")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kMath,
                                       {PickerTextResult(u"math")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEditorWrite,
                                       {PickerTextResult(u"write")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kEditorRewrite,
                                       {PickerTextResult(u"rewrite")},
                                       /*has_more_results=*/false);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLobster,
                                       {PickerTextResult(u"lobster")},
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
                                       {PickerTextResult(u"omnibox")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLocalFile,
                                       {PickerTextResult(u"local")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerTextResult(u"drive")},
                                       /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       AppendsSearchResultsRetainingSeeMoreResultsPostBurnIn) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(search_results_callback, Call(_)).Times(0);
  EXPECT_CALL(
      search_results_callback,
      Call(Each(Property("has_more_results",
                         &PickerSearchResultsSection::has_more_results, true))))
      .Times(3);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox,
                                       {PickerTextResult(u"omnibox")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kLocalFile,
                                       {PickerTextResult(u"local")},
                                       /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kDrive,
                                       {PickerTextResult(u"drive")},
                                       /*has_more_results=*/true);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       PreBurnInLinksAreDeduplicatedWithPreBurnInDriveFilesWhichCameBefore) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(UnorderedElementsAre(
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kDriveFiles),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(VariantWith<PickerDriveFileResult>(
                                         Field("id", &PickerDriveFileResult::id,
                                               std::nullopt)),
                                     VariantWith<PickerDriveFileResult>(
                                         Field("id", &PickerDriveFileResult::id,
                                               Optional(Eq("driveid1")))),
                                     VariantWith<PickerDriveFileResult>(
                                         Field("id", &PickerDriveFileResult::id,
                                               Optional(Eq("driveid2")))),
                                     VariantWith<PickerDriveFileResult>(
                                         Field("id", &PickerDriveFileResult::id,
                                               Optional(Eq("driveid3"))))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kLinks),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(
                             VariantWith<PickerBrowsingHistoryResult>(
                                 Field("url", &PickerBrowsingHistoryResult::url,
                                       GURL("https://example.com"))),
                             VariantWith<PickerBrowsingHistoryResult>(Field(
                                 "url", &PickerBrowsingHistoryResult::url,
                                 GURL("https://docs.google.com/notmatched"))),
                             VariantWith<PickerBrowsingHistoryResult>(
                                 Field("url", &PickerBrowsingHistoryResult::url,
                                       GURL("https://drive.google.com/"
                                            "notmatched")))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kDrive,
      {
          PickerDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                base::FilePath()),
      },
      /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kOmnibox,
      {
          PickerBrowsingHistoryResult(GURL("https://example.com"), u"",
                                      ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://docs.google.com/driveid1"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://drive.google.com/driveid2"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       PreBurnInLinksAreDeduplicatedWithPreBurnInDriveFilesWhichCameAfter) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(
      search_results_callback,
      Call(UnorderedElementsAre(
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kDriveFiles),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(VariantWith<PickerDriveFileResult>(
                                         Field("id", &PickerDriveFileResult::id,
                                               std::nullopt)),
                                     VariantWith<PickerDriveFileResult>(
                                         Field("id", &PickerDriveFileResult::id,
                                               Optional(Eq("driveid1")))),
                                     VariantWith<PickerDriveFileResult>(
                                         Field("id", &PickerDriveFileResult::id,
                                               Optional(Eq("driveid2")))),
                                     VariantWith<PickerDriveFileResult>(
                                         Field("id", &PickerDriveFileResult::id,
                                               Optional(Eq("driveid3"))))))),
          AllOf(Property("type", &PickerSearchResultsSection::type,
                         PickerSectionType::kLinks),
                Property("results", &PickerSearchResultsSection::results,
                         ElementsAre(
                             VariantWith<PickerBrowsingHistoryResult>(
                                 Field("url", &PickerBrowsingHistoryResult::url,
                                       GURL("https://example.com"))),
                             VariantWith<PickerBrowsingHistoryResult>(Field(
                                 "url", &PickerBrowsingHistoryResult::url,
                                 GURL("https://docs.google.com/notmatched"))),
                             VariantWith<PickerBrowsingHistoryResult>(
                                 Field("url", &PickerBrowsingHistoryResult::url,
                                       GURL("https://drive.google.com/"
                                            "notmatched")))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kOmnibox,
      {
          PickerBrowsingHistoryResult(GURL("https://example.com"), u"",
                                      ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://docs.google.com/driveid1"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://drive.google.com/driveid2"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kDrive,
      {
          PickerDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                base::FilePath()),
      },
      /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       PostBurnInLinksAreDeduplicatedWithPreBurnInDriveFiles) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kDriveFiles),
          Property(
              "results", &PickerSearchResultsSection::results,
              ElementsAre(VariantWith<PickerDriveFileResult>(Field(
                              "id", &PickerDriveFileResult::id, std::nullopt)),
                          VariantWith<PickerDriveFileResult>(
                              Field("id", &PickerDriveFileResult::id,
                                    Optional(Eq("driveid1")))),
                          VariantWith<PickerDriveFileResult>(
                              Field("id", &PickerDriveFileResult::id,
                                    Optional(Eq("driveid2")))),
                          VariantWith<PickerDriveFileResult>(
                              Field("id", &PickerDriveFileResult::id,
                                    Optional(Eq("driveid3"))))))))))
      .Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kLinks),
          Property("results", &PickerSearchResultsSection::results,
                   ElementsAre(VariantWith<PickerBrowsingHistoryResult>(Field(
                                   "url", &PickerBrowsingHistoryResult::url,
                                   GURL("https://example.com"))),
                               VariantWith<PickerBrowsingHistoryResult>(Field(
                                   "url", &PickerBrowsingHistoryResult::url,
                                   GURL("https://docs.google.com/notmatched"))),
                               VariantWith<PickerBrowsingHistoryResult>(Field(
                                   "url", &PickerBrowsingHistoryResult::url,
                                   GURL("https://drive.google.com/"
                                        "notmatched")))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kDrive,
      {
          PickerDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                base::FilePath()),
      },
      /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kOmnibox,
      {
          PickerBrowsingHistoryResult(GURL("https://example.com"), u"",
                                      ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://docs.google.com/driveid1"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://drive.google.com/driveid2"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       PostBurnInLinksAreDeduplicatedWithPostBurnInDriveFilesWhichCameBefore) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kDriveFiles),
          Property(
              "results", &PickerSearchResultsSection::results,
              ElementsAre(VariantWith<PickerDriveFileResult>(Field(
                              "id", &PickerDriveFileResult::id, std::nullopt)),
                          VariantWith<PickerDriveFileResult>(
                              Field("id", &PickerDriveFileResult::id,
                                    Optional(Eq("driveid1")))),
                          VariantWith<PickerDriveFileResult>(
                              Field("id", &PickerDriveFileResult::id,
                                    Optional(Eq("driveid2")))),
                          VariantWith<PickerDriveFileResult>(
                              Field("id", &PickerDriveFileResult::id,
                                    Optional(Eq("driveid3"))))))))))
      .Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kLinks),
          Property("results", &PickerSearchResultsSection::results,
                   ElementsAre(VariantWith<PickerBrowsingHistoryResult>(Field(
                                   "url", &PickerBrowsingHistoryResult::url,
                                   GURL("https://example.com"))),
                               VariantWith<PickerBrowsingHistoryResult>(Field(
                                   "url", &PickerBrowsingHistoryResult::url,
                                   GURL("https://docs.google.com/notmatched"))),
                               VariantWith<PickerBrowsingHistoryResult>(Field(
                                   "url", &PickerBrowsingHistoryResult::url,
                                   GURL("https://drive.google.com/"
                                        "notmatched")))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kDrive,
      {
          PickerDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                base::FilePath()),
      },
      /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kOmnibox,
      {
          PickerBrowsingHistoryResult(GURL("https://example.com"), u"",
                                      ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://docs.google.com/driveid1"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://drive.google.com/driveid2"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       PostBurnInDriveFilesAreDeduplicatedWithPreBurnInLinks) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kLinks),
          Property(
              "results", &PickerSearchResultsSection::results,
              ElementsAre(VariantWith<PickerBrowsingHistoryResult>(
                              Field("url", &PickerBrowsingHistoryResult::url,
                                    GURL("https://example.com"))),
                          VariantWith<PickerBrowsingHistoryResult>(Field(
                              "url", &PickerBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/notmatched"))),
                          VariantWith<PickerBrowsingHistoryResult>(
                              Field("url", &PickerBrowsingHistoryResult::url,
                                    GURL("https://docs.google.com/driveid1"))),
                          VariantWith<PickerBrowsingHistoryResult>(Field(
                              "url", &PickerBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/driveid1?edit"))),
                          VariantWith<PickerBrowsingHistoryResult>(
                              Field("url", &PickerBrowsingHistoryResult::url,
                                    GURL("https://drive.google.com/driveid2"))),
                          VariantWith<PickerBrowsingHistoryResult>(
                              Field("url", &PickerBrowsingHistoryResult::url,
                                    GURL("https://drive.google.com/"
                                         "notmatched")))))))))
      .Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kDriveFiles),
          Property(
              "results", &PickerSearchResultsSection::results,
              ElementsAre(VariantWith<PickerDriveFileResult>(Field(
                              "id", &PickerDriveFileResult::id, std::nullopt)),
                          VariantWith<PickerDriveFileResult>(
                              Field("id", &PickerDriveFileResult::id,
                                    Optional(Eq("driveid3"))))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kOmnibox,
      {
          PickerBrowsingHistoryResult(GURL("https://example.com"), u"",
                                      ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://docs.google.com/driveid1"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://drive.google.com/driveid2"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kDrive,
      {
          PickerDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                base::FilePath()),
      },
      /*has_more_results=*/true);
}

TEST_F(PickerSearchAggregatorMultipleSourcesTest,
       PostBurnInDriveFilesAreDeduplicatedWithPostBurnInLinksWhichCameBefore) {
  MockSearchResultsCallback search_results_callback;
  testing::InSequence seq;
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kLinks),
          Property(
              "results", &PickerSearchResultsSection::results,
              ElementsAre(VariantWith<PickerBrowsingHistoryResult>(
                              Field("url", &PickerBrowsingHistoryResult::url,
                                    GURL("https://example.com"))),
                          VariantWith<PickerBrowsingHistoryResult>(Field(
                              "url", &PickerBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/notmatched"))),
                          VariantWith<PickerBrowsingHistoryResult>(
                              Field("url", &PickerBrowsingHistoryResult::url,
                                    GURL("https://docs.google.com/driveid1"))),
                          VariantWith<PickerBrowsingHistoryResult>(Field(
                              "url", &PickerBrowsingHistoryResult::url,
                              GURL("https://docs.google.com/driveid1?edit"))),
                          VariantWith<PickerBrowsingHistoryResult>(
                              Field("url", &PickerBrowsingHistoryResult::url,
                                    GURL("https://drive.google.com/driveid2"))),
                          VariantWith<PickerBrowsingHistoryResult>(
                              Field("url", &PickerBrowsingHistoryResult::url,
                                    GURL("https://drive.google.com/"
                                         "notmatched")))))))))
      .Times(1);
  EXPECT_CALL(
      search_results_callback,
      Call(ElementsAre(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kDriveFiles),
          Property(
              "results", &PickerSearchResultsSection::results,
              ElementsAre(VariantWith<PickerDriveFileResult>(Field(
                              "id", &PickerDriveFileResult::id, std::nullopt)),
                          VariantWith<PickerDriveFileResult>(
                              Field("id", &PickerDriveFileResult::id,
                                    Optional(Eq("driveid3"))))))))))
      .Times(1);

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));

  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kOmnibox,
      {
          PickerBrowsingHistoryResult(GURL("https://example.com"), u"",
                                      ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/notmatched"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://docs.google.com/driveid1"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://docs.google.com/driveid1?edit"), u"",
              ui::ImageModel()),
          PickerBrowsingHistoryResult(GURL("https://drive.google.com/driveid2"),
                                      u"", ui::ImageModel()),
          PickerBrowsingHistoryResult(
              GURL("https://drive.google.com/notmatched"), u"",
              ui::ImageModel()),
      },
      /*has_more_results=*/true);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kDrive,
      {
          PickerDriveFileResult(/*id=*/std::nullopt, /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid1", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid2", /*title=*/u"", GURL(),
                                base::FilePath()),
          PickerDriveFileResult("driveid3", /*title=*/u"", GURL(),
                                base::FilePath()),
      },
      /*has_more_results=*/true);
}

}  // namespace
}  // namespace ash
