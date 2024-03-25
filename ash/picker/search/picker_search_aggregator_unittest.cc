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

using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsSupersetOf;
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

class PickerSearchAggregatorTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PickerSearchAggregatorTest, DoesNotPublishResultsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(0);

  PickerSearchAggregator aggregator(
      /*burn_in_period=*/base::Milliseconds(100),
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(base::Milliseconds(99));
}

TEST_F(PickerSearchAggregatorTest, ShowGifResultsLast) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(LastElement(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kGifs),
          Property(
              "results", &PickerSearchResultsSection::results,
              Contains(Property(
                  "data", &PickerSearchResult::data,
                  VariantWith<PickerSearchResult::GifData>(AllOf(
                      Field("full_url", &PickerSearchResult::GifData::full_url,
                            Property("spec", &GURL::spec,
                                     "https://media.tenor.com/GOabrbLMl4AAAAAC/"
                                     "plink-cat-plink.gif")),
                      Field("content_description",
                            &PickerSearchResult::GifData::content_description,
                            u"cat blink"))))))))))
      .Times(AtLeast(1));

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kTenor,
      {ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(360, 360),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAC/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorTest, CombinesSearchResults) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(IsSupersetOf({
          AllOf(
              Property("type", &PickerSearchResultsSection::type,
                       PickerSectionType::kGifs),
              Property(
                  "results", &PickerSearchResultsSection::results,
                  Contains(Property(
                      "data", &PickerSearchResult::data,
                      VariantWith<PickerSearchResult::GifData>(AllOf(
                          Field("full_url",
                                &PickerSearchResult::GifData::full_url,
                                Property("spec", &GURL::spec,
                                         "https://media.tenor.com/"
                                         "GOabrbLMl4AAAAAC/"
                                         "plink-cat-plink.gif")),
                          Field(
                              "content_description",
                              &PickerSearchResult::GifData::content_description,
                              u"cat blink"))))))),
          AllOf(
              Property("type", &PickerSearchResultsSection::type,
                       PickerSectionType::kLinks),
              Property(
                  "results", &PickerSearchResultsSection::results,
                  ElementsAre(Property(
                      "data", &PickerSearchResult::data,
                      VariantWith<PickerSearchResult::BrowsingHistoryData>(
                          Field(
                              "url",
                              &PickerSearchResult::BrowsingHistoryData::url,
                              Property(
                                  "spec", &GURL::spec,
                                  "https://www.google.com/search?q=cat"))))))),
      })))
      .Times(AtLeast(1));

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kOmnibox,
      {ash::PickerSearchResult::BrowsingHistory(
          GURL("https://www.google.com/search?q=cat"), u"cat - Google Search",
          ui::ImageModel())});
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kTenor,
      {ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(360, 360),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAC/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorTest, DoNotShowEmptySectionsDuringBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Not(Contains(Property("type", &PickerSearchResultsSection::type,
                                 PickerSectionType::kLinks)))))
      .Times(AtLeast(1));

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox, {});
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kTenor,
      {ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(360, 360),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAC/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});
  task_environment().FastForwardBy(kBurnInPeriod);
}

TEST_F(PickerSearchAggregatorTest, DoNotShowEmptySectionsAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Not(Contains(Property("type", &PickerSearchResultsSection::type,
                                 PickerSectionType::kLinks)))))
      .Times(AtLeast(1));

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(PickerSearchSource::kOmnibox, {});
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kTenor,
      {ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(360, 360),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAC/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});
}

TEST_F(PickerSearchAggregatorTest, ShowGifResultsEvenAfterBurnIn) {
  MockSearchResultsCallback search_results_callback;
  EXPECT_CALL(search_results_callback, Call).Times(AnyNumber());
  EXPECT_CALL(
      search_results_callback,
      Call(Contains(AllOf(
          Property("type", &PickerSearchResultsSection::type,
                   PickerSectionType::kGifs),
          Property(
              "results", &PickerSearchResultsSection::results,
              Contains(Property(
                  "data", &PickerSearchResult::data,
                  VariantWith<PickerSearchResult::GifData>(AllOf(
                      Field("full_url", &PickerSearchResult::GifData::full_url,
                            Property("spec", &GURL::spec,
                                     "https://media.tenor.com/GOabrbLMl4AAAAAC/"
                                     "plink-cat-plink.gif")),
                      Field("content_description",
                            &PickerSearchResult::GifData::content_description,
                            u"cat blink"))))))))))
      .Times(AtLeast(1));

  PickerSearchAggregator aggregator(
      kBurnInPeriod,
      base::BindRepeating(&MockSearchResultsCallback::Call,
                          base::Unretained(&search_results_callback)));
  task_environment().FastForwardBy(kBurnInPeriod);
  aggregator.HandleSearchSourceResults(
      PickerSearchSource::kTenor,
      {ash::PickerSearchResult::Gif(
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAd/plink-cat-plink.gif"),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAe/plink-cat-plink.png"),
          gfx::Size(360, 360),
          GURL("https://media.tenor.com/GOabrbLMl4AAAAAC/plink-cat-plink.gif"),
          gfx::Size(480, 480), u"cat blink")});
}

}  // namespace
}  // namespace ash
