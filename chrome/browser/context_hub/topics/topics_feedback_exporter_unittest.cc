// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/topics/topics_feedback_exporter.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/json/json_reader.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "components/history/core/browser/journeys/journey.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

namespace mojom = ::browser::context_hub::mojom;

using ::base::test::IsJson;
using ::testing::IsEmpty;

base::Time UtcTime(int year,
                   int month,
                   int day,
                   int hour,
                   int minute,
                   int second) {
  base::Time time;
  CHECK(base::Time::FromUTCExploded({.year = year,
                                     .month = month,
                                     .day_of_month = day,
                                     .hour = hour,
                                     .minute = minute,
                                     .second = second},
                                    &time));
  return time;
}

TopicsFeedbackHistoryVisit Visit(std::string_view url,
                                 std::u16string title,
                                 base::Time visit_time,
                                 bool is_foreign = false) {
  return {.url = GURL(url),
          .title = std::move(title),
          .visit_time = visit_time,
          .is_foreign = is_foreign};
}

history::journeys::Journey Journey(
    std::string id,
    std::vector<history::journeys::JourneyVisit> visits) {
  return history::journeys::Journey(
      std::move(id), "Title", base::Time::UnixEpoch(), std::nullopt,
      std::nullopt, std::nullopt, std::move(visits));
}

history::journeys::JourneyVisit JourneyVisit(std::string_view url,
                                             base::Time visit_time,
                                             bool is_foreign = false) {
  return history::journeys::JourneyVisit(GURL(url), u"", visit_time,
                                         is_foreign);
}

mojom::TopicsFeedbackExportOptionsPtr DefaultOptions() {
  auto options = mojom::TopicsFeedbackExportOptions::New();
  options->window_days = kDefaultTopicsFeedbackWindowDays;
  return options;
}

// Builds the bundle with a default context and returns its "visits" list.
base::ListValue BuildVisits(const TopicsFeedbackExportData& data,
                            const mojom::TopicsFeedbackExportOptions& options) {
  std::optional<base::DictValue> bundle = base::JSONReader::ReadDict(
      BuildTopicsFeedbackBundle(data, TopicsFeedbackExportContext(), options),
      base::JSON_PARSE_RFC);
  CHECK(bundle);
  base::ListValue* visits = bundle->FindList("visits");
  CHECK(visits);
  return std::move(*visits);
}

class TopicsFeedbackExporterTest : public testing::Test {
 protected:
  const base::Time kTime0 = UtcTime(2026, 9, 10, 8, 0, 0);
  const base::Time kTime1 =
      UtcTime(2026, 9, 22, 10, 1, 2) + base::Microseconds(123456);
  const base::Time kTime2 = UtcTime(2026, 9, 22, 10, 5, 0);
  const base::Time kTime3 = UtcTime(2026, 9, 23, 9, 0, 0);
};

TEST_F(TopicsFeedbackExporterTest, ClampWindowDays) {
  EXPECT_EQ(1, ClampTopicsFeedbackWindowDays(-3));
  EXPECT_EQ(1, ClampTopicsFeedbackWindowDays(0));
  EXPECT_EQ(5, ClampTopicsFeedbackWindowDays(5));
  EXPECT_EQ(14, ClampTopicsFeedbackWindowDays(14));
  EXPECT_EQ(14, ClampTopicsFeedbackWindowDays(30));
}

TEST_F(TopicsFeedbackExporterTest, FormatTime) {
  EXPECT_EQ("2026-09-22T10:01:02.123456Z", FormatTopicsFeedbackTime(kTime1));
  EXPECT_EQ("2026-09-22T10:05:00.000000Z", FormatTopicsFeedbackTime(kTime2));
}

TEST_F(TopicsFeedbackExporterTest, Preview_GroupsMinimizedVisitsByDomain) {
  TopicsFeedbackExportData data;
  data.journeys.push_back(
      Journey("topic", {JourneyVisit("https://example.com/a", kTime1)}));
  // Newest first. The fragment is dropped, so both example.com/a visits are
  // the same URL, titled after the newest visit.
  data.visits = {
      Visit("https://docs.google.com/d", u"Doc", kTime3),
      Visit("https://example.com/a#section", u"A (new)", kTime2),
      Visit("https://example.com/b", u"B", kTime2),
      Visit("https://example.com/a", u"A (old)", kTime1),
      Visit("chrome://settings", u"Settings", kTime1),
  };
  data.unresolvable_topics = 3;

  mojom::TopicsFeedbackExportPreviewPtr preview =
      BuildTopicsFeedbackExportPreview(data);

  EXPECT_EQ(1u, preview->stats->topics);
  EXPECT_EQ(4u, preview->stats->visits);
  // Only the kTime1 example.com/a visit is linked to the topic.
  EXPECT_EQ(3u, preview->stats->unclustered_visits);
  EXPECT_EQ(3u, preview->stats->unresolvable_topics);

  ASSERT_EQ(2u, preview->domains.size());
  const mojom::TopicsFeedbackExportDomain& example = *preview->domains[0];
  EXPECT_EQ("example.com", example.domain);
  EXPECT_EQ(3u, example.visit_count);
  EXPECT_FALSE(example.default_excluded);
  ASSERT_EQ(2u, example.urls.size());
  EXPECT_EQ(GURL("https://example.com/a"), example.urls[0]->url);
  EXPECT_EQ("A (new)", example.urls[0]->title);
  EXPECT_EQ(2u, example.urls[0]->visit_count);
  EXPECT_EQ(GURL("https://example.com/b"), example.urls[1]->url);
  EXPECT_EQ(1u, example.urls[1]->visit_count);

  const mojom::TopicsFeedbackExportDomain& docs = *preview->domains[1];
  EXPECT_EQ("docs.google.com", docs.domain);
  EXPECT_EQ(1u, docs.visit_count);
  EXPECT_TRUE(docs.default_excluded);
}

TEST_F(TopicsFeedbackExporterTest, Preview_Empty) {
  mojom::TopicsFeedbackExportPreviewPtr preview =
      BuildTopicsFeedbackExportPreview(TopicsFeedbackExportData());
  EXPECT_EQ(0u, preview->stats->topics);
  EXPECT_EQ(0u, preview->stats->visits);
  EXPECT_EQ(0u, preview->stats->unclustered_visits);
  EXPECT_THAT(preview->domains, IsEmpty());
}

TEST_F(TopicsFeedbackExporterTest, Bundle_LinksVisitsByTimeAndMinimizedUrl) {
  TopicsFeedbackExportData data;
  data.journeys.push_back(
      Journey("topic-1", {JourneyVisit("https://example.com/a#frag", kTime1),
                          JourneyVisit("https://example.com/b", kTime2)}));
  // A visit in two topics is linked to the first.
  data.journeys.push_back(
      Journey("topic-2", {JourneyVisit("https://example.com/b", kTime2),
                          JourneyVisit("https://example.com/c", kTime3)}));
  data.visits = {
      // Same URL as a topic visit, but a different time.
      Visit("https://example.com/c", u"", kTime2),
      Visit("https://example.com/b", u"", kTime2),
      // Same time as a topic visit, but a different URL.
      Visit("https://example.com/z", u"", kTime1),
      // Matches the topic visit once both fragments are dropped.
      Visit("https://example.com/a#other", u"", kTime1),
  };

  base::ListValue visits = BuildVisits(data, *DefaultOptions());

  ASSERT_EQ(4u, visits.size());
  EXPECT_EQ(nullptr, visits[0].GetDict().FindString("topic_id"));
  EXPECT_EQ("topic-1", *visits[1].GetDict().FindString("topic_id"));
  EXPECT_EQ(nullptr, visits[2].GetDict().FindString("topic_id"));
  EXPECT_EQ("topic-1", *visits[3].GetDict().FindString("topic_id"));

  mojom::TopicsFeedbackExportPreviewPtr preview =
      BuildTopicsFeedbackExportPreview(data);
  EXPECT_EQ(2u, preview->stats->unclustered_visits);
}

TEST_F(TopicsFeedbackExporterTest, Bundle_DropsNonWebVisitsAndMinimizesUrls) {
  TopicsFeedbackExportData data;
  data.visits = {
      Visit("chrome://settings", u"Settings", kTime3),
      Visit("file:///home/alice/notes.txt", u"notes", kTime3),
      Visit("data:text/html,hi", u"", kTime3),
      Visit("chrome-extension://abcdefghijklmnop/page.html", u"", kTime3),
      Visit("https://user:pass@example.com/p?q=cats&access_token=x&v=42#f",
            u"Cats", kTime2),
  };

  base::ListValue visits = BuildVisits(data, *DefaultOptions());

  ASSERT_EQ(1u, visits.size());
  EXPECT_EQ("https://example.com/p?q=cats&access_token=REDACTED&v=42",
            *visits[0].GetDict().FindString("url"));
}

TEST_F(TopicsFeedbackExporterTest, Bundle_RecordsDevice) {
  TopicsFeedbackExportData data;
  data.journeys.push_back(
      Journey("topic", {JourneyVisit("https://example.com/a", kTime1,
                                     /*is_foreign=*/true),
                        JourneyVisit("https://example.com/b", kTime2,
                                     /*is_foreign=*/false)}));
  data.visits = {
      Visit("https://example.com/b", u"", kTime2, /*is_foreign=*/false),
      Visit("https://example.com/a", u"", kTime1, /*is_foreign=*/true),
  };

  std::optional<base::DictValue> bundle = base::JSONReader::ReadDict(
      BuildTopicsFeedbackBundle(data, TopicsFeedbackExportContext(),
                                *DefaultOptions()),
      base::JSON_PARSE_RFC);
  ASSERT_TRUE(bundle);
  EXPECT_THAT(*bundle->FindList("visits"), IsJson(R"([
    {
      "t": "2026-09-22T10:05:00.000000Z",
      "url": "https://example.com/b",
      "title": "",
      "topic_id": "topic",
      "device": "local",
      "redacted": false
    },
    {
      "t": "2026-09-22T10:01:02.123456Z",
      "url": "https://example.com/a",
      "title": "",
      "topic_id": "topic",
      "device": "synced",
      "redacted": false
    }
  ])"));
  EXPECT_THAT(*(*bundle->FindList("topics"))[0].GetDict().FindList("visits"),
              IsJson(R"([
                {"t": "2026-09-22T10:01:02.123456Z", "device": "synced"},
                {"t": "2026-09-22T10:05:00.000000Z", "device": "local"}
              ])"));
}

TEST_F(TopicsFeedbackExporterTest, Bundle_ExcludedDomainsAreRedactedStubs) {
  TopicsFeedbackExportData data;
  data.journeys.push_back(
      Journey("topic", {JourneyVisit("https://docs.google.com/d", kTime3)}));
  data.visits = {
      Visit("https://docs.google.com/d", u"Secret doc", kTime3),
      Visit("https://mail.google.com/inbox", u"Inbox", kTime2,
            /*is_foreign=*/true),
      Visit("https://example.com/", u"Example", kTime1),
  };
  // mail.google.com is excluded by default but the rater opted it back in.
  auto options = DefaultOptions();
  options->excluded_domains = {"docs.google.com", "example.com"};

  EXPECT_THAT(BuildVisits(data, *options), IsJson(R"([
    {
      "t": "2026-09-23T09:00:00.000000Z",
      "topic_id": "topic",
      "device": "local",
      "redacted": true
    },
    {
      "t": "2026-09-22T10:05:00.000000Z",
      "url": "https://mail.google.com/inbox",
      "title": "Inbox",
      "topic_id": null,
      "device": "synced",
      "redacted": false
    },
    {
      "t": "2026-09-22T10:01:02.123456Z",
      "topic_id": null,
      "device": "local",
      "redacted": true
    }
  ])"));
}

TEST_F(TopicsFeedbackExporterTest, Bundle_ExcludedUrlsAreRedactedStubs) {
  TopicsFeedbackExportData data;
  data.visits = {
      Visit("https://example.com/private", u"Private", kTime3),
      Visit("https://example.com/public", u"Public", kTime2),
      Visit("https://example.com/private#again", u"Private", kTime1,
            /*is_foreign=*/true),
  };
  auto options = DefaultOptions();
  // Unminimized URLs are matched after minimization.
  options->excluded_urls = {GURL("https://example.com/private#frag")};

  EXPECT_THAT(BuildVisits(data, *options), IsJson(R"([
    {
      "t": "2026-09-23T09:00:00.000000Z",
      "topic_id": null,
      "device": "local",
      "redacted": true
    },
    {
      "t": "2026-09-22T10:05:00.000000Z",
      "url": "https://example.com/public",
      "title": "Public",
      "topic_id": null,
      "device": "local",
      "redacted": false
    },
    {
      "t": "2026-09-22T10:01:02.123456Z",
      "topic_id": null,
      "device": "synced",
      "redacted": true
    }
  ])"));
}

TEST_F(TopicsFeedbackExporterTest, Bundle_KeepsTitlesOfUnredactedVisits) {
  TopicsFeedbackExportData data;
  data.visits = {
      Visit("https://a.com/", u"A", kTime2),
      Visit("https://b.com/", u"B", kTime1),
  };

  base::ListValue visits = BuildVisits(data, *DefaultOptions());

  ASSERT_EQ(2u, visits.size());
  EXPECT_EQ("A", *visits[0].GetDict().FindString("title"));
  EXPECT_EQ("B", *visits[1].GetDict().FindString("title"));
}

TEST_F(TopicsFeedbackExporterTest, Bundle_FeedbackDuplicateOf) {
  TopicsFeedbackExportData data;
  auto feedback = mojom::TopicFeedback::New();
  feedback->id = "topic-1";
  feedback->snapshot = mojom::TopicSnapshot::New();
  feedback->snapshot->title = "Topic 1";
  feedback->snapshot->time_rated = kTime3;
  feedback->rating = mojom::TopicRating::kLiked;
  feedback->duplicate_of = mojom::DuplicateTopicSnapshot::New(
      "topic-2", "Topic 2", std::vector{kTime2});
  data.feedbacks.push_back(std::move(feedback));

  std::optional<base::DictValue> bundle = base::JSONReader::ReadDict(
      BuildTopicsFeedbackBundle(data, TopicsFeedbackExportContext(),
                                *DefaultOptions()),
      base::JSON_PARSE_RFC);
  ASSERT_TRUE(bundle);
  EXPECT_THAT(*bundle->FindListByDottedPath("feedback.topics"), IsJson(R"([{
    "id": "topic-1",
    "snapshot": {
      "title": "Topic 1",
      "emoji": "",
      "overview": "",
      "visit_times": [],
      "time_rated": "2026-09-23T09:00:00.000000Z"
    },
    "rating": "liked",
    "defects": [],
    "comment": "",
    "query_feedbacks": [],
    "rejected_visits": [],
    "duplicate_of": {
      "id": "topic-2",
      "title": "Topic 2",
      "visit_times": ["2026-09-22T10:05:00.000000Z"]
    }
  }])"));
}

TEST_F(TopicsFeedbackExporterTest, Bundle_MatchesSchema) {
  TopicsFeedbackExportData data;
  data.journeys.push_back(history::journeys::Journey(
      "topic-1", "Trip planning", kTime3, "E", "Long overview", std::nullopt,
      {JourneyVisit("https://example.com/a#frag", kTime1),
       JourneyVisit("https://other.com/x", kTime0, /*is_foreign=*/true)},
      {history::journeys::JourneyContinuationQuery("Query title",
                                                   "Query prompt")}));
  data.visits = {
      Visit("https://docs.google.com/doc", u"Doc", kTime3),
      Visit("https://news.com/?token=abc&q=1", u"News", kTime2,
            /*is_foreign=*/true),
      Visit("chrome://settings", u"Settings", kTime1),
      Visit("https://example.com/a", u"Page A", kTime1),
  };
  data.unresolvable_topics = 2;

  auto feedback = mojom::TopicFeedback::New();
  feedback->id = "topic-1";
  feedback->snapshot =
      mojom::TopicSnapshot::New("Trip planning", "E", "Long overview",
                                std::vector{kTime1, kTime0}, kTime3);
  feedback->rating = mojom::TopicRating::kDisliked;
  feedback->defects = {mojom::TopicDefectCategory::kTooBroad,
                       mojom::TopicDefectCategory::kOther,
                       mojom::TopicDefectCategory::kOutdated};
  feedback->comment = "Needs narrower scoping.";
  feedback->query_feedbacks.push_back(
      mojom::TopicQueryFeedback::New(0, "Query title", true));
  feedback->rejected_visits = {kTime0};
  data.feedbacks.push_back(std::move(feedback));

  TopicsFeedbackExportContext context;
  context.exported_at = UtcTime(2026, 9, 24, 21, 0, 0);
  context.window_start = UtcTime(2026, 9, 19, 21, 0, 0);
  context.window_days = 5;
  context.chrome_version = "155.0.8043.0";
  context.chrome_channel = "canary";
  context.finch = TopicsFeedbackExportContext::FinchGroup{
      .trial = "TopicsStudy", .group = "Enabled"};
  context.feature_params = {{"fishfood_feedback", "true"}};

  auto options = mojom::TopicsFeedbackExportOptions::New();
  options->window_days = 5;
  options->excluded_domains = {"docs.google.com"};
  options->missing_topics = "A topic about my garden.";

  EXPECT_THAT(BuildTopicsFeedbackBundle(data, context, *options), IsJson(R"({
    "schema_version": 1,
    "exported_at": "2026-09-24T21:00:00.000000Z",
    "chrome": {"version": "155.0.8043.0", "channel": "canary"},
    "feature_params": {"Topics": {"fishfood_feedback": "true"}},
    "finch": {"trial": "TopicsStudy", "group": "Enabled"},
    "window_days": 5,
    "window_start": "2026-09-19T21:00:00.000000Z",
    "redaction": {
      "excluded_domains_count": 1,
      "excluded_urls_count": 0
    },
    "stats": {
      "topics": 1,
      "visits": 3,
      "unclustered_visits": 2,
      "unresolvable_topics": 2
    },
    "topics": [{
      "id": "topic-1",
      "title": "Trip planning",
      "emoji": "E",
      "overview": "Long overview",
      "short_overview": "",
      "created_at": "2026-09-23T09:00:00.000000Z",
      "visits": [
        {"t": "2026-09-22T10:01:02.123456Z", "device": "local"},
        {"t": "2026-09-10T08:00:00.000000Z", "device": "synced"}
      ],
      "continuation_queries": [
        {"title": "Query title", "prompt": "Query prompt"}
      ]
    }],
    "visits": [
      {
        "t": "2026-09-23T09:00:00.000000Z",
        "topic_id": null,
        "device": "local",
        "redacted": true
      },
      {
        "t": "2026-09-22T10:05:00.000000Z",
        "url": "https://news.com/?token=REDACTED&q=1",
        "title": "News",
        "topic_id": null,
        "device": "synced",
        "redacted": false
      },
      {
        "t": "2026-09-22T10:01:02.123456Z",
        "url": "https://example.com/a",
        "title": "Page A",
        "topic_id": "topic-1",
        "device": "local",
        "redacted": false
      }
    ],
    "feedback": {
      "topics": [{
        "id": "topic-1",
        "snapshot": {
          "title": "Trip planning",
          "emoji": "E",
          "overview": "Long overview",
          "visit_times": [
            "2026-09-22T10:01:02.123456Z",
            "2026-09-10T08:00:00.000000Z"
          ],
          "time_rated": "2026-09-23T09:00:00.000000Z"
        },
        "rating": "disliked",
        "defects": ["too_broad", "other", "outdated"],
        "comment": "Needs narrower scoping.",
        "query_feedbacks": [
          {"index": 0, "query_text": "Query title", "liked": true}
        ],
        "rejected_visits": ["2026-09-10T08:00:00.000000Z"],
        "duplicate_of": null
      }],
      "missing_topics": "A topic about my garden."
    }
  })"));
}

TEST_F(TopicsFeedbackExporterTest, Bundle_NoFinchTrial) {
  std::optional<base::DictValue> bundle = base::JSONReader::ReadDict(
      BuildTopicsFeedbackBundle(TopicsFeedbackExportData(),
                                TopicsFeedbackExportContext(),
                                *DefaultOptions()),
      base::JSON_PARSE_RFC);
  ASSERT_TRUE(bundle);
  const base::Value* finch = bundle->Find("finch");
  ASSERT_TRUE(finch);
  EXPECT_TRUE(finch->is_none());
  EXPECT_THAT(*bundle->FindDictByDottedPath("feature_params.Topics"),
              IsJson("{}"));
}

}  // namespace
}  // namespace context_hub
