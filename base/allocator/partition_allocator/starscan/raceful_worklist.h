// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_RACEFUL_WORKLIST_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_RACEFUL_WORKLIST_H_

#include <atomic>
#include <vector>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/starscan/metadata_allocator.h"
#include "base/compiler_specific.h"
#include "base/rand_util.h"

namespace base {
namespace internal {

template <typename T>
class RacefulWorklist {
  struct Node {
    explicit Node(const T& value) : value(value) {}
    Node(const Node& other)
        : value(other.value),
          is_being_visited(
              other.is_being_visited.load(std::memory_order_relaxed)),
          is_visited(other.is_visited.load(std::memory_order_relaxed)) {}

    T value;
    std::atomic<bool> is_being_visited{false};
    std::atomic<bool> is_visited{false};
  };
  using Underlying = std::vector<Node, MetadataAllocator<Node>>;

 public:
  class RandomizedView {
   public:
    explicit RandomizedView(RacefulWorklist& worklist)
        : worklist_(worklist), offset_(0) {
      if (worklist.data_.size() > 0)
        offset_ =
            static_cast<size_t>(base::RandGenerator(worklist.data_.size()));
    }

    RandomizedView(const RandomizedView&) = delete;
    const RandomizedView& operator=(const RandomizedView&) = delete;

    template <typename Function>
    void Visit(Function f);

   private:
    RacefulWorklist& worklist_;
    size_t offset_;
  };

  RacefulWorklist() = default;

  RacefulWorklist(const RacefulWorklist&) = delete;
  RacefulWorklist& operator=(const RacefulWorklist&) = delete;

  void Push(const T& t) { data_.push_back(Node(t)); }

  template <typename It>
  void Push(It begin, It end) {
    std::transform(begin, end, std::back_inserter(data_),
                   [](const T& t) { return Node(t); });
  }

  template <typename Function>
  void VisitNonConcurrently(Function) const;

 private:
  Underlying data_;
  std::atomic<bool> fully_visited_{false};
};

template <typename T>
template <typename Function>
void RacefulWorklist<T>::VisitNonConcurrently(Function f) const {
  for (const auto& t : data_)
    f(t.value);
}

template <typename T>
template <typename Function>
void RacefulWorklist<T>::RandomizedView::Visit(Function f) {
  auto& data = worklist_.data_;
  std::vector<typename Underlying::iterator,
              MetadataAllocator<typename Underlying::iterator>>
      to_revisit;

  // To avoid worklist iteration, quick check if the worklist was already
  // visited.
  if (worklist_.fully_visited_.load(std::memory_order_acquire))
    return;

  const auto offset_it = std::next(data.begin(), offset_);

  // First, visit items starting from the offset.
  for (auto it = offset_it; it != data.end(); ++it) {
    if (it->is_visited.load(std::memory_order_relaxed))
      continue;
    if (it->is_being_visited.load(std::memory_order_relaxed)) {
      to_revisit.push_back(it);
      continue;
    }
    it->is_being_visited.store(true, std::memory_order_relaxed);
    f(it->value);
    it->is_visited.store(true, std::memory_order_relaxed);
  }

  // Then, visit items before the offset.
  for (auto it = data.begin(); it != offset_it; ++it) {
    if (it->is_visited.load(std::memory_order_relaxed))
      continue;
    if (it->is_being_visited.load(std::memory_order_relaxed)) {
      to_revisit.push_back(it);
      continue;
    }
    it->is_being_visited.store(true, std::memory_order_relaxed);
    f(it->value);
    it->is_visited.store(true, std::memory_order_relaxed);
  }

  // Finally, racefully visit items that were scanned by some other thread.
  for (auto it : to_revisit) {
    if (LIKELY(it->is_visited.load(std::memory_order_relaxed)))
      continue;
    // Don't bail out here if the item is being visited by another thread.
    // This is helpful to guarantee forward progress if the other thread
    // is making slow progress.
    it->is_being_visited.store(true, std::memory_order_relaxed);
    f(it->value);
    it->is_visited.store(true, std::memory_order_relaxed);
  }

  worklist_.fully_visited_.store(true, std::memory_order_release);
}

}  // namespace internal
}  // namespace base
#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_RACEFUL_WORKLIST_H_
