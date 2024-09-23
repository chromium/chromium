// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/util/mrfu_cache.h"

#include <cmath>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"

namespace app_list {
namespace {

constexpr int kVersion = 2;

// We boost scores with the equation
//
//   score = score + (1 - score) * k
//
// where k is a boost coefficient. This is hard to reason about, so instead
// set a 'boost factor', which is the answer to question: "how many consecutive
// uses should it take for a score to start at 0 and reach 0.8?". We can then
// define k based on the boost factor. Note 0.8 is chosen arbitrarily, but
// it's a reasonably high score.
//
// Here's our terminology:
//  - x is the score
//  - f(x) is one decay followed by one boost
//  - f_n(x) is f applied n times to x
//  - k is the boost coefficient
//  - D is the decay coefficient
//  - N is the boost factor
//
// We want an equation for the score after using a new item N times - ie.
// f_N(0) - and then solve for k.
//
//    f(x) = Dx + (1-Dx)k
//         = k + (1-k)Dx
//
//  f_n(x) = (1-k)^n D^n x + k sum((1-k)^i D^i, 0 <= i < n)
//
//  f_N(0) = k sum( (1-k)^i D^i, 0 <= i < N)
//         = k (1 - D^N (1-k)^N) / (1 + D(k-1))              identity on sum
//
// Therefore we're looking for the value of k that satisfies
//
//   k (1 - D^N (1-k)^N) / (1 + D(k-1)) = 0.8
//
// which isn't easily solvable, so this function approximates it numerically.
float ApproximateBoostCoefficient(float decay_coefficient, float boost_factor) {
  float D = decay_coefficient;
  float N = boost_factor;
  float target = 0.8f;

  float k_min = 0.0f;
  float k_max = 1.0f;
  for (int i = 0; i < 10; ++i) {
    float k = (k_min + k_max) / 2.0f;
    float value = k * (1 - pow(D, N) * pow(1 - k, N)) / (1 + D * k - D);
    if (value < target) {
      k_min = k;
    } else {
      k_max = k;
    }
  }

  return (k_min + k_max) / 2.0f;
}

}  // namespace

MrfuCache::MrfuCache(MrfuCache::Proto proto, const Params& params)
    : proto_(std::move(proto)) {
  // `proto_` is a class member so it is safe to call `RegisterOnInitUnsafe()`.
  proto_.RegisterOnInitUnsafe(
      base::BindOnce(&MrfuCache::OnProtoInit, base::Unretained(this)));

  proto_.Init();
  // See header comment for explanation.
  decay_coeff_ = exp(log(0.5f) / params.half_life);
  boost_coeff_ = ApproximateBoostCoefficient(decay_coeff_, params.boost_factor);
  max_items_ = params.max_items;
  min_score_ = params.min_score;
}

MrfuCache::~MrfuCache() {}

void MrfuCache::Sort(Items& items) {
  std::sort(items.begin(), items.end(),
            [](auto const& a, auto const& b) { return a.second > b.second; });
}

void MrfuCache::Use(const std::string& item) {
  if (!proto_.initialized())
    return;

  // Get the Score for |item| from the proto. If it doesn't exist, create an
  // empty score.
  Score* score;
  auto* items = proto_->mutable_items();
  const auto& it = items->find(item);
  if (it != items->end()) {
    score = &it->second;
  } else {
    auto ret = items->insert({item, Score()});
    DCHECK(ret.second);
    score = &ret.first->second;
  }

  // The order of these three steps is important: first move 'time' forward one
  // step, then decay the score, then add the boost for the current use.
  proto_->set_update_count(proto_->update_count() + 1);
  Decay(score);

  float boost = boost_coeff_ * (1.0f - score->score());
  score->set_score(score->score() + boost);
  proto_->set_total_score(proto_->total_score() + boost);

  MaybeCleanup();
  proto_.QueueWrite();
}

float MrfuCache::Get(const std::string& item) {
  if (!proto_.initialized())
    return 0.0f;

  auto* items = proto_->mutable_items();
  const auto& it = items->find(item);
  if (it == items->end())
    return 0.0f;

  // |score| may not be current, so |Decay| it if needed.
  Score* score = &it->second;
  Decay(score);

  return score->score();
}

float MrfuCache::GetNormalized(const std::string& item) {
  if (!proto_.initialized() || proto_->total_score() == 0.0f)
    return 0.0f;
  return Get(item) / proto_->total_score();
}

MrfuCache::Items MrfuCache::GetAll() {
  if (!proto_.initialized())
    return {};

  MrfuCache::Items results;
  for (auto& item_score : *proto_->mutable_items()) {
    Score& score = item_score.second;
    Decay(&score);
    results.emplace_back(item_score.first, score.score());
  }
  return results;
}

MrfuCache::Items MrfuCache::GetAllNormalized() {
  if (!proto_.initialized() || proto_->total_score() == 0.0f)
    return {};

  auto results = GetAll();
  const float total = proto_->total_score();
  for (auto& pair : results)
    pair.second /= total;
  return results;
}

void MrfuCache::Delete(const std::string& item) {
  if (!proto_.initialized())
    return;
  proto_->set_total_score(proto_->total_score() - Get(item));
  proto_->mutable_items()->erase(item);
  proto_.QueueWrite();
}

void MrfuCache::ResetWithItems(const Items& items) {
  DCHECK(proto_.initialized());
  proto_->Clear();

  proto_->set_update_count(0);
  float total_score = 0.0f;
  auto* proto_items = proto_->mutable_items();
  for (const auto& item_score : items) {
    Score score;
    score.set_score(item_score.second);
    score.set_last_update_count(0);
    proto_items->insert({item_score.first, score});
    total_score += item_score.second;
  }
  proto_->set_total_score(total_score);
  proto_.QueueWrite();
}

void MrfuCache::Decay(Score* score) {
  int64_t update_count = proto_->update_count();
  int64_t count_delta = update_count - score->last_update_count();
  if (count_delta > 0) {
    float decay = std::pow(decay_coeff_, count_delta);
    proto_->set_total_score(proto_->total_score() +
                            (decay - 1.0f) * score->score());
    score->set_score(score->score() * decay);
    score->set_last_update_count(update_count);
    proto_.QueueWrite();
  }
}

void MrfuCache::MaybeCleanup() {
  if (base::checked_cast<size_t>(proto_->items_size()) < 2u * max_items_)
    return;

  // Ensure all scores are up to date, and then keep all those over the
  // |min_score_| threshold.
  std::vector<std::pair<std::string, Score>> kept_items;
  for (auto& item_score : *proto_->mutable_items()) {
    Score& score = item_score.second;
    Decay(&score);
    if (score.score() > min_score_)
      kept_items.emplace_back(item_score.first, item_score.second);
  }

  // Sort them high-to-low by score.
  std::sort(kept_items.begin(), kept_items.end(),
            [](auto const& a, auto const& b) {
              return a.second.score() > b.second.score();
            });

  // Clear the proto and reinsert at most |max_items_| items.
  float new_total = 0.0f;
  proto_->clear_items();
  for (size_t i = 0; i < std::min(max_items_, kept_items.size()); ++i) {
    proto_->mutable_items()->insert(
        {kept_items[i].first, kept_items[i].second});
    new_total += kept_items[i].second.score();
  }
  proto_->set_total_score(new_total);
  proto_.QueueWrite();
}

void MrfuCache::OnProtoInit() {
  if (!proto_->has_version() || proto_->version() != kVersion) {
    proto_.Purge();
  }

  proto_->set_version(kVersion);
}

}  // namespace app_list
