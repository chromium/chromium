// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/topics/topic_feedback_conversions.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"

namespace context_hub {

namespace {

namespace mojom = ::browser::context_hub::mojom;

// Dictionary keys. Shared names (e.g. `title`, `visit_times`) are used at
// more than one nesting level.
constexpr char kSnapshotKey[] = "snapshot";
constexpr char kRatingKey[] = "rating";
constexpr char kLikedKey[] = "liked";
constexpr char kDefectsKey[] = "defects";
constexpr char kCommentKey[] = "comment";
constexpr char kQueryFeedbacksKey[] = "query_feedbacks";
constexpr char kRejectedVisitsKey[] = "rejected_visits";
constexpr char kDuplicateOfKey[] = "duplicate_of";
constexpr char kIdKey[] = "id";
constexpr char kTitleKey[] = "title";
constexpr char kEmojiKey[] = "emoji";
constexpr char kOverviewKey[] = "overview";
constexpr char kVisitTimesKey[] = "visit_times";
constexpr char kTimeRatedKey[] = "time_rated";
constexpr char kIndexKey[] = "index";
constexpr char kQueryTextKey[] = "query_text";

base::ListValue TimesToList(const std::vector<base::Time>& times) {
  base::ListValue list;
  list.reserve(times.size());
  for (base::Time time : times) {
    list.Append(base::TimeToValue(time));
  }
  return list;
}

// Returns an empty vector if `list` is null; skips elements that are not
// valid times.
std::vector<base::Time> TimesFromList(const base::ListValue* list) {
  std::vector<base::Time> times;
  if (!list) {
    return times;
  }
  times.reserve(list->size());
  for (const base::Value& value : *list) {
    if (std::optional<base::Time> time = base::ValueToTime(value)) {
      times.push_back(*time);
    }
  }
  return times;
}

base::DictValue SnapshotToDict(const mojom::TopicSnapshot& snapshot) {
  base::DictValue dict;
  dict.Set(kTitleKey, snapshot.title);
  if (!snapshot.emoji.empty()) {
    dict.Set(kEmojiKey, snapshot.emoji);
  }
  if (!snapshot.overview.empty()) {
    dict.Set(kOverviewKey, snapshot.overview);
  }
  dict.Set(kVisitTimesKey, TimesToList(snapshot.visit_times));
  dict.Set(kTimeRatedKey, base::TimeToValue(snapshot.time_rated));
  return dict;
}

// Returns null if `dict` is null or lacks a title or time_rated.
mojom::TopicSnapshotPtr SnapshotFromDict(const base::DictValue* dict) {
  if (!dict) {
    return nullptr;
  }
  const std::string* title = dict->FindString(kTitleKey);
  std::optional<base::Time> time_rated =
      base::ValueToTime(dict->Find(kTimeRatedKey));
  if (!title || !time_rated) {
    return nullptr;
  }
  auto snapshot = mojom::TopicSnapshot::New();
  snapshot->title = *title;
  if (const std::string* emoji = dict->FindString(kEmojiKey)) {
    snapshot->emoji = *emoji;
  }
  if (const std::string* overview = dict->FindString(kOverviewKey)) {
    snapshot->overview = *overview;
  }
  snapshot->visit_times = TimesFromList(dict->FindList(kVisitTimesKey));
  snapshot->time_rated = *time_rated;
  return snapshot;
}

base::ListValue DefectsToList(
    const std::vector<mojom::TopicDefectCategory>& defects) {
  base::ListValue list;
  list.reserve(defects.size());
  for (mojom::TopicDefectCategory defect : defects) {
    list.Append(static_cast<int>(defect));
  }
  return list;
}

// Skips non-integer values and categories unknown to this Chrome.
std::vector<mojom::TopicDefectCategory> DefectsFromList(
    const base::ListValue* list) {
  std::vector<mojom::TopicDefectCategory> defects;
  if (!list) {
    return defects;
  }
  defects.reserve(list->size());
  for (const base::Value& value : *list) {
    std::optional<int> raw = value.GetIfInt();
    if (!raw) {
      continue;
    }
    auto category = static_cast<mojom::TopicDefectCategory>(*raw);
    if (mojom::IsKnownEnumValue(category)) {
      defects.push_back(category);
    }
  }
  return defects;
}

base::ListValue QueryFeedbacksToList(
    const std::vector<mojom::TopicQueryFeedbackPtr>& query_feedbacks) {
  base::ListValue list;
  list.reserve(query_feedbacks.size());
  for (const mojom::TopicQueryFeedbackPtr& query_feedback : query_feedbacks) {
    list.Append(base::DictValue()
                    .Set(kIndexKey, query_feedback->index)
                    .Set(kQueryTextKey, query_feedback->query_text)
                    .Set(kLikedKey, query_feedback->liked));
  }
  return list;
}

// Skips elements that are not dictionaries or lack any field.
std::vector<mojom::TopicQueryFeedbackPtr> QueryFeedbacksFromList(
    const base::ListValue* list) {
  std::vector<mojom::TopicQueryFeedbackPtr> query_feedbacks;
  if (!list) {
    return query_feedbacks;
  }
  query_feedbacks.reserve(list->size());
  for (const base::Value& value : *list) {
    const base::DictValue* dict = value.GetIfDict();
    if (!dict) {
      continue;
    }
    std::optional<int> index = dict->FindInt(kIndexKey);
    const std::string* query_text = dict->FindString(kQueryTextKey);
    std::optional<bool> liked = dict->FindBool(kLikedKey);
    if (!index || *index < 0 || !query_text || !liked) {
      continue;
    }
    query_feedbacks.push_back(
        mojom::TopicQueryFeedback::New(*index, *query_text, *liked));
  }
  return query_feedbacks;
}

base::DictValue DuplicateToDict(
    const mojom::DuplicateTopicSnapshot& duplicate) {
  return base::DictValue()
      .Set(kIdKey, duplicate.id)
      .Set(kTitleKey, duplicate.title)
      .Set(kVisitTimesKey, TimesToList(duplicate.visit_times));
}

// Returns null if `dict` is null or lacks an id or title.
mojom::DuplicateTopicSnapshotPtr DuplicateFromDict(
    const base::DictValue* dict) {
  if (!dict) {
    return nullptr;
  }
  const std::string* id = dict->FindString(kIdKey);
  const std::string* title = dict->FindString(kTitleKey);
  if (!id || !title) {
    return nullptr;
  }
  return mojom::DuplicateTopicSnapshot::New(
      *id, *title, TimesFromList(dict->FindList(kVisitTimesKey)));
}

}  // namespace

base::DictValue TopicFeedbackToDict(const mojom::TopicFeedback& feedback) {
  base::DictValue dict;
  dict.Set(kSnapshotKey, SnapshotToDict(*feedback.snapshot));
  dict.Set(kRatingKey, static_cast<int>(feedback.rating));
  dict.Set(kDefectsKey, DefectsToList(feedback.defects));
  if (!feedback.comment.empty()) {
    dict.Set(kCommentKey, feedback.comment);
  }
  dict.Set(kQueryFeedbacksKey, QueryFeedbacksToList(feedback.query_feedbacks));
  dict.Set(kRejectedVisitsKey, TimesToList(feedback.rejected_visits));
  if (feedback.duplicate_of) {
    dict.Set(kDuplicateOfKey, DuplicateToDict(*feedback.duplicate_of));
  }
  return dict;
}

mojom::TopicFeedbackPtr TopicFeedbackFromDict(const std::string& topic_id,
                                              const base::DictValue& dict) {
  mojom::TopicSnapshotPtr snapshot =
      SnapshotFromDict(dict.FindDict(kSnapshotKey));
  if (!snapshot) {
    return nullptr;
  }
  auto feedback = mojom::TopicFeedback::New();
  feedback->id = topic_id;
  feedback->snapshot = std::move(snapshot);
  // Missing, non-integer, or unknown ratings fall back to kUnrated.
  feedback->rating = mojom::TopicRating::kUnrated;
  if (std::optional<int> raw_rating = dict.FindInt(kRatingKey)) {
    auto rating = static_cast<mojom::TopicRating>(*raw_rating);
    if (mojom::IsKnownEnumValue(rating)) {
      feedback->rating = rating;
    }
  }
  feedback->defects = DefectsFromList(dict.FindList(kDefectsKey));
  if (const std::string* comment = dict.FindString(kCommentKey)) {
    feedback->comment = *comment;
  }
  feedback->query_feedbacks =
      QueryFeedbacksFromList(dict.FindList(kQueryFeedbacksKey));
  feedback->rejected_visits = TimesFromList(dict.FindList(kRejectedVisitsKey));
  feedback->duplicate_of = DuplicateFromDict(dict.FindDict(kDuplicateOfKey));
  return feedback;
}

}  // namespace context_hub
