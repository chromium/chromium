// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_topic_queue.h"

#include <algorithm>
#include <limits>
#include <random>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_constants.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

constexpr net::BackoffEntry::Policy kFetchTopicRetryBackoffPolicy = {
    10,             // Number of initial errors to ignore.
    500,            // Initial delay in ms.
    2.0,            // Factor by which the waiting time will be multiplied.
    0.2,            // Fuzzing percentage.
    2 * 60 * 1000,  // Maximum delay in ms.
    -1,             // Never discard the entry.
    true,           // Use initial delay.
};

int TypeToIndex(::ambient::TopicType topic_type) {
  int index = static_cast<int>(topic_type);
  DCHECK_GE(index, 0);
  return index;
}

::ambient::TopicType IndexToType(int index) {
  ::ambient::TopicType topic_type = static_cast<::ambient::TopicType>(index);
  return topic_type;
}

std::vector<AmbientModeTopic> CreatePairedTopics(
    const std::vector<AmbientModeTopic>& topics) {
  // We pair two topics if:
  // 1. They are in the landscape orientation.
  // 2. They are in the same category.
  // 3. They are not Geo photos.
  base::flat_map<int, std::vector<int>> topics_by_type;
  std::vector<AmbientModeTopic> paired_topics;
  int topic_idx = -1;
  for (const auto& topic : topics) {
    topic_idx++;

    // Do not pair Geo photos, which will be rotate to fill the screen.
    // If a photo is portrait, it is from Google Photos and should have a paired
    // photo already.
    if (topic.topic_type == ::ambient::TopicType::kGeo || topic.is_portrait) {
      paired_topics.emplace_back(topic);
      continue;
    }

    int type_index = TypeToIndex(topic.topic_type);
    auto it = topics_by_type.find(type_index);
    if (it == topics_by_type.end()) {
      topics_by_type.insert({type_index, {topic_idx}});
    } else {
      it->second.emplace_back(topic_idx);
    }
  }

  // We merge two unpaired topics to create a new topic with related images.
  for (auto it = topics_by_type.begin(); it < topics_by_type.end(); ++it) {
    size_t idx = 0;
    while (idx < it->second.size() - 1) {
      AmbientModeTopic paired_topic;
      const auto& topic_1 = topics[it->second[idx]];
      const auto& topic_2 = topics[it->second[idx + 1]];
      paired_topic.url = topic_1.url;
      paired_topic.related_image_url = topic_2.url;

      paired_topic.details = topic_1.details;
      paired_topic.related_details = topic_2.details;
      paired_topic.topic_type = IndexToType(it->first);
      paired_topic.is_portrait = topic_1.is_portrait;
      paired_topics.emplace_back(paired_topic);

      idx += 2;
    }
  }

  std::shuffle(paired_topics.begin(), paired_topics.end(),
               std::default_random_engine());
  return paired_topics;
}

std::pair<AmbientModeTopic, AmbientModeTopic> SplitTopic(
    const AmbientModeTopic& paired_topic) {
  const auto clear_related_fields_from_topic = [](AmbientModeTopic& topic) {
    topic.related_image_url.clear();
    topic.related_details.clear();
  };

  AmbientModeTopic topic_with_primary(paired_topic);
  clear_related_fields_from_topic(topic_with_primary);

  AmbientModeTopic topic_with_related(paired_topic);
  topic_with_related.url = std::move(topic_with_related.related_image_url);
  topic_with_related.details = std::move(topic_with_related.related_details);
  clear_related_fields_from_topic(topic_with_related);
  return std::make_pair(topic_with_primary, topic_with_related);
}

}  // namespace

AmbientTopicQueue::AmbientTopicQueue(
    int topic_fetch_limit,
    int topic_fetch_size,
    base::TimeDelta topic_fetch_interval,
    bool should_split_topics,
    Delegate* delegate,
    AmbientBackendController* backend_controller)
    : topic_fetch_limit_(topic_fetch_limit),
      topic_fetch_size_(topic_fetch_size),
      topic_fetch_interval_(topic_fetch_interval),
      should_split_topics_(should_split_topics),
      delegate_(delegate),
      backend_controller_(backend_controller),
      fetch_topic_retry_backoff_(&kFetchTopicRetryBackoffPolicy) {
  DCHECK_GT(topic_fetch_size_, 0);
  DCHECK(backend_controller_);
  FetchTopics();
}

AmbientTopicQueue::~AmbientTopicQueue() = default;

void AmbientTopicQueue::WaitForTopicsAvailable(WaitCallback wait_cb) {
  DCHECK(wait_cb);
  if (!IsEmpty()) {
    std::move(wait_cb).Run(WaitResult::kTopicsAvailable);
  } else if (HasReachedTopicFetchLimit()) {
    std::move(wait_cb).Run(WaitResult::kTopicFetchLimitReached);
  } else if (topic_fetch_in_progress_) {
    pending_wait_cbs_.push_back(std::move(wait_cb));
  } else {
    // Only other possible option is that we're backing off because of a
    // previously failed topic fetch.
    std::move(wait_cb).Run(WaitResult::kTopicFetchBackingOff);
  }
}

AmbientModeTopic AmbientTopicQueue::Pop() {
  DCHECK(!IsEmpty());
  AmbientModeTopic popped_topic = std::move(available_topics_.front());
  available_topics_.pop();
  if (available_topics_.empty())
    FetchTopics();
  return popped_topic;
}

bool AmbientTopicQueue::IsEmpty() const {
  return available_topics_.empty();
}

bool AmbientTopicQueue::HasReachedTopicFetchLimit() const {
  return total_topics_fetched_ >= topic_fetch_limit_;
}

void AmbientTopicQueue::FetchTopics() {
  if (topic_fetch_in_progress_ || HasReachedTopicFetchLimit())
    return;

  topic_fetch_in_progress_ = true;
  fetch_topic_timer_.Stop();
  pending_topic_batches_.clear();

  // Make one topic network request per topic size and wait for them all to
  // complete.
  std::vector<gfx::Size> topic_sizes = delegate_->GetTopicSizes();
  int num_topic_sizes_requested = topic_sizes.size();
  DCHECK_GT(num_topic_sizes_requested, 0);
  // Distribute the |topic_fetch_size_| equally among the different topic
  // sizes requested. For example, if |topic_fetch_size_| is 100 and there is a
  // portrait and landscape size requested, request 50 of each. Impose a floor
  // of 1 though in case there are a bunch of topic sizes requested.
  int num_topics_per_request =
      std::max(topic_fetch_size_ / num_topic_sizes_requested, 1);
  auto barrier_closure = base::BarrierClosure(
      /*num_closures=*/num_topic_sizes_requested,
      base::BindOnce(&AmbientTopicQueue::OnAllScreenUpdateInfoFetched,
                     weak_factory_.GetWeakPtr()));
  for (const gfx::Size& requested_topic_size : topic_sizes) {
    backend_controller_->FetchScreenUpdateInfo(
        num_topics_per_request,
        // If |should_split_topics_| is true, it does not make sense to ever
        // request paired personal portrait topics since they will ultimately
        // just be unpaired on the client anyways.
        /*show_pair_personal_portraits=*/!should_split_topics_,
        requested_topic_size,
        base::BindOnce(&AmbientTopicQueue::OnScreenUpdateInfoFetched,
                       weak_factory_.GetWeakPtr(), barrier_closure,
                       requested_topic_size));
  }
}

void AmbientTopicQueue::OnScreenUpdateInfoFetched(
    const base::RepeatingClosure& barrier_closure,
    const gfx::Size& requested_topic_size,
    const ash::ScreenUpdate& screen_update) {
  DCHECK(barrier_closure);
  std::vector<AmbientModeTopic> processed_topics;
  if (should_split_topics_) {
    for (const AmbientModeTopic& topic : screen_update.next_topics) {
      if (topic.related_image_url.empty()) {
        processed_topics.push_back(topic);
      } else {
        std::pair<AmbientModeTopic, AmbientModeTopic> split_topic =
            SplitTopic(topic);
        processed_topics.push_back(std::move(split_topic.first));
        processed_topics.push_back(std::move(split_topic.second));
      }
    }
  } else {
    std::vector<AmbientModeTopic> related_topics =
        CreatePairedTopics(screen_update.next_topics);
    for (AmbientModeTopic& topic : related_topics) {
      processed_topics.push_back(std::move(topic));
    }
  }

  // It is possible that |screen_update| is an empty instance if fatal errors
  // happened during the fetch or CreatePairedTopics() yielded no paired topics.
  if (processed_topics.empty()) {
    if (screen_update.next_topics.empty()) {
      LOG(WARNING)
          << "IMAX server returned screen update with no topics for size "
          << requested_topic_size.ToString();
    } else {
      LOG(WARNING) << "CreatePairedTopics() yielded no topics for size "
                   << requested_topic_size.ToString();
    }
  }

  bool inserted = pending_topic_batches_
                      .emplace(std::make_pair(requested_topic_size.width(),
                                              requested_topic_size.height()),
                               std::move(processed_topics))
                      .second;
  DCHECK(inserted) << "Duplicate topic size found: "
                   << requested_topic_size.ToString();
  barrier_closure.Run();
}

void AmbientTopicQueue::OnAllScreenUpdateInfoFetched() {
  DCHECK(topic_fetch_in_progress_);
  topic_fetch_in_progress_ = false;

  size_t total_fetched_topics = 0;
  size_t smallest_topic_batch = std::numeric_limits<size_t>::max();
  for (const auto& [_, topic_batch] : pending_topic_batches_) {
    total_fetched_topics += topic_batch.size();
    if (topic_batch.size() < smallest_topic_batch)
      smallest_topic_batch = topic_batch.size();
  }

  if (total_fetched_topics == 0) {
    LOG(ERROR) << "Topic fetch returned no topics. Retrying with backoff";
    fetch_topic_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    ScheduleFetchTopics(/*backoff=*/true);
    RunPendingWaitCallbacks(WaitResult::kTopicFetchBackingOff);
    return;
  }

  if (smallest_topic_batch > 0) {
    // Intersperse topics from each requested size in the queue so that the
    // caller sees a uniform distribution of each size when popping from it.
    for (size_t topic_idx = 0; topic_idx < smallest_topic_batch; ++topic_idx) {
      for (auto& [_, topic_batch] : pending_topic_batches_) {
        DCHECK_LT(topic_idx, topic_batch.size());
        Push(std::move(topic_batch[topic_idx]));
      }
    }
  } else {
    // In the worst case where there were no topics fetched a topic size, add
    // just one topic from all of the other topic sizes. This means there will
    // not be a completely uniform distribution in the end, but this is damage
    // control and prevents scenarios where we don't fill the queue with
    // anything because of one problematic request.
    for (auto& [_, topic_batch] : pending_topic_batches_) {
      if (!topic_batch.empty())
        Push(std::move(topic_batch.front()));
    }
  }

  fetch_topic_retry_backoff_.InformOfRequest(/*succeeded=*/true);
  ScheduleFetchTopics(/*backoff=*/false);
  RunPendingWaitCallbacks(WaitResult::kTopicsAvailable);
}

void AmbientTopicQueue::ScheduleFetchTopics(bool backoff) {
  // If retry, use the backoff delay, otherwise the default delay.
  const base::TimeDelta delay =
      backoff ? fetch_topic_retry_backoff_.GetTimeUntilRelease()
              : topic_fetch_interval_;
  fetch_topic_timer_.Start(FROM_HERE, delay,
                           base::BindOnce(&AmbientTopicQueue::FetchTopics,
                                          weak_factory_.GetWeakPtr()));
}

void AmbientTopicQueue::Push(AmbientModeTopic topic) {
  if (HasReachedTopicFetchLimit())
    return;

  available_topics_.push(std::move(topic));
  ++total_topics_fetched_;
}

void AmbientTopicQueue::RunPendingWaitCallbacks(WaitResult wait_result) {
  for (WaitCallback& wait_cb : pending_wait_cbs_) {
    // Run the callbacks asynchronously in case the callback's implementation
    // invokes WaitForTopicsAvailable() again.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(wait_cb), wait_result));
  }
  pending_wait_cbs_.clear();
}

}  // namespace ash
