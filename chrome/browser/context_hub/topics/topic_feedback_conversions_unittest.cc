// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/topics/topic_feedback_conversions.h"

#include <string>
#include <vector>

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace context_hub {

namespace {

namespace mojom = ::browser::context_hub::mojom;

using ::testing::ElementsAre;
using ::testing::IsEmpty;

constexpr char kTopicId[] = "topic_1";

base::Time FixedTime() {
  return base::Time::FromSecondsSinceUnixEpoch(1'700'000'000);
}

// Builds a TopicFeedback with every field populated.
mojom::TopicFeedbackPtr MakeFullFeedback() {
  const base::Time now = FixedTime();
  auto feedback = mojom::TopicFeedback::New();
  feedback->id = kTopicId;
  feedback->snapshot = mojom::TopicSnapshot::New(
      "Topic title", "🔵", "Topic overview.",
      std::vector<base::Time>{now - base::Hours(2), now - base::Hours(1)}, now);
  feedback->rating = mojom::TopicRating::kDisliked;
  feedback->defects = {mojom::TopicDefectCategory::kTooBroad,
                       mojom::TopicDefectCategory::kOther,
                       mojom::TopicDefectCategory::kOutdated};
  feedback->comment = "Feedback comment.";
  feedback->query_feedbacks.push_back(
      mojom::TopicQueryFeedback::New(0, "Query one", true));
  feedback->query_feedbacks.push_back(
      mojom::TopicQueryFeedback::New(2, "Query two", false));
  feedback->rejected_visits = {now - base::Hours(1)};
  feedback->duplicate_of = mojom::DuplicateTopicSnapshot::New(
      "topic_dup", "Duplicate title",
      std::vector<base::Time>{now - base::Days(1)});
  return feedback;
}

// Builds a TopicFeedback with only the required fields set.
mojom::TopicFeedbackPtr MakeMinimalFeedback() {
  auto feedback = mojom::TopicFeedback::New();
  feedback->id = kTopicId;
  feedback->snapshot = mojom::TopicSnapshot::New(
      "Title", /*emoji=*/"", /*overview=*/"", std::vector<base::Time>(),
      FixedTime());
  return feedback;
}

// The smallest dictionary that TopicFeedbackFromDict accepts.
base::DictValue MakeMinimalDict() {
  return base::DictValue().Set(
      "snapshot", base::DictValue()
                      .Set("title", "Title")
                      .Set("time_rated", base::TimeToValue(FixedTime())));
}

TEST(TopicFeedbackConversionsTest, RoundTripsAllFields) {
  mojom::TopicFeedbackPtr expected = MakeFullFeedback();
  mojom::TopicFeedbackPtr actual =
      TopicFeedbackFromDict(kTopicId, TopicFeedbackToDict(*expected));
  ASSERT_TRUE(actual);
  EXPECT_TRUE(mojo::Equals(expected, actual));
}

TEST(TopicFeedbackConversionsTest, RoundTripsMinimalFields) {
  mojom::TopicFeedbackPtr expected = MakeMinimalFeedback();
  mojom::TopicFeedbackPtr actual =
      TopicFeedbackFromDict(kTopicId, TopicFeedbackToDict(*expected));
  ASSERT_TRUE(actual);
  EXPECT_TRUE(mojo::Equals(expected, actual));
  EXPECT_EQ(mojom::TopicRating::kUnrated, actual->rating);
  EXPECT_TRUE(actual->comment.empty());
  EXPECT_TRUE(actual->duplicate_of.is_null());
}

TEST(TopicFeedbackConversionsTest, OptionalFieldsAreOmittedFromDict) {
  base::DictValue dict = TopicFeedbackToDict(*MakeMinimalFeedback());
  EXPECT_FALSE(dict.contains("comment"));
  EXPECT_FALSE(dict.contains("duplicate_of"));
  EXPECT_FALSE(dict.FindDict("snapshot")->contains("emoji"));
  EXPECT_FALSE(dict.FindDict("snapshot")->contains("overview"));
}

TEST(TopicFeedbackConversionsTest, UsesTopicIdArgument) {
  mojom::TopicFeedbackPtr actual = TopicFeedbackFromDict(
      "other_id", TopicFeedbackToDict(*MakeFullFeedback()));
  ASSERT_TRUE(actual);
  EXPECT_EQ("other_id", actual->id);
}

TEST(TopicFeedbackConversionsTest, RejectsMissingSnapshot) {
  EXPECT_FALSE(TopicFeedbackFromDict(kTopicId, base::DictValue()));
  EXPECT_FALSE(TopicFeedbackFromDict(
      kTopicId, base::DictValue().Set("snapshot", "not a dict")));
}

TEST(TopicFeedbackConversionsTest, RejectsSnapshotWithoutRequiredFields) {
  EXPECT_FALSE(TopicFeedbackFromDict(
      kTopicId,
      base::DictValue().Set("snapshot",
                            base::DictValue().Set("title", "No time_rated"))));
  EXPECT_FALSE(TopicFeedbackFromDict(
      kTopicId,
      base::DictValue().Set(
          "snapshot", base::DictValue().Set("time_rated",
                                            base::TimeToValue(FixedTime())))));
}

TEST(TopicFeedbackConversionsTest, AcceptsMinimalDict) {
  mojom::TopicFeedbackPtr actual =
      TopicFeedbackFromDict(kTopicId, MakeMinimalDict());
  ASSERT_TRUE(actual);
  EXPECT_TRUE(mojo::Equals(MakeMinimalFeedback(), actual));
}

TEST(TopicFeedbackConversionsTest, SkipsUnknownAndMalformedDefects) {
  base::DictValue dict = MakeMinimalDict();
  dict.Set("defects",
           base::ListValue()
               .Append(static_cast<int>(mojom::TopicDefectCategory::kTooBroad))
               // A category added by a newer Chrome, unknown to this one.
               .Append(10000)
               .Append("not an int")
               .Append(static_cast<int>(mojom::TopicDefectCategory::kOther)));

  mojom::TopicFeedbackPtr actual = TopicFeedbackFromDict(kTopicId, dict);
  ASSERT_TRUE(actual);
  EXPECT_THAT(actual->defects,
              ElementsAre(mojom::TopicDefectCategory::kTooBroad,
                          mojom::TopicDefectCategory::kOther));
}

TEST(TopicFeedbackConversionsTest, SkipsMalformedQueryFeedbacks) {
  base::DictValue dict = MakeMinimalDict();
  dict.Set("query_feedbacks",
           base::ListValue()
               .Append("not a dict")
               // Missing `liked`.
               .Append(base::DictValue().Set("index", 0).Set("query_text", "q"))
               // Negative index.
               .Append(base::DictValue()
                           .Set("index", -1)
                           .Set("query_text", "q")
                           .Set("liked", true))
               .Append(base::DictValue()
                           .Set("index", 3)
                           .Set("query_text", "kept")
                           .Set("liked", false)));

  mojom::TopicFeedbackPtr actual = TopicFeedbackFromDict(kTopicId, dict);
  ASSERT_TRUE(actual);
  ASSERT_EQ(1u, actual->query_feedbacks.size());
  EXPECT_EQ(3, actual->query_feedbacks[0]->index);
  EXPECT_EQ("kept", actual->query_feedbacks[0]->query_text);
  EXPECT_FALSE(actual->query_feedbacks[0]->liked);
}

TEST(TopicFeedbackConversionsTest, SkipsMalformedTimes) {
  base::DictValue dict = MakeMinimalDict();
  dict.Set("rejected_visits", base::ListValue()
                                  .Append("not a time")
                                  .Append(base::TimeToValue(FixedTime())));

  mojom::TopicFeedbackPtr actual = TopicFeedbackFromDict(kTopicId, dict);
  ASSERT_TRUE(actual);
  EXPECT_THAT(actual->rejected_visits, ElementsAre(FixedTime()));
}

TEST(TopicFeedbackConversionsTest, DropsMalformedDuplicateOf) {
  base::DictValue dict = MakeMinimalDict();
  // Missing `title`.
  dict.Set("duplicate_of", base::DictValue().Set("id", "topic_dup"));

  mojom::TopicFeedbackPtr actual = TopicFeedbackFromDict(kTopicId, dict);
  ASSERT_TRUE(actual);
  EXPECT_TRUE(actual->duplicate_of.is_null());
}

TEST(TopicFeedbackConversionsTest, UnknownRatingReadsAsUnrated) {
  base::DictValue dict = MakeMinimalDict();
  // A rating value added by a newer Chrome, unknown to this one.
  dict.Set("rating", 10000);

  mojom::TopicFeedbackPtr actual = TopicFeedbackFromDict(kTopicId, dict);
  ASSERT_TRUE(actual);
  EXPECT_EQ(mojom::TopicRating::kUnrated, actual->rating);
}

TEST(TopicFeedbackConversionsTest, IgnoresWrongTypeOptionalFields) {
  base::DictValue dict = MakeMinimalDict();
  dict.Set("rating", "liked");
  dict.Set("comment", 42);
  dict.Set("defects", "none");

  mojom::TopicFeedbackPtr actual = TopicFeedbackFromDict(kTopicId, dict);
  ASSERT_TRUE(actual);
  EXPECT_EQ(mojom::TopicRating::kUnrated, actual->rating);
  EXPECT_TRUE(actual->comment.empty());
  EXPECT_THAT(actual->defects, IsEmpty());
}

}  // namespace

}  // namespace context_hub
