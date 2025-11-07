// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_variants_reader.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tool_request_variant.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {
namespace {

template <typename T>
void VerifyToolHasHistogramEntry(
    const base::HistogramVariantsEntryMap& tool_requests) {
  EXPECT_TRUE(tool_requests.contains(T::kName))
      << "ToolRequest with name " << T::kName
      << " not found in "
         "//tools/metrics/histograms/metadata/actor/"
         "histograms.xml:ToolRequest.";
}

template <std::size_t... Idxs>
void CheckToolRequestVariantNamesImpl(
    const base::HistogramVariantsEntryMap& tool_requests,
    std::index_sequence<Idxs...>) {
  (VerifyToolHasHistogramEntry<
       std::variant_alternative_t<Idxs, actor::ToolRequestVariant>>(
       tool_requests),
   ...);
}

TEST(ActorHistogramsTest, CheckToolRequestVariantNames) {
  std::optional<base::HistogramVariantsEntryMap> tool_requests =
      base::ReadVariantsFromHistogramsXml("ToolRequest", "actor");
  ASSERT_TRUE(tool_requests.has_value());
  EXPECT_EQ(std::variant_size_v<actor::ToolRequestVariant>,
            tool_requests->size());

  CheckToolRequestVariantNamesImpl(
      *tool_requests, std::make_index_sequence<
                          std::variant_size_v<actor::ToolRequestVariant>>{});
}

TEST(ActorHistogramsTest, CheckActorTaskStateVariantNames) {
  std::optional<base::HistogramVariantsEntryMap> task_states =
      base::ReadVariantsFromHistogramsXml("ActorTaskState", "actor");
  ASSERT_TRUE(task_states.has_value());
  ASSERT_EQ(task_states->size(),
            static_cast<size_t>(ActorTask::State::kMaxValue) + 1);

  for (int i = 0; i <= static_cast<int>(ActorTask::State::kMaxValue); i++) {
    std::string state_string = ToString(static_cast<ActorTask::State>(i));
    EXPECT_TRUE(task_states->contains(state_string))
        << "ActorTask::State " << state_string
        << " not found in "
           "//tools/metrics/histograms/metadata/actor/"
           "histograms.xml:ActorTaskState.";
  }
}

}  // namespace
}  // namespace actor
