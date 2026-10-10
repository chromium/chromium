// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_TOPICS_TOPICS_FEEDBACK_EXPORTER_H_
#define CHROME_BROWSER_CONTEXT_HUB_TOPICS_TOPICS_FEEDBACK_EXPORTER_H_

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "components/history/core/browser/journeys/journey.h"
#include "url/gurl.h"

// Builds the Topics fishfood feedback export (go/jbi-fishfood-feedback-design)
// from data already gathered from History and ContextHubService. Everything
// here is pure, so the export format can be tested without a profile.

namespace context_hub {

// Version of the exported JSON bundle format.
inline constexpr int kTopicsFeedbackSchemaVersion = 1;

// Bounds and default of the number of days of history in the export window.
inline constexpr int kMinTopicsFeedbackWindowDays = 1;
inline constexpr int kDefaultTopicsFeedbackWindowDays = 5;
inline constexpr int kMaxTopicsFeedbackWindowDays = 14;

// Returns `window_days` clamped to
// [kMinTopicsFeedbackWindowDays, kMaxTopicsFeedbackWindowDays].
int ClampTopicsFeedbackWindowDays(int window_days);

// Formats `time` as a UTC ISO-8601 timestamp with microsecond precision, e.g.
// "2026-09-22T10:01:02.123456Z". All timestamps in the bundle use this format.
std::string FormatTopicsFeedbackTime(base::Time time);

// A history visit in the export window, before minimization.
struct TopicsFeedbackHistoryVisit {
  GURL url;
  std::u16string title;
  base::Time visit_time;
  // True if the visit was synced from another device.
  bool is_foreign = false;
};

// Everything the export is built from.
struct TopicsFeedbackExportData {
  TopicsFeedbackExportData();
  TopicsFeedbackExportData(TopicsFeedbackExportData&&);
  TopicsFeedbackExportData& operator=(TopicsFeedbackExportData&&);
  ~TopicsFeedbackExportData();

  // Resolved Topics, in the order they are exported. If a visit belongs to
  // more than one, it is linked to the first.
  std::vector<history::journeys::Journey> journeys;
  // Visits in the export window, in the order they are exported (newest
  // first).
  std::vector<TopicsFeedbackHistoryVisit> visits;
  // Number of stored Topics that could not be resolved on this device.
  size_t unresolvable_topics = 0;
  // Stored fishfood feedback.
  std::vector<browser::context_hub::mojom::TopicFeedbackPtr> feedbacks;
};

// Chrome and experiment state recorded in the bundle.
struct TopicsFeedbackExportContext {
  struct FinchGroup {
    std::string trial;
    std::string group;
  };

  TopicsFeedbackExportContext();
  TopicsFeedbackExportContext(const TopicsFeedbackExportContext&);
  TopicsFeedbackExportContext& operator=(const TopicsFeedbackExportContext&);
  TopicsFeedbackExportContext(TopicsFeedbackExportContext&&);
  TopicsFeedbackExportContext& operator=(TopicsFeedbackExportContext&&);
  ~TopicsFeedbackExportContext();

  base::Time exported_at;
  // Start of the export window, i.e. the cutoff of the history query.
  base::Time window_start;
  int window_days = kDefaultTopicsFeedbackWindowDays;
  std::string chrome_version;
  std::string chrome_channel;
  // Field trial the Topics feature is associated with, if any.
  std::optional<FinchGroup> finch;
  // Params of the Topics feature.
  std::map<std::string, std::string> feature_params;
};

// Returns the preview shown to the rater before export: the summary stats, and
// the minimized visits in the window grouped by host.
browser::context_hub::mojom::TopicsFeedbackExportPreviewPtr
BuildTopicsFeedbackExportPreview(const TopicsFeedbackExportData& data);

// Returns the schema_version 1 JSON bundle. Visits with non-http(s) URLs are
// dropped and the remaining URLs are minimized with
// `MinimizeUrlForTopicsFeedback()`. Each visit is linked to the Topic
// containing a visit with the same time and minimized URL. Visits to
// `options.excluded_domains` or `options.excluded_urls` are exported as
// stubs without a URL or title.
std::string BuildTopicsFeedbackBundle(
    const TopicsFeedbackExportData& data,
    const TopicsFeedbackExportContext& context,
    const browser::context_hub::mojom::TopicsFeedbackExportOptions& options);

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_TOPICS_TOPICS_FEEDBACK_EXPORTER_H_
