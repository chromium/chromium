// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_MRFU_CACHE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_MRFU_CACHE_H_

#include <map>
#include <string>

#include "ash/utility/persistent_proto.h"
#include "chrome/browser/ash/app_list/search/util/mrfu_cache.pb.h"

namespace app_list {

namespace test {
class MrfuCacheTest;
}

// The most-recently-frequently-used cache stores a mapping from strings -
// called items - to scores that is persisted to disk. The cache has two main
// operations: |Use| boosts the score of an item, and |Get| returns the score
// of an item.
//
// THEORY
//
// - Every item in the MRFU cache has a score, which is *boosted* on use and
//   *decays* as other items are used.
//
// - We don't want item scores to grow unbounded, so we boost scores on use
//   with
//
//     item.score = item.score + (1 - item.score) * boost_coeff
//
//   where boost_coeff is in (0,1). This keeps scores in [0,1).
//
//   boost_coeff is complicated to reason about, so instead we define a
//   boost_factor parameter, which is the number of consecutive uses a new item
//   needs to get a score of 0.8. boost_coeff is then calculated based on this.
//
// - Each time an item is used, the scores of all items decay. This is done by
//
//     item.score = item.score * decay_coeff
//     decay_coeff := exp( ln(0.5) / half_life )
//
//   decay_coeff is defined so that an item's score will decay by half after
//   half_life further uses of other items.
//
// IMPLEMENTATION
//
// We can optimize the theory by only decaying items on-the-fly. Track two
// numbers per item: its score, and the last 'time' we updated it. 'Time' is a
// counter that increments on each call to |Use|. We can apply many iterations
// of decay at once like so:
//
//  def decay(item):
//    decay = exp(decay_coeff, current_time - item.last_time)
//    item.score = item.score * decay
//    item.last_time = current_time
//
// This leads to a simple implementation of our two operations:
//
//  def use(item):
//    current_time += 1
//    decay(item)
//    item.score = item.score + (1 - item.score) * boost_factor
//
//  def get(item):
//    item.score = item.score * decay(item)
class MrfuCache {
 public:
  // All user-settable parameters of the MRFU cache. The struct has some
  // reasonable defaults, but this should be customized per use-case.
  struct Params {
    // How many uses of other items before a score decays by half.
    float half_life = 10.0f;
    // How many consecutive uses of an item it takes to reach a score of 0.8.
    float boost_factor = 5.0f;
    // A soft limit on the number of items stored.
    size_t max_items = 50;
    // Items below min_score may be deleted.
    float min_score = 0.01f;
  };

  // A vector of items and their scores. No guarantees of ordering.
  using Items = std::vector<std::pair<std::string, float>>;
  using Proto = ash::PersistentProto<MrfuCacheProto>;

  MrfuCache(MrfuCache::Proto proto, const Params& params);
  ~MrfuCache();

  MrfuCache(const MrfuCache&) = delete;
  MrfuCache& operator=(const MrfuCache&) = delete;

  // Sort |items| high-to-low according to their scores.
  static void Sort(Items& items);

  // Records the use of |item|, increasing its score and decaying other scores.
  void Use(const std::string& item);

  // Returns the score of |item|.
  float Get(const std::string& item);

  // Returns the score of |item| divided by the sum of all scores.
  float GetNormalized(const std::string& item);

  // Returns all items in the cache and their scores.
  Items GetAll();

  // Returns all items in the cache and their scores, normalized by the sum of
  // all scores.
  Items GetAllNormalized();

  // Removes |item| from the cache. This is best-effort: if the proto is not
  // initialized then |item| is not deleted.
  void Delete(const std::string& item);

  // Clears the current content of the cache and replaces it with the given
  // items and scores. It is invalid to call this before the cache is
  // initialized.
  void ResetWithItems(const Items& items);

  // Whether the cache has finished reading from disk.
  bool initialized() const { return proto_.initialized(); }

  // The number of items in the cache. Returns 0 if the cache hasn't finished
  // initializing.
  size_t size() const {
    if (!initialized())
      return 0u;
    return proto_->items_size();
  }

  // Whether the cache has no items stored. Returns true if the cache hasn't
  // finished initializing.
  bool empty() const { return size() == 0; }

 private:
  friend class test::MrfuCacheTest;

  using Score = MrfuCacheProto::Score;

  void Decay(Score* score);
  void MaybeCleanup();
  void OnProtoInit();

  ash::PersistentProto<MrfuCacheProto> proto_;

  float decay_coeff_;
  float boost_coeff_;
  size_t max_items_;
  float min_score_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_UTIL_MRFU_CACHE_H_
