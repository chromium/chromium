// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_TOPIC_QUEUE_H_
#define ASH_AMBIENT_MODEL_AMBIENT_TOPIC_QUEUE_H_

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"

namespace ash {

class AmbientBackendController;

// Maintains a queue of topics fetched from the IMAX server. Note "topic" in
// this class refers to the early stage in the pipeline where the topic only has
// a primary (and optional related) photo url. AmbientTopicQueue does not deal
// with downloading/decoding actual photos.
//
// The AmbientTopicQueue automatically fills itself. The caller only has a Pop()
// API. It fills itself with new topics from IMAX when:
// * The caller has popped all existing topics from the queue (it's empty).
// * The |topic_fetch_interval| provided in the constructor has elapsed since
//   the last topic fetch. This is done so that a sufficient amount of topics
//   are available in the queue regardless of how often the caller calls Pop().
//   (The access token required to fetch topics is only valid for a fixed period
//   after entering ambient mode).
//
// The AmbientTopicQueue stops filling itself when the total number of topics it
// has fetched exceeds the |topic_fetch_limit| provided in the constructor.
// When this limit has been reached, the queue remains empty even after the
// caller Pop()s everything from it.
class ASH_EXPORT AmbientTopicQueue {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns a non-empty set of sizes that specify the desired size of the
    // topic photos returned by IMAX. This is invoked before each topic fetch,
    // so implementations may change the set of sizes they specify if desired.
    //
    // The AmbientTopicQueue will make a best effort to maintain a uniform
    // distribution of the specified topic sizes in its queue. Note that the
    // |topic_fetch_limit| is always honored though and is a global limit for
    // all topics in the queue irrespective of size.
    virtual std::vector<gfx::Size> GetTopicSizes() = 0;
  };

  // Starts automatically filling the queue on construction. Note this class
  // intentionally does not have a method to clear the queue/reset its state.
  // For this, it's cheap to just destroy and re-create the queue.
  //
  // |topic_fetch_limit|: See class-level comments above.
  // |topic_fetch_size|: Number of topics requested from IMAX in each individual
  //                     topic fetch.
  // |topic_fetch_interval|: See class-level comments above.
  // |should_split_topics|: See |AmbientPhotoConfig.should_split_topics|.
  AmbientTopicQueue(int topic_fetch_limit,
                    int topic_fetch_size,
                    base::TimeDelta topic_fetch_interval,
                    bool should_split_topics,
                    Delegate* delegate,
                    AmbientBackendController* backend_controller);
  AmbientTopicQueue(const AmbientTopicQueue&) = delete;
  AmbientTopicQueue& operator=(const AmbientTopicQueue&) = delete;
  ~AmbientTopicQueue();

  // Invokes the |wait_cb| when the queue has topics in it (i.e. the queue is
  // not empty). If the queue already has topics in it, the |wait_cb| is invoked
  // immediately and synchronously.
  enum class WaitResult {
    // IsEmpty() is guaranteed to be false.
    kTopicsAvailable,
    // IsEmpty() will be true for the following:
    //
    // The most recent topic fetch failed, and the queue is currently backing
    // off, waiting for the IMAX server to recover. This result is returned
    // immediately so that the caller can implement any fallback logic it
    // has (ex: reading photos from an on-disc cache) rather than getting stuck
    // waiting for long periods of time.
    kTopicFetchBackingOff,
    // The |topic_fetch_limit| has been reached, so the queue will not be filled
    // with any more topics for the rest of its lifetime.
    kTopicFetchLimitReached,
  };
  using WaitCallback = base::OnceCallback<void(WaitResult)>;
  void WaitForTopicsAvailable(WaitCallback wait_cb);

  // Pops() the topic from the front of the queue. Fails with a fatal error if
  // IsEmpty().
  AmbientModeTopic Pop();

  // Whether or not there are any topics in the queue currently.
  bool IsEmpty() const;

 private:
  bool HasReachedTopicFetchLimit() const;

  void FetchTopics();
  void OnScreenUpdateInfoFetched(const base::RepeatingClosure& barrier_closure,
                                 const gfx::Size& requested_topic_size,
                                 const ash::ScreenUpdate& screen_update);
  void OnAllScreenUpdateInfoFetched();
  void ScheduleFetchTopics(bool backoff);
  void Push(AmbientModeTopic topic);
  void RunPendingWaitCallbacks(WaitResult wait_result);

  const int topic_fetch_limit_;
  const int topic_fetch_size_;
  const base::TimeDelta topic_fetch_interval_;
  const bool should_split_topics_;
  const raw_ptr<Delegate> delegate_;
  const raw_ptr<AmbientBackendController> backend_controller_;

  std::queue<AmbientModeTopic> available_topics_;
  int total_topics_fetched_ = 0;

  // Whether a topic fetch is current in progress (waiting for IMAX to respond).
  // For simplicity, there should only be one topic fetch active at any given
  // time.
  bool topic_fetch_in_progress_ = false;

  // For topic fetches scheduled in the future (either a regularly scheduled one
  // or one from backoff).
  base::OneShotTimer fetch_topic_timer_;

  // Backoff for fetch topics retries.
  net::BackoffEntry fetch_topic_retry_backoff_;

  // Accumulation of WaitForTopicsAvailable() calls made while a topic fetch
  // is currently in progress. These are flushed/run when the topic fetch
  // completes.
  std::vector<WaitCallback> pending_wait_cbs_;

  // Transient storage for the IMAX responses returned from each topic fetch.
  // There is one entry per topic size requested since there is also one network
  // request made per topic size.
  //
  // The key contains the requested topic size's width/height. Using a map
  // ensures a *deterministic* ordering of the topic sizes when processing this
  // map after each fetch, but the order itself is irrelevant.
  base::flat_map<std::pair<int, int>, std::vector<AmbientModeTopic>>
      pending_topic_batches_;

  base::WeakPtrFactory<AmbientTopicQueue> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_TOPIC_QUEUE_H_
