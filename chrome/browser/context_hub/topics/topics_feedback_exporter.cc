// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/topics/topics_feedback_exporter.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/context_hub/topics/topics_feedback_url_util.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "components/history/core/browser/journeys/journey.h"
#include "url/gurl.h"

namespace context_hub {

namespace {

namespace mojom = ::browser::context_hub::mojom;

// A history visit that survives minimization, linked to its Topic.
struct ExportedVisit {
  base::Time visit_time;
  // Minimized URL.
  GURL url;
  // Points into `TopicsFeedbackExportData::visits`.
  raw_ref<const std::u16string> title;
  bool is_foreign;
  // Id of the Topic the visit belongs to, or null if it is unclustered. Points
  // into `TopicsFeedbackExportData::journeys`.
  raw_ptr<const std::string> topic_id;
};

// Drops visits that must not be exported, minimizes the URLs of the rest, and
// links each to the first Topic with a visit at the same time to the same
// minimized URL.
std::vector<ExportedVisit> MinimizeAndLinkVisits(
    const TopicsFeedbackExportData& data) {
  std::map<std::pair<base::Time, std::string>, const std::string*>
      topic_id_by_visit;
  for (const history::journeys::Journey& journey : data.journeys) {
    for (const history::journeys::JourneyVisit& visit : journey.visits) {
      if (std::optional<GURL> url = MinimizeUrlForTopicsFeedback(visit.url)) {
        // `emplace()` keeps the first Topic if a visit is in several.
        topic_id_by_visit.emplace(std::make_pair(visit.visit_time, url->spec()),
                                  &journey.journey_id);
      }
    }
  }

  std::vector<ExportedVisit> exported_visits;
  exported_visits.reserve(data.visits.size());
  for (const TopicsFeedbackHistoryVisit& visit : data.visits) {
    std::optional<GURL> url = MinimizeUrlForTopicsFeedback(visit.url);
    if (!url) {
      continue;
    }
    auto it =
        topic_id_by_visit.find(std::make_pair(visit.visit_time, url->spec()));
    exported_visits.push_back(ExportedVisit{
        .visit_time = visit.visit_time,
        .url = std::move(*url),
        .title = raw_ref<const std::u16string>(visit.title),
        .is_foreign = visit.is_foreign,
        .topic_id = it != topic_id_by_visit.end() ? it->second : nullptr,
    });
  }
  return exported_visits;
}

mojom::TopicsFeedbackExportStatsPtr ComputeStats(
    const TopicsFeedbackExportData& data,
    const std::vector<ExportedVisit>& visits) {
  const auto unclustered_visits = std::ranges::count_if(
      visits, [](const ExportedVisit& visit) { return !visit.topic_id; });
  return mojom::TopicsFeedbackExportStats::New(
      base::checked_cast<uint32_t>(data.journeys.size()),
      base::checked_cast<uint32_t>(visits.size()),
      base::checked_cast<uint32_t>(unclustered_visits),
      base::saturated_cast<uint32_t>(data.unresolvable_topics));
}

std::string_view DeviceToString(bool is_foreign) {
  return is_foreign ? "synced" : "local";
}

std::string_view RatingToString(mojom::TopicRating rating) {
  switch (rating) {
    case mojom::TopicRating::kUnrated:
      return "unrated";
    case mojom::TopicRating::kLiked:
      return "liked";
    case mojom::TopicRating::kDisliked:
      return "disliked";
  }
  NOTREACHED();
}

std::string_view DefectToString(mojom::TopicDefectCategory defect) {
  switch (defect) {
    case mojom::TopicDefectCategory::kIrrelevantInformationIncluded:
      return "irrelevant_information_included";
    case mojom::TopicDefectCategory::kRelatedInformationMissing:
      return "related_information_missing";
    case mojom::TopicDefectCategory::kTitleWrongOrVague:
      return "title_wrong_or_vague";
    case mojom::TopicDefectCategory::kEmojiWrong:
      return "emoji_wrong";
    case mojom::TopicDefectCategory::kOverviewInaccurate:
      return "overview_inaccurate";
    case mojom::TopicDefectCategory::kTooBroad:
      return "too_broad";
    case mojom::TopicDefectCategory::kCombinesSeparateTopics:
      return "combines_separate_topics";
    case mojom::TopicDefectCategory::kTooNarrowOrFragmented:
      return "too_narrow_or_fragmented";
    case mojom::TopicDefectCategory::kIncidentalBrowsing:
      return "incidental_browsing";
    case mojom::TopicDefectCategory::kDuplicateOfAnotherTopic:
      return "duplicate_of_another_topic";
    case mojom::TopicDefectCategory::kSensitiveTopic:
      return "sensitive_topic";
    case mojom::TopicDefectCategory::kOther:
      return "other";
    case mojom::TopicDefectCategory::kOutdated:
      return "outdated";
  }
  NOTREACHED();
}

base::ListValue TimesToList(const std::vector<base::Time>& times) {
  base::ListValue list;
  list.reserve(times.size());
  for (base::Time time : times) {
    list.Append(FormatTopicsFeedbackTime(time));
  }
  return list;
}

base::DictValue StatsToDict(const mojom::TopicsFeedbackExportStats& stats) {
  return base::DictValue()
      .Set("topics", base::checked_cast<int>(stats.topics))
      .Set("visits", base::checked_cast<int>(stats.visits))
      .Set("unclustered_visits",
           base::checked_cast<int>(stats.unclustered_visits))
      .Set("unresolvable_topics",
           base::checked_cast<int>(stats.unresolvable_topics));
}

base::DictValue JourneyToDict(const history::journeys::Journey& journey) {
  base::ListValue visits;
  visits.reserve(journey.visits.size());
  for (const history::journeys::JourneyVisit& visit : journey.visits) {
    visits.Append(base::DictValue()
                      .Set("t", FormatTopicsFeedbackTime(visit.visit_time))
                      .Set("device", DeviceToString(visit.is_foreign)));
  }

  base::ListValue continuation_queries;
  continuation_queries.reserve(journey.continuation_queries.size());
  for (const history::journeys::JourneyContinuationQuery& query :
       journey.continuation_queries) {
    continuation_queries.Append(base::DictValue()
                                    .Set("title", query.title)
                                    .Set("prompt", query.prompt));
  }

  return base::DictValue()
      .Set("id", journey.journey_id)
      .Set("title", journey.title)
      .Set("emoji", journey.emoji.value_or(""))
      .Set("overview", journey.overview.value_or(""))
      .Set("short_overview", journey.short_overview.value_or(""))
      .Set("created_at", FormatTopicsFeedbackTime(journey.creation_time))
      .Set("visits", std::move(visits))
      .Set("continuation_queries", std::move(continuation_queries));
}

base::DictValue DuplicateTopicToDict(
    const mojom::DuplicateTopicSnapshot& duplicate) {
  return base::DictValue()
      .Set("id", duplicate.id)
      .Set("title", duplicate.title)
      .Set("visit_times", TimesToList(duplicate.visit_times));
}

base::DictValue FeedbackToDict(const mojom::TopicFeedback& feedback) {
  base::ListValue defects;
  defects.reserve(feedback.defects.size());
  for (mojom::TopicDefectCategory defect : feedback.defects) {
    defects.Append(DefectToString(defect));
  }

  base::ListValue query_feedbacks;
  query_feedbacks.reserve(feedback.query_feedbacks.size());
  for (const mojom::TopicQueryFeedbackPtr& query_feedback :
       feedback.query_feedbacks) {
    query_feedbacks.Append(base::DictValue()
                               .Set("index", query_feedback->index)
                               .Set("query_text", query_feedback->query_text)
                               .Set("liked", query_feedback->liked));
  }

  const mojom::TopicSnapshot& snapshot = *feedback.snapshot;
  return base::DictValue()
      .Set("id", feedback.id)
      .Set(
          "snapshot",
          base::DictValue()
              .Set("title", snapshot.title)
              .Set("emoji", snapshot.emoji)
              .Set("overview", snapshot.overview)
              .Set("visit_times", TimesToList(snapshot.visit_times))
              .Set("time_rated", FormatTopicsFeedbackTime(snapshot.time_rated)))
      .Set("rating", RatingToString(feedback.rating))
      .Set("defects", std::move(defects))
      .Set("comment", feedback.comment)
      .Set("query_feedbacks", std::move(query_feedbacks))
      .Set("rejected_visits", TimesToList(feedback.rejected_visits))
      // Null rather than absent when there is no duplicate, so every key is
      // always present.
      .Set("duplicate_of",
           feedback.duplicate_of
               ? base::Value(DuplicateTopicToDict(*feedback.duplicate_of))
               : base::Value());
}

}  // namespace

TopicsFeedbackExportData::TopicsFeedbackExportData() = default;
TopicsFeedbackExportData::TopicsFeedbackExportData(TopicsFeedbackExportData&&) =
    default;
TopicsFeedbackExportData& TopicsFeedbackExportData::operator=(
    TopicsFeedbackExportData&&) = default;
TopicsFeedbackExportData::~TopicsFeedbackExportData() = default;

TopicsFeedbackExportContext::TopicsFeedbackExportContext() = default;
TopicsFeedbackExportContext::TopicsFeedbackExportContext(
    const TopicsFeedbackExportContext&) = default;
TopicsFeedbackExportContext& TopicsFeedbackExportContext::operator=(
    const TopicsFeedbackExportContext&) = default;
TopicsFeedbackExportContext::TopicsFeedbackExportContext(
    TopicsFeedbackExportContext&&) = default;
TopicsFeedbackExportContext& TopicsFeedbackExportContext::operator=(
    TopicsFeedbackExportContext&&) = default;
TopicsFeedbackExportContext::~TopicsFeedbackExportContext() = default;

int ClampTopicsFeedbackWindowDays(int window_days) {
  return std::clamp(window_days, kMinTopicsFeedbackWindowDays,
                    kMaxTopicsFeedbackWindowDays);
}

std::string FormatTopicsFeedbackTime(base::Time time) {
  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  int64_t microseconds = time.ToDeltaSinceWindowsEpoch().InMicroseconds() %
                         base::Time::kMicrosecondsPerSecond;
  if (microseconds < 0) {
    microseconds += base::Time::kMicrosecondsPerSecond;
  }
  return base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02d.%06dZ", exploded.year, exploded.month,
      exploded.day_of_month, exploded.hour, exploded.minute, exploded.second,
      static_cast<int>(microseconds));
}

mojom::TopicsFeedbackExportPreviewPtr BuildTopicsFeedbackExportPreview(
    const TopicsFeedbackExportData& data) {
  const std::vector<ExportedVisit> visits = MinimizeAndLinkVisits(data);

  // Domains and their URLs, keyed by host and URL spec. Visits are newest
  // first, so the first visit to a URL provides its title.
  std::map<std::string,
           std::map<std::string, mojom::TopicsFeedbackExportUrlPtr>>
      urls_by_domain;
  for (const ExportedVisit& visit : visits) {
    mojom::TopicsFeedbackExportUrlPtr& url =
        urls_by_domain[std::string(visit.url.host())][visit.url.spec()];
    if (!url) {
      url = mojom::TopicsFeedbackExportUrl::New(
          visit.url, base::UTF16ToUTF8(*visit.title), /*visit_count=*/0);
    }
    ++url->visit_count;
  }

  std::vector<mojom::TopicsFeedbackExportDomainPtr> domains;
  domains.reserve(urls_by_domain.size());
  for (auto& [host, urls_by_spec] : urls_by_domain) {
    auto domain = mojom::TopicsFeedbackExportDomain::New();
    domain->domain = host;
    domain->default_excluded = IsDefaultExcludedDomain(host);
    domain->urls.reserve(urls_by_spec.size());
    for (auto& [spec, url] : urls_by_spec) {
      domain->visit_count += url->visit_count;
      domain->urls.push_back(std::move(url));
    }
    // Most visited first. The sort is stable, so ties stay ordered by spec.
    std::ranges::stable_sort(domain->urls, std::ranges::greater(),
                             &mojom::TopicsFeedbackExportUrl::visit_count);
    domains.push_back(std::move(domain));
  }
  // Most visited first. Ties stay ordered by host.
  std::ranges::stable_sort(domains, std::ranges::greater(),
                           &mojom::TopicsFeedbackExportDomain::visit_count);

  return mojom::TopicsFeedbackExportPreview::New(ComputeStats(data, visits),
                                                 std::move(domains));
}

std::string BuildTopicsFeedbackBundle(
    const TopicsFeedbackExportData& data,
    const TopicsFeedbackExportContext& context,
    const mojom::TopicsFeedbackExportOptions& options) {
  const std::vector<ExportedVisit> visits = MinimizeAndLinkVisits(data);

  const std::set<std::string, std::less<>> excluded_domains(
      options.excluded_domains.begin(), options.excluded_domains.end());
  // Minimize the URLs so they compare equal to the exported ones even if the
  // caller passed them unminimized.
  std::set<std::string, std::less<>> excluded_urls;
  for (const GURL& url : options.excluded_urls) {
    if (std::optional<GURL> minimized = MinimizeUrlForTopicsFeedback(url)) {
      excluded_urls.insert(minimized->spec());
    }
  }

  base::DictValue feature_params;
  for (const auto& [name, value] : context.feature_params) {
    feature_params.Set(name, value);
  }

  base::Value finch;
  if (context.finch) {
    finch = base::Value(base::DictValue()
                            .Set("trial", context.finch->trial)
                            .Set("group", context.finch->group));
  }

  base::ListValue topics;
  topics.reserve(data.journeys.size());
  for (const history::journeys::Journey& journey : data.journeys) {
    topics.Append(JourneyToDict(journey));
  }

  base::ListValue visits_list;
  visits_list.reserve(visits.size());
  for (const ExportedVisit& visit : visits) {
    base::Value topic_id;
    if (visit.topic_id) {
      topic_id = base::Value(*visit.topic_id);
    }
    base::DictValue visit_dict;
    visit_dict.Set("t", FormatTopicsFeedbackTime(visit.visit_time));
    // A visit is either exported with its URL and title, or redacted.
    const bool redacted = excluded_domains.contains(visit.url.host()) ||
                          excluded_urls.contains(visit.url.spec());
    if (!redacted) {
      visit_dict.Set("url", visit.url.spec());
      visit_dict.Set("title", base::UTF16ToUTF8(*visit.title));
    }
    visit_dict.Set("topic_id", std::move(topic_id));
    visit_dict.Set("device", DeviceToString(visit.is_foreign));
    visit_dict.Set("redacted", redacted);
    visits_list.Append(std::move(visit_dict));
  }

  base::ListValue feedbacks;
  feedbacks.reserve(data.feedbacks.size());
  for (const mojom::TopicFeedbackPtr& feedback : data.feedbacks) {
    feedbacks.Append(FeedbackToDict(*feedback));
  }

  base::DictValue bundle =
      base::DictValue()
          .Set("schema_version", kTopicsFeedbackSchemaVersion)
          .Set("exported_at", FormatTopicsFeedbackTime(context.exported_at))
          .Set("chrome", base::DictValue()
                             .Set("version", context.chrome_version)
                             .Set("channel", context.chrome_channel))
          .Set("feature_params",
               base::DictValue().Set("Topics", std::move(feature_params)))
          .Set("finch", std::move(finch))
          .Set("window_days", context.window_days)
          .Set("window_start", FormatTopicsFeedbackTime(context.window_start))
          .Set("redaction",
               base::DictValue()
                   .Set("excluded_domains_count",
                        base::checked_cast<int>(excluded_domains.size()))
                   .Set("excluded_urls_count",
                        base::checked_cast<int>(excluded_urls.size())))
          .Set("stats", StatsToDict(*ComputeStats(data, visits)))
          .Set("topics", std::move(topics))
          .Set("visits", std::move(visits_list))
          .Set("feedback", base::DictValue()
                               .Set("topics", std::move(feedbacks))
                               .Set("missing_topics", options.missing_topics));

  std::optional<std::string> json = base::WriteJsonWithOptions(
      bundle, base::JSONWriter::OPTIONS_PRETTY_PRINT);
  // Serialization only fails for binary values or excessive nesting, neither
  // of which the bundle contains.
  CHECK(json);
  return std::move(*json);
}

}  // namespace context_hub
